#include "TeamCarryGameMode.h"
#include "TeamCarryGameState.h"
#include "Kismet/GameplayStatics.h"
#include "TCSaveGame.h"
#include "Furniture/TCFurnitureActor.h"
#include "CatchCharacter/Furniture/FurnitureStat.h"
#include "CatchCharacter/Furniture/FurnitureActor.h"
#include "CatchCharacter/Furniture/FurnitureDamage.h"
#include "GameFramework/PlayerState.h"
#include "Network/Session/TCGameInstance.h"
#include "Network/Session/TCSessionFlow.h"
#include "Player/PlayerState/TCPlayerState.h"
#include "Player/PlayerController/TCPlayerController.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// 디버그: 제한시간 타이머 정지. 콘솔 "TC.TimerPause 1" 또는 "TC.TimerPauseToggle"(F10 바인딩).
	// 타이머는 서버(GameMode)에서만 차감되므로 호스트 쪽에서 켜야 적용된다.
	TAutoConsoleVariable<int32> CVarTimerPause(
		TEXT("TC.TimerPause"), 0,
		TEXT("디버그: 게임 제한시간 타이머 정지 (0=진행, 1=정지)"));
	FAutoConsoleCommand CmdTimerPauseToggle(
		TEXT("TC.TimerPauseToggle"),
		TEXT("디버그: 제한시간 타이머 정지 토글 (F10)"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const int32 NewVal = CVarTimerPause.GetValueOnGameThread() ? 0 : 1;
			CVarTimerPause->Set(NewVal, ECVF_SetByConsole);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(9102, 2.f, FColor::Yellow,
					FString::Printf(TEXT("[타이머 디버그] %s"), NewVal ? TEXT("정지") : TEXT("재개")));
			}
		}));
}

ATeamCarryGameMode::ATeamCarryGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    TotalFurnitureCount = 0;
    AccumulatedScore = 0;

    // Seamless Travel(ATCLobbyGameMode::ATCLobbyGameMode() 와 동일 이유 — 리슨 소켓 유지,
    // SteamSockets 재바인딩 실패 방지). 이 값이 비어 있으면(기본값 false) 로비→스테이지 트래블이
    // hard travel로 실행되어 LoadMap 이 게임 스레드를 수 초간 동기 블로킹하고, 그동안 렌더링이
    // 멈춰 로딩 게이지 화면이 검은 화면(+로그)처럼 보이는 문제가 있었다. HandleSeamlessTravelPlayer()/
    // PostLogin() 은 이미 이 경로(PostLogin 을 안 타는 seamless 합류)를 전제로 로딩 게이트를
    // 리셋하도록 구현되어 있었으므로, 실제로는 이 플래그만 누락되어 있었다.
    bUseSeamlessTravel = true;

    // GameState 클래스 설정
    GameStateClass = ATeamCarryGameState::StaticClass();
}

ATeamCarryGameState* ATeamCarryGameMode::GetCachedGameState()
{
    if (!CachedGameState)
    {
        CachedGameState = GetGameState<ATeamCarryGameState>();
    }
    return CachedGameState;
}

