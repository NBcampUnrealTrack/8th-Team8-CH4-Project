// TCPlayerState.cpp

#include "Player/PlayerState/TCPlayerState.h"
#include "Network/Session/TCLobbyGameState.h"
#include "Network/Net/TCNetStatics.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ATCPlayerState::ATCPlayerState()
{
	// 로비 복제값은 자주 바뀌지 않으니 기본 빈도로 충분.
	SetNetUpdateFrequency(10.0f);
}

void ATCPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATCPlayerState, bIsReady);
	DOREPLIFETIME(ATCPlayerState, LobbySlotIndex);
	DOREPLIFETIME(ATCPlayerState, CharacterIndex);
}

void ATCPlayerState::SetReadyAuthoritative(bool bInReady)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTCNet, Warning, TEXT("SetReadyAuthoritative: 비권위 호출 무시"));
		return;
	}
	if (bIsReady == bInReady)
	{
		return;
	}
	bIsReady = bInReady;
	// 서버 자신은 OnRep 이 안 불리므로 직접 알린다(리슨서버 로컬 UI 갱신).
	NotifyLobbyChanged();
}

void ATCPlayerState::SetLobbySlotIndexAuthoritative(int32 InSlot)
{
	if (!HasAuthority())
	{
		return;
	}
	if (LobbySlotIndex == InSlot)
	{
		return;
	}
	LobbySlotIndex = InSlot;
	NotifyLobbyChanged();
}

void ATCPlayerState::SetCharacterIndexAuthoritative(int32 InCharacterIndex)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTCNet, Warning, TEXT("SetCharacterIndexAuthoritative: 비권위 호출 무시"));
		return;
	}
	if (CharacterIndex == InCharacterIndex)
	{
		return;
	}
	CharacterIndex = InCharacterIndex;
	// 서버 자신은 OnRep 이 안 불리므로 직접 알린다(리슨서버 로컬 UI 갱신).
	NotifyLobbyChanged();
}

void ATCPlayerState::OnRep_LobbyInfo()
{
	// 클라에서 복제값이 도착하면 UI 갱신을 트리거.
	NotifyLobbyChanged();
}

void ATCPlayerState::NotifyLobbyChanged()
{
	if (UWorld* World = GetWorld())
	{
		if (ATCLobbyGameState* LobbyGS = World->GetGameState<ATCLobbyGameState>())
		{
			LobbyGS->NotifyLobbyChanged();
		}
	}
}
