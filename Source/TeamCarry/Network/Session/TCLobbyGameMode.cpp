// TCLobbyGameMode.cpp

#include "Network/Session/TCLobbyGameMode.h"
#include "Network/Session/TCLobbyGameState.h"
#include "Network/Session/TCSessionFlow.h"
#include "Player/PlayerState/TCPlayerState.h"
#include "Player/PlayerController/TCPlayerController.h"
#include "Network/Net/TCNetStatics.h"

ATCLobbyGameMode::ATCLobbyGameMode()
{
	// 로비는 조작 폰 없음(UI 전용). 캐릭터 프리뷰가 필요하면 에디터 BP 에서 덮어쓴다.
	DefaultPawnClass = nullptr;

	GameStateClass = ATCLobbyGameState::StaticClass();
	PlayerStateClass = ATCPlayerState::StaticClass();
	PlayerControllerClass = ATCPlayerController::StaticClass();
}

void ATCLobbyGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);

	if (APlayerController* PC = Cast<APlayerController>(NewPlayer))
	{
		if (ATCPlayerState* PS = PC->GetPlayerState<ATCPlayerState>())
		{
			PS->SetLobbySlotIndexAuthoritative(NextSlotIndex++);
			UE_LOG(LogTCNet, Log, TEXT("[Lobby] Slot %d 배정: %s"), PS->GetLobbySlotIndex(), *PS->GetPlayerName());
		}
	}
}

void ATCLobbyGameMode::Logout(AController* Exiting)
{
	// 슬롯 인덱스는 회수하지 않는다(중간 퇴장 시 빈 슬롯 허용). UI 는 GameState 변경으로 갱신.
	if (ATCLobbyGameState* LobbyGS = GetGameState<ATCLobbyGameState>())
	{
		LobbyGS->NotifyLobbyChanged();
	}
	Super::Logout(Exiting);
}

void ATCLobbyGameMode::StartGameFromLobby(ATCPlayerController* RequestingPC)
{
	ATCLobbyGameState* LobbyGS = GetGameState<ATCLobbyGameState>();
	if (!LobbyGS || !LobbyGS->AreAllPlayersReady())
	{
		UE_LOG(LogTCNet, Warning, TEXT("[Lobby] 시작 거부: 전원 준비 미완료"));
		return;
	}

	// 맵 결정·ServerTravel 은 SessionFlow 에 위임(새 게임/이어하기 분기 단일화).
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTCSessionFlow* Flow = GI->GetSubsystem<UTCSessionFlow>())
		{
			UE_LOG(LogTCNet, Log, TEXT("[Lobby] 전원 준비 완료 — 게임 시작"));
			Flow->HostStartGame();
			return;
		}
	}
	UE_LOG(LogTCNet, Warning, TEXT("[Lobby] StartGameFromLobby: UTCSessionFlow 없음"));
}
