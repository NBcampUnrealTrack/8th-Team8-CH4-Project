// TCLobbyGameState.cpp

#include "Network/Session/TCLobbyGameState.h"
#include "Player/PlayerState/TCPlayerState.h"
#include "Network/Net/TCNetStatics.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"

void ATCLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATCLobbyGameState, RoomCode);
	DOREPLIFETIME(ATCLobbyGameState, SelectedStageId);
	DOREPLIFETIME(ATCLobbyGameState, SessionLogEntries);
}

void ATCLobbyGameState::SetRoomCodeAuthoritative(const FString& InCode)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTCNet, Warning, TEXT("SetRoomCodeAuthoritative: 비권위 호출 무시"));
		return;
	}
	if (RoomCode == InCode)
	{
		return;
	}
	RoomCode = InCode;
	// 서버(호스트) 자신은 OnRep 이 안 불리므로 직접 UI 갱신 통지.
	NotifyLobbyChanged();
}

void ATCLobbyGameState::OnRep_RoomCode()
{
	// 클라: 코드 도착 → 위젯 갱신 트리거.
	NotifyLobbyChanged();
}

void ATCLobbyGameState::SetSelectedStageIdAuthoritative(int32 InStageId)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTCNet, Warning, TEXT("SetSelectedStageIdAuthoritative: 비권위 호출 무시"));
		return;
	}
	if (SelectedStageId == InStageId)
	{
		return;
	}
	SelectedStageId = InStageId;
	// 서버(호스트) 자신은 OnRep 이 안 불리므로 직접 UI 갱신 통지.
	NotifyLobbyChanged();
}

void ATCLobbyGameState::OnRep_SelectedStageId()
{
	// 클라: 방장이 고른 스테이지 도착 → 위젯 갱신 트리거.
	NotifyLobbyChanged();
}

void ATCLobbyGameState::AddSessionLogEntry(const FText& NewEntry)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTCNet, Warning, TEXT("AddSessionLogEntry: 비권위 호출 무시"));
		return;
	}

	// 브로드캐스트용으로 추가 직전 상태를 캡처(리슨 서버 호스트는 OnRep 이 자동 호출되지 않는다).
	const TArray<FText> OldEntries = SessionLogEntries;
	SessionLogEntries.Add(NewEntry);
	OnRep_SessionLogEntries(OldEntries);
}

void ATCLobbyGameState::OnRep_SessionLogEntries(const TArray<FText>& OldSessionLogEntries)
{
	if (UWorld* World = GetWorld())
	{
		if (UMockUIController* MockController = World->GetGameInstance()->GetSubsystem<UMockUIController>())
		{
			// 이전 값 대비 새로 추가된 항목만 순서대로 Broadcast(늦게 접속한 클라는 전체 이력을 받는다).
			for (int32 i = OldSessionLogEntries.Num(); i < SessionLogEntries.Num(); ++i)
			{
				MockController->OnSessionLogAdded.Broadcast(SessionLogEntries[i]);
			}
		}
	}
}

bool ATCLobbyGameState::AreAllPlayersReady() const
{
	int32 Counted = 0;
	for (APlayerState* PS : PlayerArray)
	{
		const ATCPlayerState* TCPS = Cast<ATCPlayerState>(PS);
		if (!TCPS)
		{
			continue;
		}
		// 아직 합류 중(BeginPlay 전)인 슬롯 미배정 PS 는 카운트에서 제외.
		if (TCPS->GetLobbySlotIndex() < 0)
		{
			continue;
		}
		++Counted;
		if (!TCPS->IsReady())
		{
			return false;
		}
	}
	return Counted > 0;
}

int32 ATCLobbyGameState::GetReadyCount() const
{
	int32 Ready = 0;
	for (APlayerState* PS : PlayerArray)
	{
		if (const ATCPlayerState* TCPS = Cast<ATCPlayerState>(PS))
		{
			if (TCPS->IsReady())
			{
				++Ready;
			}
		}
	}
	return Ready;
}

void ATCLobbyGameState::NotifyLobbyChanged()
{
	OnLobbyPlayersChanged.Broadcast();
}

void ATCLobbyGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	NotifyLobbyChanged();
}

void ATCLobbyGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);
	NotifyLobbyChanged();
}