void ATeamCarryGameMode::BeginPlay()
{
    Super::BeginPlay();

    TArray<AActor*> FurnitureActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATCFurnitureActor::StaticClass(), FurnitureActors);
    SetTotalFurnitureCount(FurnitureActors.Num());

    ATeamCarryGameState* GS = GetCachedGameState();
    if (GS)
    {
        // 스톱워치 초기화
        GS->ElapsedTime = 0.0f;

        // 팀 값어치 게이지의 Max 값(전체 목표 값어치)을 스테이지 시작 시 1회 복제한다. 스테이지 중 불변.
        GS->TotalLevelValue = TotalLevelValue;
    }

    // 전원 로딩 완료 대기(로딩 화면 동기화 수정): 즉시 카운트다운을 시작하지 않고, 접속 중인 모든
    // 플레이어가 ServerReportMapLoaded() 로 로딩 완료를 보고할 때까지 WaitingToStart 로 대기한다.
    // 일부 클라이언트가 응답 없이 멈추는 경우를 대비해 타임아웃 세이프티 타이머를 건다.
    SetGamePhase(EGamePhase::WaitingToStart);

    TWeakObjectPtr<ATeamCarryGameMode> WeakThis = this;
    GetWorldTimerManager().SetTimer(LoadingGateTimeoutHandle, [WeakThis]()
        {
            if (!WeakThis.IsValid()) return;

            ATeamCarryGameState* GS = WeakThis->GetCachedGameState();
            if (!GS || GS->CurrentPhase != EGamePhase::WaitingToStart)
            {
                // 이미 전원 로딩 완료 경로로 진행됨 — 타임아웃은 그대로 무시.
                return;
            }

            UE_LOG(LogTemp, Warning, TEXT("전원 로딩 완료 대기 타임아웃 — 강제로 게임을 시작합니다."));
            WeakThis->StartCountdown();

            // 응답 없는 플레이어가 있을 수 있으므로, 접속 중인 모든 PC에 강제로 InGame 진입을 지시한다.
            if (UWorld* World = WeakThis->GetWorld())
            {
                for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
                {
                    if (ATCPlayerController* PC = Cast<ATCPlayerController>(It->Get()))
                    {
                        PC->ClientNotifyAllPlayersLoaded();
                    }
                }
            }
        }, 20.0f, false);
}

void ATeamCarryGameMode::NotifyPlayerFinishedLoading(APlayerController* PC)
{
    ATCPlayerState* PS = PC ? PC->GetPlayerState<ATCPlayerState>() : nullptr;
    if (!PS)
    {
        return;
    }
    PS->SetHasLoadedCurrentMapAuthoritative(true);

    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS)
    {
        return;
    }

    if (GS->CurrentPhase == EGamePhase::WaitingToStart)
    {
        // 정상 동시 시작: 전원 로딩 완료 시에만 카운트다운을 시작하고, 전원에게 InGame 진입을 지시한다.
        if (!AreAllConnectedPlayersLoaded())
        {
            return;
        }

        GetWorldTimerManager().ClearTimer(LoadingGateTimeoutHandle);
        StartCountdown();

        if (UWorld* World = GetWorld())
        {
            for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
            {
                if (ATCPlayerController* EachPC = Cast<ATCPlayerController>(It->Get()))
                {
                    EachPC->ClientNotifyAllPlayersLoaded();
                }
            }
        }
    }
    else
    {
        // 재접속/후발 합류(이미 Playing 이후 단계): 전체 게이트를 기다리지 않고 그 플레이어
        // 한 명에게만 즉시 InGame 진입을 지시한다(기존 재접속 로직과 충돌하지 않도록).
        if (ATCPlayerController* JoiningPC = Cast<ATCPlayerController>(PC))
        {
            JoiningPC->ClientNotifyAllPlayersLoaded();
        }
    }
}

bool ATeamCarryGameMode::AreAllConnectedPlayersLoaded() const
{
    const ATeamCarryGameState* GS = CachedGameState;
    if (!GS)
    {
        return false;
    }
    int32 Counted = 0;
    for (APlayerState* PS : GS->PlayerArray)
    {
        const ATCPlayerState* TCPS = Cast<ATCPlayerState>(PS);
        if (!TCPS)
        {
            continue;
        }
        ++Counted;
        if (!TCPS->HasLoadedCurrentMap())
        {
            return false;
        }
    }
    return Counted > 0;
}

void ATeamCarryGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS || GS->bIsGameFinished) return;
    
    // Playing 단계일 때만 스톱워치 작동 (디버그 정지 CVar가 켜져 있으면 보류)
    if (GS->CurrentPhase == EGamePhase::Playing && CVarTimerPause.GetValueOnGameThread() == 0)
    {
        // 경과 시간 증가 (시간 제한 없음 — 모든 가구가 트럭에 들어오면 게임 종료)
        GS->ElapsedTime += DeltaTime;
    }

    // 트럭 안 가구 내구도 변화 감지 — 변화 시 즉시 점수 갱신
    if (GS->CurrentPhase == EGamePhase::Playing && FurnitureInTruck.Num() > 0)
    {
        bool bScoreChanged = false;
        int32 NewAccumulatedScore = 0;
        for (FTruckFurnitureInfo& Info : FurnitureInTruck)
        {
            if (Info.FurnitureActor.IsValid() && Info.ActorUniqueID != 0)
            {
                // FurnitureStat 컴포넌트에서 현재 내구도 읽기
                if (UFurnitureStat* Stat = Info.FurnitureActor->FindComponentByClass<UFurnitureStat>())
                {
                    const float NewHealth = Stat->GetCurrentHealth();
                    if (!FMath::IsNearlyEqual(NewHealth, Info.CurrentHealth))
                    {
                        Info.CurrentHealth = NewHealth;
                        bScoreChanged = true;
                    }
                }
            }
            NewAccumulatedScore += CalculateScore(Info.CurrentHealth, Info.MaxHealth, Info.BaseScore);
        }
        if (bScoreChanged)
        {
            AccumulatedScore = NewAccumulatedScore;
            GS->TotalScore = AccumulatedScore;
            GS->OnRep_TotalScore();
        }
    }
}

void ATeamCarryGameMode::SetTotalFurnitureCount(int32 Count)
{
    TotalFurnitureCount = Count;

    ATeamCarryGameState* GS = GetCachedGameState();
    if (GS)
    {
        GS->RemainingFurniture = Count;

        // 전체 상자 개수(파괴된 것 포함)의 고정 분모. 이 함수는 BeginPlay에서 1회만 호출되므로
        // RemainingFurniture와 동시에 설정된 이 시점의 Count가 곧 전체 개수다(UI_Technical_Spec.md 4장-7).
        GS->TotalFurnitureCount = Count;

        // 리슨 서버 호스트는 자기 자신에게 OnRep이 트리거되지 않으므로 수동 호출로 UI를 즉시 갱신한다.
        GS->OnRep_RemainingFurniture();
    }
}

void ATeamCarryGameMode::Logout(AController* Exiting)
{
    ATeamCarryGameState* GS = GetCachedGameState();
    if (GS && GS->CurrentPhase == EGamePhase::Playing)
    {
        if (APlayerController* PC = Cast<APlayerController>(Exiting))
        {
            if (PC->PlayerState && PC->PlayerState->GetUniqueId().IsValid())
            {
                DisconnectedPlayerIds.Add(PC->PlayerState->GetUniqueId());

                // 튕긴 플레이어가 있으므로 재접속 허용
                if (UTCGameInstance* GI = Cast<UTCGameInstance>(GetGameInstance()))
                {
                    GI->SetAllowJoinInProgress(true);
                    UE_LOG(LogTemp, Warning, TEXT("플레이어 이탈 | 재접속 허용 (bAllowJoinInProgress = true)"));
                }
            }
        }
    }

    Super::Logout(Exiting);

    int32 PlayerCount = GetNumPlayers();
    UE_LOG(LogTemp, Warning, TEXT("플레이어 이탈 | 남은 플레이어: %d"), PlayerCount - 1);

    // 전원 로딩 대기 중 한 명이 나가서, 남은 인원이 이미 전원 로딩 완료 상태가 되는 경우 대비
    // (로딩 화면 동기화 수정). GS 는 위에서 이미 조회했다.
    if (GS && GS->CurrentPhase == EGamePhase::WaitingToStart && AreAllConnectedPlayersLoaded())
    {
        GetWorldTimerManager().ClearTimer(LoadingGateTimeoutHandle);
        StartCountdown();

        if (UWorld* World = GetWorld())
        {
            for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
            {
                if (ATCPlayerController* EachPC = Cast<ATCPlayerController>(It->Get()))
                {
                    EachPC->ClientNotifyAllPlayersLoaded();
                }
            }
        }
    }

    // 월드가 종료 중이면 FinishGame 호출 안 함
    if (GetWorld() && !GetWorld()->bIsTearingDown && PlayerCount <= 1)
    {
        FinishGame(false);
    }
}

void ATeamCarryGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    // 스테이지 맵 진입 전원 대기 게이트 리셋(로딩 화면 동기화 수정). Seamless Travel 로 도착하는
    // 경우(로비→스테이지)는 PostLogin 이 호출되지 않으므로 HandleSeamlessTravelPlayer 가 대신 처리한다.
    if (ATCPlayerState* NewPS = NewPlayer ? NewPlayer->GetPlayerState<ATCPlayerState>() : nullptr)
    {
        NewPS->SetHasLoadedCurrentMapAuthoritative(false);

        // 접속 순서대로 번호 부여
        NewPS->SetColorIndexAuthoritative(ColorIndexCounter);
        ColorIndexCounter++;
    }

    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS) return;

    // 스테이지 진행 중일 때만 재접속 여부 확인
    if (GS->CurrentPhase == EGamePhase::Playing)
    {
        // 로비가 아닌 경우 (스테이지 진행 중)
        if (NewPlayer->PlayerState && NewPlayer->PlayerState->GetUniqueId().IsValid())
        {
            FUniqueNetIdRepl NewPlayerId = NewPlayer->PlayerState->GetUniqueId();
            bool bIsReconnecting = DisconnectedPlayerIds.Contains(NewPlayerId);

            if (bIsReconnecting)
            {
                // 튕긴 플레이어 → 재접속 허용 후 다시 참여 차단
                DisconnectedPlayerIds.Remove(NewPlayerId);
                UE_LOG(LogTemp, Warning, TEXT("플레이어 재접속 | 게임 진행 중 복귀"));

                // 대기 중인 튕긴 플레이어가 없으면 다시 차단
                if (DisconnectedPlayerIds.Num() == 0)
                {
                    if (UTCGameInstance* GI = Cast<UTCGameInstance>(GetGameInstance()))
                    {
                        GI->SetAllowJoinInProgress(false);
                        UE_LOG(LogTemp, Warning, TEXT("재접속 완료 | 참여 차단 (bAllowJoinInProgress = false)"));
                    }
                }
            }
        }
    }
}

void ATeamCarryGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
    Super::HandleSeamlessTravelPlayer(C);

    // 로비→스테이지처럼 Seamless Travel 로 도착하는 플레이어는 PostLogin 을 타지 않으므로
    // 여기서 로딩 완료 게이트를 리셋한다(ATCLobbyGameMode::HandleSeamlessTravelPlayer 와 동일 패턴).
    if (APlayerController* PC = Cast<APlayerController>(C))
    {
        if (ATCPlayerState* PS = PC->GetPlayerState<ATCPlayerState>())
        {
            PS->SetHasLoadedCurrentMapAuthoritative(false);

            // 접속 순서대로 번호 부여
            PS->SetColorIndexAuthoritative(ColorIndexCounter);
            ColorIndexCounter++;
        }
    }
}

void ATeamCarryGameMode::SaveGame(const FString& StageName)
{
    SaveGameToSlotInternal(StageName, GetActiveSaveSlotName());
}

void ATeamCarryGameMode::SaveGameToSlot(const FString& SlotName)
{
    // 이후 이 세션의 RestartStage/자동저장(FinishGame)도 같은 슬롯을 계속 쓰도록 활성 슬롯을 갱신한다.
    if (UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr)
    {
        Flow->SetSaveSelection(SlotName, true);
    }

    const FString StageName = GetWorld() ? GetWorld()->GetMapName() : FString();
    SaveGameToSlotInternal(StageName, SlotName);
}

