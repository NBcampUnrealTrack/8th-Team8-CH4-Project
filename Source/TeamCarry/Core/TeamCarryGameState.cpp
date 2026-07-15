#include "TeamCarryGameState.h"
#include "Net/UnrealNetwork.h"
#include "TeamCarry/UI/MockUIController.h" 
#include "Engine/World.h"
#include "Engine/GameInstance.h"

ATeamCarryGameState::ATeamCarryGameState()
{
	TotalScore = 0;
	TotalLevelValue = 0;
	RemainingFurniture = 0;
	ElapsedTime = 0.0f;
	TotalFurnitureCount = 0;
	DestroyedFurnitureCount = 0;
	bIsHotTime = false;
	bIsGameFinished = false;
	StarCount = 0;
	CurrentPhase = EGamePhase::WaitingToStart;
}

void ATeamCarryGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATeamCarryGameState, TotalScore);
	DOREPLIFETIME(ATeamCarryGameState, TotalLevelValue);
	DOREPLIFETIME(ATeamCarryGameState, RemainingFurniture);
	DOREPLIFETIME(ATeamCarryGameState, ElapsedTime);
	DOREPLIFETIME(ATeamCarryGameState, TotalFurnitureCount);
	DOREPLIFETIME(ATeamCarryGameState, DestroyedFurnitureCount);
	DOREPLIFETIME(ATeamCarryGameState, bIsHotTime);
	DOREPLIFETIME(ATeamCarryGameState, bIsGameFinished);
	DOREPLIFETIME(ATeamCarryGameState, StarCount);
	DOREPLIFETIME(ATeamCarryGameState, CurrentPhase);
	DOREPLIFETIME(ATeamCarryGameState, SessionLogEntries);
}

// MockUIController 캐시 가져오기
// OnRep 함수마다 GetWorld()->GetGameInstance()->GetSubsystem 호출을 방지한다.
UMockUIController* ATeamCarryGameState::GetCachedMockController() const
{
	if (!CachedMockController)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				CachedMockController = GI->GetSubsystem<UMockUIController>();
			}
		}
	}
	return CachedMockController;
}

void ATeamCarryGameState::OnRep_TotalScore()
{
	if (UMockUIController* MockController = GetCachedMockController())
	{
		UE_LOG(LogTemp, Log, TEXT("[GameState] UI 점수 갱신: %d"), TotalScore);
		MockController->UpdateTeamMoney(TotalScore);
	}
}

void ATeamCarryGameState::OnRep_RemainingFurniture()
{
	if (UMockUIController* MockController = GetCachedMockController())
	{
		UE_LOG(LogTemp, Log, TEXT("[GameState] UI 남은 가구 갱신: %d"), RemainingFurniture);
		MockController->UpdateRemainingFurniture(RemainingFurniture);
	}
}

void ATeamCarryGameState::OnRep_bIsHotTime()
{
	// 핫타임 상태 변경 시 UI 갱신 (필요 시 조민기님 UI 연동)
	UE_LOG(LogTemp, Log, TEXT("[GameState] 핫타임 상태 변경: %s"), bIsHotTime ? TEXT("ON") : TEXT("OFF"));
}

void ATeamCarryGameState::OnRep_bIsGameFinished()
{
	// 게임이 종료되었다면 최종 점수/별 개수를 전달하며 Result 화면으로 강제 전환합니다.
	// (TotalScore, StarCount는 bIsGameFinished와 같은 프레임에 함께 변경/복제되므로 이 시점에 이미 최신값이다.)
	if (bIsGameFinished)
	{
		if (UMockUIController* MockController = GetCachedMockController())
		{
			UE_LOG(LogTemp, Log, TEXT("[GameState] 게임 종료 확인. Result 화면 호출 (Score: %d, Star: %d)"), TotalScore, StarCount);
			MockController->TriggerGameResult(TotalScore, StarCount, ElapsedTime);
		}
	}
}

void ATeamCarryGameState::OnRep_StarCount()
{

}

void ATeamCarryGameState::AddSessionLogEntry(const FText& NewEntry)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AddSessionLogEntry: 비권위 호출 무시"));
		return;
	}

	// 브로드캐스트용으로 추가 직전 상태를 캡처(리슨 서버 호스트는 OnRep 이 자동 호출되지 않는다).
	const TArray<FText> OldEntries = SessionLogEntries;
	SessionLogEntries.Add(NewEntry);
	OnRep_SessionLogEntries(OldEntries);
}

void ATeamCarryGameState::OnRep_SessionLogEntries(const TArray<FText>& OldSessionLogEntries)
{
	if (UMockUIController* MockController = GetCachedMockController())
	{
		// 이전 값 대비 새로 추가된 항목만 순서대로 Broadcast(늦게 접속한 클라는 전체 이력을 받는다).
		for (int32 i = OldSessionLogEntries.Num(); i < SessionLogEntries.Num(); ++i)
		{
			MockController->OnSessionLogAdded.Broadcast(SessionLogEntries[i]);
		}
	}
}

void ATeamCarryGameState::OnRep_CurrentPhase()
{
	if (UMockUIController* MockController = GetCachedMockController())
	{
		UE_LOG(LogTemp, Log, TEXT("[GameState] UI 페이즈 갱신: %d"), (int32)CurrentPhase);
		// 카운트다운 시작 등 페이즈 변화에 따른 UI 연출이 있다면 이곳에서 호출합니다.
		// MockController->OnPhaseChanged(CurrentPhase);
	}
}
