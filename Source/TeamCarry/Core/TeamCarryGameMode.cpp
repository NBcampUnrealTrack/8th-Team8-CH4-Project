#include "TeamCarryGameMode.h"
#include "TeamCarryGameState.h"
#include "Kismet/GameplayStatics.h"
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
    }
}

void ATeamCarryGameMode::SetGamePhase(EGamePhase NewPhase)
{
    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
    if (!GS) return;

    GS->CurrentPhase = NewPhase;

    UE_LOG(LogTemp, Warning, TEXT("게임 단계 전환: %d"), (int32)NewPhase);
}

void ATeamCarryGameMode::StartCountdown()
{
    SetGamePhase(EGamePhase::Countdown);
    CountdownTime = 3.0f;

    GetWorldTimerManager().SetTimer(CountdownTimerHandle, [this]()
    {
        CountdownTime -= 1.0f;

        UE_LOG(LogTemp, Warning, TEXT("카운트다운: %.0f"), CountdownTime);

        if (CountdownTime <= 0.0f)
        {
            GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
            SetGamePhase(EGamePhase::Playing);
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

    // 예상 점수 계산 후 GameState에 반영
    GS->TotalScore = CalculateFinalScore();

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

    // 트럭 안 가구 목록에서 제거
    FurnitureInTruck.RemoveAll([&RowName](const FTruckFurnitureInfo& Info)
    {
        return Info.RowName == RowName;
    });

    // 남은 가구 복구
    GS->RemainingFurniture++;

    // 예상 점수 재계산
    GS->TotalScore = CalculateFinalScore();

    UE_LOG(LogTemp, Warning, TEXT("가구 트럭 이탈: %s | 예상 점수: %d | 남은 가구: %d"),
        *RowName.ToString(), GS->TotalScore, GS->RemainingFurniture);
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
    GS->bIsGameFinished = true;
    GS->StarCount = CalculateStar(GS->ElapsedTime);
    
    SetGamePhase(EGamePhase::Result);

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