void ATeamCarryGameMode::SaveGameToSlotInternal(const FString& StageName, const FString& SlotName)
{
    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS) return;

    // 지정한 슬롯의 기존 세이브를 불러온다(없으면 새로 생성) — GetActiveSaveSlotName() 이 아니라
    // SlotName 을 직접 조회해야, SaveGameToSlot() 이 활성 슬롯과 다른 슬롯을 대상으로 할 때도
    // 그 슬롯 자신의 기존 데이터를 정확히 불러온다.
    UTCSaveGame* SaveData = UGameplayStatics::DoesSaveGameExist(SlotName, 0)
        ? Cast<UTCSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0))
        : nullptr;
    if (!SaveData)
    {
        SaveData = Cast<UTCSaveGame>(UGameplayStatics::CreateSaveGameObject(UTCSaveGame::StaticClass()));
    }

    // 스테이지 기록 갱신
    FStageRecord& Record = SaveData->StageRecords.FindOrAdd(StageName);
    Record.bIsCleared = true;
    Record.BestStar = FMath::Max(Record.BestStar, GS->StarCount);
    Record.BestScore = FMath::Max(Record.BestScore, GS->TotalScore);
    SaveData->LastPlayedStage = StageName;

    // 저장
    UGameplayStatics::SaveGameToSlot(SaveData, SlotName, 0);

    UE_LOG(LogTemp, Warning, TEXT("게임 저장 완료 | 슬롯: %s | 스테이지: %s | 별: %d | 점수: %d"),
        *SlotName, *StageName, Record.BestStar, Record.BestScore);
}

UTCSaveGame* ATeamCarryGameMode::LoadGame()
{
    const FString SlotName = GetActiveSaveSlotName();
    if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
    {
        return Cast<UTCSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    }
    return nullptr;
}

FString ATeamCarryGameMode::GetActiveSaveSlotName() const
{
    if (const UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr)
    {
        const FString Selected = Flow->GetSelectedSlotName();
        if (!Selected.IsEmpty())
        {
            return Selected;
        }
    }
    return TEXT("TCGameSave"); // 슬롯 미선택(구버전 호환) 시 폴백
}

void ATeamCarryGameMode::SetGamePhase(EGamePhase NewPhase)
{
    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS) return;

    GS->CurrentPhase = NewPhase;

    // 리슨 서버 호스트는 자기 자신에게 OnRep이 트리거되지 않으므로 수동 호출로 UI를 즉시 갱신한다.
    GS->OnRep_CurrentPhase();

    // Playing 단계 전환 시 새 플레이어 참여 차단 + UI 초기값 갱신
    if (NewPhase == EGamePhase::Playing)
    {
        if (UTCGameInstance* GI = Cast<UTCGameInstance>(GetGameInstance()))
        {
            GI->SetAllowJoinInProgress(false);
            UE_LOG(LogTemp, Warning, TEXT("스테이지 시작 | 참여 차단 (bAllowJoinInProgress = false)"));
        }

        GS->OnRep_RemainingFurniture();
        GS->OnRep_TotalScore();
    }

    // Result 단계 전환 시 다시 참여 허용
    if (NewPhase == EGamePhase::Result)
    {
        if (UTCGameInstance* GI = Cast<UTCGameInstance>(GetGameInstance()))
        {
            GI->SetAllowJoinInProgress(true);
            UE_LOG(LogTemp, Warning, TEXT("스테이지 종료 | 참여 허용 (bAllowJoinInProgress = true)"));
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("게임 단계 전환: %d"), (int32)NewPhase);
}

void ATeamCarryGameMode::StartCountdown()
{
    SetGamePhase(EGamePhase::Countdown);
    CountdownTime = 3.0f;

    // this 원시 포인터 대신 약한 참조(Weak Pointer)를 생성합니다.
    TWeakObjectPtr<ATeamCarryGameMode> WeakThis = this;

    GetWorldTimerManager().SetTimer(CountdownTimerHandle, [WeakThis]()
    {
        // 타이머가 실행되는 순간, 게임 모드가 이미 파괴되었다면 즉시 실행을 취소하여 크래시를 방지합니다.
        if (!WeakThis.IsValid()) return;

        WeakThis->CountdownTime -= 1.0f;
        UE_LOG(LogTemp, Warning, TEXT("카운트다운: %.0f"), WeakThis->CountdownTime);

        if (WeakThis->CountdownTime <= 0.0f)
        {
            // 타이머를 해제하고 게임 상태를 변경합니다.
            WeakThis->GetWorldTimerManager().ClearTimer(WeakThis->CountdownTimerHandle);
            WeakThis->SetGamePhase(EGamePhase::Playing);
        }
    }, 1.0f, true);
}

