// TCPlayerController.cpp

#include "Player/PlayerController/TCPlayerController.h"
#include "Player/PlayerState/TCPlayerState.h"
#include "Network/Session/TCLobbyGameMode.h"
#include "Network/Net/TCNetStatics.h"

// --- UI 테스트용 MockUIController, GameInstance ---
#include "TeamCarry/UI/MockUIController.h"
#include "Engine/GameInstance.h"

void ATCPlayerController::RequestSetReady(bool bInReady)
{
	ServerSetReady(bInReady);
}

void ATCPlayerController::RequestStartGame()
{
	ServerRequestStartGame();
}

// --- UI 테스트용 BeginPlay() ---
void ATCPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 서버 뒷단이 아닌, 실제 모니터 화면을 보고 있는 '로컬 플레이어'일 때만 UI를 띄웁니다.
	if (IsLocalPlayerController())
	{
		if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
		{
			// 현재 프로젝트 구조에 따라 PushOverlay 또는 ReplaceState를 사용합니다.
			// 로비 UI가 전체 화면 상태라면 ReplaceState(EE_UIState::CharacterSelect) 등을 활용할 수 있습니다.
			MockController->ReplaceState(EE_UIState::CharacterSelect);

			UE_LOG(LogTCNet, Log, TEXT("[PlayerController] 로컬 플레이어 로비 진입 완료. S_CharacterSelect 호출."));
		}
	}
}

void ATCPlayerController::ServerSetReady_Implementation(bool bInReady)
{
	if (ATCPlayerState* PS = GetPlayerState<ATCPlayerState>())
	{
		PS->SetReadyAuthoritative(bInReady);
	}
}

void ATCPlayerController::ServerRequestStartGame_Implementation()
{
	// 시작 권한·전원 준비 검증은 GameMode 에 위임(서버 권위).
	if (ATCLobbyGameMode* LobbyGM = GetWorld() ? GetWorld()->GetAuthGameMode<ATCLobbyGameMode>() : nullptr)
	{
		LobbyGM->StartGameFromLobby(this);
	}
	else
	{
		UE_LOG(LogTCNet, Warning, TEXT("ServerRequestStartGame: ATCLobbyGameMode 없음 — 무시"));
	}
}
