#include "TeamCarryGameMode.h"
#include "TeamCarryGameState.h"
#include "Kismet/GameplayStatics.h"

ATeamCarryGameMode::ATeamCarryGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    bIsStopWatchRunning = false;
    TotalFurnitureCount = 0;

    // GameState 클래스 설정
    GameStateClass = ATeamCarryGameState::StaticClass();
}

void ATeamCarryGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 스톱워치 시작
    bIsStopWatchRunning = true;
}

void ATeamCarryGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 스톱워치 갱신
    if (bIsStopWatchRunning)
    {
        ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
        if (GS)
        {
            GS->ElapsedTime += DeltaTime;
        }
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

void ATeamCarryGameMode::OnFurnitureLoaded(FName RowName, float CurrentHealth, float MaxHealth, int32 BaseScore)
{
    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
    if (!GS) return;

    // 중복 정산 방지는 가구 쪽(홍민기님)에서 처리

    // 점수 계산
    int32 FinalScore = CalculateScore(CurrentHealth, MaxHealth, BaseScore);

    // 점수 누적
    GS->TotalScore += FinalScore;

    // 남은 가구 개수 차감
    GS->RemainingFurniture--;

    // 승패 판정
    CheckGameFinished();
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

void ATeamCarryGameMode::CheckGameFinished()
{
    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
    if (!GS) return;

    // 남은 가구가 0이면 클리어
    if (GS->RemainingFurniture <= 0)
    {
        FinishGame(true);
    }
}

void ATeamCarryGameMode::FinishGame(bool bIsClear)
{
    // 스톱워치 정지
    bIsStopWatchRunning = false;

    ATeamCarryGameState* GS = GetGameState<ATeamCarryGameState>();
    if (GS)
    {
        GS->bIsGameFinished = true;
    }

    // 결과창 표시 (조민기님 UI 완성 후 연동)
}