void ATeamCarryGameMode::OnFurnitureEnterTruck(AActor* FurnitureActor, float CurrentHealth, float MaxHealth, int32 BaseScore)
{
    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS || GS->bIsGameFinished) return;

    // 트럭 안 가구 목록에 추가 — 액터 포인터 + UniqueID 로 개별 추적
    FTruckFurnitureInfo Info;
    Info.FurnitureActor = FurnitureActor;
    Info.ActorUniqueID = FurnitureActor ? FurnitureActor->GetUniqueID() : 0;
    Info.CurrentHealth = CurrentHealth;
    Info.MaxHealth = MaxHealth;
    Info.BaseScore = BaseScore;
    FurnitureInTruck.Add(Info);

    // 진입 시 현재 내구도로 점수 추가
    AccumulatedScore += CalculateScore(CurrentHealth, MaxHealth, BaseScore);

    // 남은 가구 차감
    GS->RemainingFurniture--;
    GS->OnRep_RemainingFurniture();

    GS->TotalScore = AccumulatedScore;
    GS->OnRep_TotalScore();

    UE_LOG(LogTemp, Warning, TEXT("가구 트럭 진입 | 예상 점수: %d | 남은 가구: %d"),
        GS->TotalScore, GS->RemainingFurniture);

    // 3초 후 가구 무적 설정 — DamageSystem 이 BeginPlay 이후 유효해지므로 딜레이 적용.
    // 타이머 핸들을 멤버 맵에 보관해 로컬 변수 소멸로 타이머가 취소되는 문제를 방지한다.
    if (AFurnitureActor* Furniture = Cast<AFurnitureActor>(FurnitureActor))
    {
        TWeakObjectPtr<AFurnitureActor> WeakFurniture = Furniture;
        TWeakObjectPtr<ATeamCarryGameMode> WeakThis = this;
        FTimerHandle& Handle = InvincibleTimerHandles.FindOrAdd(FurnitureActor);
        GetWorldTimerManager().SetTimer(Handle, [WeakFurniture, WeakThis]()
        {
            if (!WeakFurniture.IsValid() || !WeakThis.IsValid()) return;
            if (!WeakFurniture->GetDamageSystem())
            {
                UE_LOG(LogTemp, Warning, TEXT("[GameMode] %s DamageSystem nullptr — 무적 설정 불가"), *WeakFurniture->GetName());
                return;
            }
            WeakFurniture->GetDamageSystem()->SetSuperInvincible(true);
            UE_LOG(LogTemp, Log, TEXT("[GameMode] %s 무적 ON (3초 경과)"), *WeakFurniture->GetName());
            WeakThis->InvincibleTimerHandles.Remove(WeakFurniture.Get());
        }, 3.0f, false);
    }

    // 모든 가구가 트럭 안에 들어오면 게임 종료
    if (GS->RemainingFurniture <= 0)
    {
        FinishGame(true);
    }
}

