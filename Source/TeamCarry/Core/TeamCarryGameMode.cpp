#include "TeamCarryGameMode.h"
#include "TeamCarryGameState.h"
#include "Kismet/GameplayStatics.h"
#include "TCSaveGame.h"
#include "Furniture/TCFurnitureActor.h"

ATeamCarryGameMode::ATeamCarryGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    TotalFurnitureCount = 0;

    // GameState 클래스 설정
    GameStateClass = ATeamCarryGameState::StaticClass();
}

void ATeamCarryGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    TArray<AActor*> FurnitureActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATCFurnitureActor::StaticClass(), FurnitureActors);
    SetTotalFurnitureCount(FurnitureActors.Num());

    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
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

    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
    if (!GS || GS->bIsGameFinished) return;
    
    // Playing 단계일 때만 스톱워치 작동
    if (GS->CurrentPhase == EGamePhase::Playing)
    {
        GS->ElapsedTime += DeltaTime;
    }
}

void ATeamCarryGameMode::SetTotalFurnitureCount(int32 Count)
{
    TotalFurnitureCount = Count;

    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
    if (GS)
    {
        GS->RemainingFurniture = Count;

        // 리슨 서버 호스트는 자기 자신에게 OnRep이 트리거되지 않으므로 수동 호출로 UI를 즉시 갱신한다.
        GS->OnRep_RemainingFurniture();
    }
}

void ATeamCarryGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);

    // 남은 플레이어 수 확인
    int32 PlayerCount = GetNumPlayers();
    UE_LOG(LogTemp, Warning, TEXT("플레이어 이탈 | 남은 플레이어: %d"), PlayerCount - 1);

    // 모든 플레이어가 나가면 게임 종료
    if (PlayerCount <= 1)
    {
        FinishGame(false);
    }
}

void ATeamCarryGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
    if (!GS) return;

    UE_LOG(LogTemp, Warning, TEXT("플레이어 재접속 | 현재 단계: %d"), (int32)GS->CurrentPhase);

    // 게임 진행 중에 재접속하면 현재 게임 상태 동기화
    if (GS->CurrentPhase == EGamePhase::Playing)
    {
        UE_LOG(LogTemp, Warning, TEXT("플레이어 재접속 | 게임 진행 중 복귀"));
    }
}

void ATeamCarryGameMode::SaveGame(const FString& StageName)
{
    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
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
    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
    if (!GS) return;

    GS->CurrentPhase = NewPhase;

    // 리슨 서버 호스트는 자기 자신에게 OnRep이 트리거되지 않으므로 수동 호출로 UI를 즉시 갱신한다.
    GS->OnRep_CurrentPhase();

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
            if (!WeakThis.IsValid())
            {
                return;
            }

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
    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
    if (!GS) return;
    
    if (GS->bIsGameFinished) return;

    // 트럭 안 가구 목록에 추가
    FTruckFurnitureInfo Info;
    Info.RowName = RowName;
    Info.CurrentHealth = CurrentHealth;
    Info.MaxHealth = MaxHealth;
    Info.BaseScore = BaseScore;
    FurnitureInTruck.Add(Info);

    // 남은 가구 차감
    GS->RemainingFurniture--;
    GS->OnRep_RemainingFurniture();

    // 예상 점수 계산 후 GameState에 반영
    GS->TotalScore = CalculateFinalScore();
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
    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
    if (!GS) return;
    
    if (GS->bIsGameFinished) return;

    // 같은 RowName 중 첫 번째 하나만 제거
    for (int32 i = 0; i < FurnitureInTruck.Num(); i++)
    {
        if (FurnitureInTruck[i].RowName == RowName)
        {
            FurnitureInTruck.RemoveAt(i);
            break;
        }
    }

    GS->RemainingFurniture++;

    GS->OnRep_RemainingFurniture();
    GS->TotalScore = CalculateFinalScore();
    GS->OnRep_TotalScore();

    UE_LOG(LogTemp, Warning, TEXT("가구 트럭 이탈: %s | 예상 점수: %d | 남은 가구: %d"),
        *RowName.ToString(), GS->TotalScore, GS->RemainingFurniture);
}

void ATeamCarryGameMode::OnFurnitureDestroyed()
{
    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
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
    int32 Total = 0;
    for (const FTruckFurnitureInfo& Info : FurnitureInTruck)
    {
        Total += CalculateScore(Info.CurrentHealth, Info.MaxHealth, Info.BaseScore);
    }
    return Total;
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
    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
    if (!GS) return;

    // 최종 점수 확정
    GS->TotalScore = CalculateFinalScore();
    GS->OnRep_TotalScore();

    GS->StarCount = CalculateStar(GS->ElapsedTime);

    // bIsGameFinished를 true로 만들기 전에 TotalScore/StarCount를 먼저 확정해야 한다.
    // OnRep_bIsGameFinished()가 TriggerGameResult(TotalScore, StarCount)로 두 값을 함께 읽어가기 때문이다.
    GS->bIsGameFinished = true;
    GS->OnRep_bIsGameFinished();

    SetGamePhase(EGamePhase::Result);
    
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