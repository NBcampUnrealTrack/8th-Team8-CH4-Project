#include "TeamCarryGameState.h"
#include "Net/UnrealNetwork.h"

ATeamCarryGameState::ATeamCarryGameState()
{
	TotalScore = 0;
	RemainingFurniture = 0;
	ElapsedTime = 0.0f;
	bIsGameFinished = false;
}

void ATeamCarryGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATeamCarryGameState, TotalScore);
	DOREPLIFETIME(ATeamCarryGameState, RemainingFurniture);
	DOREPLIFETIME(ATeamCarryGameState, ElapsedTime);
	DOREPLIFETIME(ATeamCarryGameState, bIsGameFinished);
}

void ATeamCarryGameState::OnRep_TotalScore()
{
	// UI 갱신 (조민기님 UI 완성 후 연동)
}

void ATeamCarryGameState::OnRep_RemainingFurniture()
{
	// 남은 가구 UI 갱신
}

void ATeamCarryGameState::OnRep_bIsGameFinished()
{
	// 결과창 표시
}