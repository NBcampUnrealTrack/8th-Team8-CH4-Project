// TCLobbyGameState.cpp

#include "Network/Session/TCLobbyGameState.h"
#include "Player/PlayerState/TCPlayerState.h"
#include "Network/Net/TCNetStatics.h"
#include "Net/UnrealNetwork.h"

void ATCLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATCLobbyGameState, RoomCode);
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
