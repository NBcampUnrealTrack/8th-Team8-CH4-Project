#include "TeamCarryGameState.h"
#include "Net/UnrealNetwork.h"
#include "TeamCarry/UI/MockUIController.h" 
#include "Engine/World.h"
#include "Engine/GameInstance.h"

ATeamCarryGameState::ATeamCarryGameState()
{
	TotalScore = 0;
	RemainingFurniture = 0;
	ElapsedTime = 0.0f;
	bIsGameFinished = false;
	StarCount = 0;
	CurrentPhase = EGamePhase::WaitingToStart;
}

void ATeamCarryGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATeamCarryGameState, TotalScore);
	DOREPLIFETIME(ATeamCarryGameState, RemainingFurniture);
	DOREPLIFETIME(ATeamCarryGameState, ElapsedTime);
	DOREPLIFETIME(ATeamCarryGameState, bIsGameFinished);
	DOREPLIFETIME(ATeamCarryGameState, StarCount);
	DOREPLIFETIME(ATeamCarryGameState, CurrentPhase);
}

void ATeamCarryGameState::OnRep_TotalScore()
{
	if (UWorld* World = GetWorld())
	{
		if (UMockUIController* MockController = World->GetGameInstance()->GetSubsystem<UMockUIController>())
		{
			UE_LOG(LogTemp, Log, TEXT("[GameState] UI 점수 갱신: %d"), TotalScore);
			MockController->UpdateTeamMoney(TotalScore);
		}
	}
}

void ATeamCarryGameState::OnRep_RemainingFurniture()
{
	if (UWorld* World = GetWorld())
	{
		if (UMockUIController* MockController = World->GetGameInstance()->GetSubsystem<UMockUIController>())
		{
			UE_LOG(LogTemp, Log, TEXT("[GameState] UI 남은 가구 갱신: %d"), RemainingFurniture);
			MockController->UpdateRemainingFurniture(RemainingFurniture);
		}
	}
}

void ATeamCarryGameState::OnRep_bIsGameFinished()
{
	// 게임이 종료되었다면 최종 점수/별 개수를 전달하며 Result 화면으로 강제 전환합니다.
	// (TotalScore, StarCount는 bIsGameFinished와 같은 프레임에 함께 변경/복제되므로 이 시점에 이미 최신값이다.)
	if (bIsGameFinished)
	{
		if (UWorld* World = GetWorld())
		{
			if (UMockUIController* MockController = World->GetGameInstance()->GetSubsystem<UMockUIController>())
			{
				UE_LOG(LogTemp, Log, TEXT("[GameState] 게임 종료 확인. Result 화면 호출 (Score: %d, Star: %d)"), TotalScore, StarCount);
				MockController->TriggerGameResult(TotalScore, StarCount);
			}
		}
	}
}

void ATeamCarryGameState::OnRep_StarCount()
{
	// 별 개수 UI 갱신 (보류)
}

void ATeamCarryGameState::OnRep_CurrentPhase()
{
	if (UWorld* World = GetWorld())
	{
		if (UMockUIController* MockController = World->GetGameInstance()->GetSubsystem<UMockUIController>())
		{
			UE_LOG(LogTemp, Log, TEXT("[GameState] UI 페이즈 갱신: %d"), (int32)CurrentPhase);
			// 카운트다운 시작 등 페이즈 변화에 따른 UI 연출이 있다면 이곳에서 호출합니다.
			// MockController->OnPhaseChanged(CurrentPhase);
		}
	}
}