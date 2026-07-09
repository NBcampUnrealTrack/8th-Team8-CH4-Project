#include "TeamCarryGameMode.h"
#include "TeamCarryGameState.h"
#include "Kismet/GameplayStatics.h"
#include "TCSaveGame.h"
#include "Furniture/TCFurnitureActor.h"
#include "GameFramework/PlayerState.h"
#include "Network/Session/TCGameInstance.h"

ATeamCarryGameMode::ATeamCarryGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    TotalFurnitureCount = 0;
    AccumulatedScore = 0;

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
        GS->ElapsedTime = 0.0f;
    }

    // 게임 시작 시 카운트다운 시작
    SetGamePhase(EGamePhase::WaitingToStart);
    StartCountdown();
}

void ATeamCarryGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS || GS->bIsGameFinished) return;
    
    // Playing 단계일 때만 스톱워치 작동
    if (GS->CurrentPhase == EGamePhase::Playing)
    {
        GS->ElapsedTime += DeltaTime;

        // 제한시간 초과 시 실패 종료 (TimeLimitSeconds <= 0 이면 무제한)
        if (TimeLimitSeconds > 0.0f && GS->ElapsedTime >= TimeLimitSeconds)
        {
            UE_LOG(LogTemp, Warning, TEXT("제한시간 %.0f초 초과 — 게임 종료"), TimeLimitSeconds);
            FinishGame(false);
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

    // 월드가 종료 중이면 FinishGame 호출 안 함
    if (GetWorld() && !GetWorld()->bIsTearingDown && PlayerCount <= 1)
    {
        FinishGame(false);
    }
}

void ATeamCarryGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

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

void ATeamCarryGameMode::SaveGame(const FString& StageName)
{
    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS) return;

    // 기존 세이브 불러오기 (없으면 새로 생성)
    UTCSaveGame* SaveData = LoadGame();
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
    UGameplayStatics::SaveGameToSlot(SaveData, TEXT("TCGameSave"), 0);

    UE_LOG(LogTemp, Warning, TEXT("게임 저장 완료 | 스테이지: %s | 별: %d | 점수: %d"),
        *StageName, Record.BestStar, Record.BestScore);
}

UTCSaveGame* ATeamCarryGameMode::LoadGame()
{
    if (UGameplayStatics::DoesSaveGameExist(TEXT("TCGameSave"), 0))
    {
        return Cast<UTCSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("TCGameSave"), 0));
    }
    return nullptr;
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

void ATeamCarryGameMode::OnFurnitureEnterTruck(FName RowName, float CurrentHealth, float MaxHealth, int32 BaseScore)
{
    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS || GS->bIsGameFinished) return;

    // 트럭 안 가구 목록에 추가
    FTruckFurnitureInfo Info;
    Info.RowName = RowName;
    Info.CurrentHealth = CurrentHealth;
    Info.MaxHealth = MaxHealth;
    Info.BaseScore = BaseScore;
    FurnitureInTruck.Add(Info);

    AccumulatedScore += CalculateScore(CurrentHealth, MaxHealth, BaseScore);

    GS->RemainingFurniture--;
    GS->OnRep_RemainingFurniture();

    GS->TotalScore = AccumulatedScore;
    GS->OnRep_TotalScore();

    UE_LOG(LogTemp, Warning, TEXT("가구 트럭 진입: %s | 예상 점수: %d | 남은 가구: %d"),
        *RowName.ToString(), GS->TotalScore, GS->RemainingFurniture);

    // 모든 가구가 트럭 안에 들어오면 게임 종료
    if (GS->RemainingFurniture <= 0)
    {
        FinishGame(true);
    }
}

void ATeamCarryGameMode::OnFurnitureExitTruck(FName RowName)
{
    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS || GS->bIsGameFinished) return;

    // 같은 RowName 중 첫 번째 하나만 제거
    for (int32 i = 0; i < FurnitureInTruck.Num(); i++)
    {
        if (FurnitureInTruck[i].RowName == RowName)
        {
            AccumulatedScore -= CalculateScore(
                FurnitureInTruck[i].CurrentHealth,
                FurnitureInTruck[i].MaxHealth,
                FurnitureInTruck[i].BaseScore);
            FurnitureInTruck.RemoveAt(i);
            break;
        }
    }

    GS->RemainingFurniture++;
    GS->OnRep_RemainingFurniture();

    GS->TotalScore = AccumulatedScore;
    GS->OnRep_TotalScore();

    UE_LOG(LogTemp, Warning, TEXT("가구 트럭 이탈: %s | 예상 점수: %d | 남은 가구: %d"),
        *RowName.ToString(), GS->TotalScore, GS->RemainingFurniture);
}

void ATeamCarryGameMode::OnFurnitureDestroyed()
{
    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS || GS->bIsGameFinished) return;

    GS->RemainingFurniture--;
    GS->OnRep_RemainingFurniture();

    UE_LOG(LogTemp, Warning, TEXT("가구 파괴 | 남은 가구: %d"), GS->RemainingFurniture);

    if (GS->RemainingFurniture <= 0)
    {
        FinishGame(true);
    }
}

int32 ATeamCarryGameMode::CalculateStar(float ElapsedTime)
{
    if (ElapsedTime <= StarThreeTime) return 3;
    if (ElapsedTime <= StarTwoTime)  return 2;
    return 1;
}

int32 ATeamCarryGameMode::CalculateFinalScore()
{
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
    else if (HealthRatio > 0.2f)  PayoutRate = -0.2f;
    else if (HealthRatio > 0.0f)  PayoutRate = -0.3f;
    else                          PayoutRate = -0.5f;

    return FMath::FloorToInt(BaseScore * PayoutRate);
}

void ATeamCarryGameMode::FinishGame(bool bIsClear)
{
    ATeamCarryGameState* GS = GetCachedGameState();
    if (!GS) return;

    GS->TotalScore = AccumulatedScore;
    GS->OnRep_TotalScore();

    GS->StarCount = CalculateStar(GS->ElapsedTime);
    
    GS->bIsGameFinished = true;
    GS->OnRep_bIsGameFinished();

    SetGamePhase(EGamePhase::Result);

    GetWorldTimerManager().ClearTimer(CountdownTimerHandle);

    if (bIsClear)
    {
        SaveGame(GetWorld()->GetMapName());
    }

    UE_LOG(LogTemp, Warning, TEXT("게임 종료 | 최종 점수: %d | 별: %d개 | 소요 시간: %.1f초"),
        GS->TotalScore, GS->StarCount, GS->ElapsedTime);
}

bool ATeamCarryGameMode::IsStageCleared() const
{
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
