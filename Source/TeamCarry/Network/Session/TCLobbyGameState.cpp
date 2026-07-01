// TCLobbyGameState.cpp

#include "Network/Session/TCLobbyGameState.h"
#include "Player/PlayerState/TCPlayerState.h"
#include "Network/Net/TCNetStatics.h"

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