void ATeamCarryGameMode::OnFurnitureExitTruck(AActor* FurnitureActor, float CurrentHealth, float MaxHealth, int32 BaseScore)
{
    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS || GS->bIsGameFinished) return;

    // 파괴된 가구면 OnFurnitureDestroyed 에서 처리하므로 무시
    // (가구 파괴 시 물리 이벤트로 End Overlap 이 먼저 호출되는 경우 방지)
    if (ATCFurnitureActor* TCFurniture = Cast<ATCFurnitureActor>(FurnitureActor))
    {
        if (TCFurniture->bIsFurnitureDestroyed) return;
    }

    // 트럭 이탈 시 무적 해제 + 진행 중인 3초 타이머도 취소
    if (AFurnitureActor* Furniture = Cast<AFurnitureActor>(FurnitureActor))
    {
        // 3초 타이머가 아직 진행 중이면 취소
        if (FTimerHandle* Handle = InvincibleTimerHandles.Find(FurnitureActor))
        {
            GetWorldTimerManager().ClearTimer(*Handle);
            InvincibleTimerHandles.Remove(FurnitureActor);
            UE_LOG(LogTemp, Log, TEXT("[GameMode] %s 무적 타이머 취소 (트럭 이탈)"), *Furniture->GetName());
        }
        // 이미 무적 상태면 해제
        if (Furniture->GetDamageSystem())
        {
            Furniture->GetDamageSystem()->SetSuperInvincible(false);
            UE_LOG(LogTemp, Log, TEXT("[GameMode] %s 무적 OFF (트럭 이탈)"), *Furniture->GetName());
        }
    }

    // UniqueID 로 찾아서 목록에서 제거 (포인터 비교 불일치 방지)
    const uint32 TargetID = FurnitureActor ? FurnitureActor->GetUniqueID() : 0;
    for (int32 i = 0; i < FurnitureInTruck.Num(); i++)
    {
        if (FurnitureInTruck[i].ActorUniqueID == TargetID)
        {
            FurnitureInTruck.RemoveAt(i);
            break;
        }
    }

    // 이탈 시 현재 내구도로 점수 차감
    AccumulatedScore -= CalculateScore(CurrentHealth, MaxHealth, BaseScore);

    GS->RemainingFurniture++;
    GS->OnRep_RemainingFurniture();

    GS->TotalScore = AccumulatedScore;
    GS->OnRep_TotalScore();

    UE_LOG(LogTemp, Warning, TEXT("가구 트럭 이탈 | 예상 점수: %d | 남은 가구: %d"),
        GS->TotalScore, GS->RemainingFurniture);
}

void ATeamCarryGameMode::OnFurnitureDestroyed(AActor* FurnitureActor)
{
    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS || GS->bIsGameFinished) return;

    for (int32 i = 0; i < FurnitureInTruck.Num(); i++)
    {
        if (FurnitureInTruck[i].FurnitureActor.Get() == FurnitureActor)
        {
            GS->RemainingFurniture++;
            FurnitureInTruck.RemoveAt(i);

            // 파괴된 가구 제외한 나머지 가구들로 점수 재계산
            AccumulatedScore = 0;
            for (const FTruckFurnitureInfo& Info : FurnitureInTruck)
            {
                AccumulatedScore += CalculateScore(Info.CurrentHealth, Info.MaxHealth, Info.BaseScore);
            }

            GS->TotalScore = AccumulatedScore;
            GS->OnRep_TotalScore();
            UE_LOG(LogTemp, Warning, TEXT("[GameMode] 트럭 안 가구 파괴 — 점수 재계산: %d"), AccumulatedScore);
            break;
        }
    }

    GS->RemainingFurniture--;
    // 파괴된 가구 개수 증가 — Txt_FurnitureCount 분모 갱신 및 TCFeedbackSubsystem 핫타임 비율 계산에 사용
    GS->DestroyedFurnitureCount++;
    GS->OnRep_RemainingFurniture();

    UE_LOG(LogTemp, Warning, TEXT("가구 파괴 | 남은 가구: %d | 파괴 누계: %d"), GS->RemainingFurniture, GS->DestroyedFurnitureCount);

    if (GS->RemainingFurniture <= 0)
    {
        FinishGame(true);
    }
}

int32 ATeamCarryGameMode::CalculateStar()
{
    // 트럭 안 가구 비율 기준 별 판정
    if (TotalFurnitureCount <= 0) return 1;

    const float Ratio = (float)FurnitureInTruck.Num() / (float)TotalFurnitureCount;

    if (Ratio >= 0.75f) return 3; // 75% 이상 트럭 안 → 별 3개
    if (Ratio >= 0.50f) return 2; // 50% 이상 → 별 2개
    return 1;                     // 그 이하 → 별 1개
}

int32 ATeamCarryGameMode::CalculateFinalScore()
{
    // 누적 점수 방식으로 최적화 (전체 순회 제거)
    return AccumulatedScore;
}

int32 ATeamCarryGameMode::CalculateScore(float CurrentHealth, float MaxHealth, int32 BaseScore)
{
    if (MaxHealth <= 0) return 0;

    float HealthRatio = CurrentHealth / MaxHealth;

    float PayoutRate = 0.0f;
    if (HealthRatio > 0.8f)       PayoutRate =  1.0f;
    else if (HealthRatio > 0.6f)  PayoutRate =  0.8f;
    else if (HealthRatio > 0.4f)  PayoutRate =  0.6f;
    else if (HealthRatio > 0.2f)  PayoutRate =  0.4f;
    else if (HealthRatio > 0.0f)  PayoutRate =  0.2f;
    else                          PayoutRate =  0.0f;

    return FMath::FloorToInt(BaseScore * PayoutRate);
}

void ATeamCarryGameMode::FinishGame(bool bIsClear)
{
    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS) return;

    // 재진입 가드: bIsGameFinished 는 5초 뒤에야 true 가 되므로, 그 사이 Tick 의 시간초과
    // 조건과 가구 적재 경로가 FinishGame 을 매 프레임 반복 호출해 결과 트리거/세이브가
    // 수백 번 중복 실행된다. 페이즈를 즉시 Result 로 확정해 두 경로를 모두 차단한다.
    if (GS->CurrentPhase == EGamePhase::Result) return;

    GS->TotalScore = AccumulatedScore;
    GS->OnRep_TotalScore();

    // 트럭 안 가구 비율 기준으로 별 판정
    GS->StarCount = CalculateStar();

    // 명세 4장-8: 로컬 Pause 대신 타이머류도 명시적으로 정지시킨다(카운트다운 중 조기 종료되는 경우 대비).
    GetWorldTimerManager().ClearTimer(CountdownTimerHandle);

    // 페이즈는 즉시 Result 로 전환(위 재진입 가드의 기준). 결과 UI 트리거(bIsGameFinished)만
    // 5초 뒤에 흘려보내 연출 딜레이를 유지한다. (OnRep_CurrentPhase 는 로그만 찍는 no-op)
    SetGamePhase(EGamePhase::Result);

    // 5초 딜레이 후 결과창 표시
    TWeakObjectPtr<ATeamCarryGameMode> WeakThis = this;
    FTimerHandle FinishTimerHandle;
    GetWorldTimerManager().SetTimer(FinishTimerHandle, [WeakThis, bIsClear]()
    {
        if (!WeakThis.IsValid()) return;

        ATeamCarryGameState* GS = WeakThis->GetCachedGameState();
        if (!GS) return;

        // bIsGameFinished를 true로 만들기 전에 TotalScore/StarCount를 먼저 확정해야 한다.
        // OnRep_bIsGameFinished()가 TriggerGameResult(TotalScore, StarCount)로 두 값을 함께 읽어가기 때문이다.
        GS->bIsGameFinished = true;
        GS->OnRep_bIsGameFinished();

        if (bIsClear)
        {
            WeakThis->SaveGame(WeakThis->GetWorld()->GetMapName());
        }

        UE_LOG(LogTemp, Warning, TEXT("게임 종료 | 최종 점수: %d | 별: %d개 | 소요 시간: %.1f초"),
            GS->TotalScore, GS->StarCount, GS->ElapsedTime);

    }, 5.0f, false);
}

bool ATeamCarryGameMode::IsStageCleared() const
{
    // const 함수라 CachedGameState를 직접 쓸 수 없으므로 GetGameState 사용
    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
    if (!GS) return false;
    return GS->bIsGameFinished;
}

int32 ATeamCarryGameMode::GetDeliveredCount() const
{
    return FurnitureInTruck.Num();
}

int32 ATeamCarryGameMode::GetTargetCount() const
{
    return TotalFurnitureCount;
}

void ATeamCarryGameMode::OnStageCleared_Implementation()
{
    FinishGame(true);
}