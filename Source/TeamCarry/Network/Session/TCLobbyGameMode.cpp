// TCLobbyGameMode.cpp

#include "Network/Session/TCLobbyGameMode.h"
#include "Network/Session/TCLobbyGameState.h"
#include "Network/Session/TCSessionFlow.h"
#include "Network/Session/TCGameInstance.h"
#include "Player/PlayerState/TCPlayerState.h"
#include "Player/PlayerController/TCPlayerController.h"
#include "Network/Net/TCNetStatics.h"

ATCLobbyGameMode::ATCLobbyGameMode()
{
	// 로비는 조작 폰 없음(UI 전용). 캐릭터 프리뷰가 필요하면 에디터 BP 에서 덮어쓴다.
	DefaultPawnClass = nullptr;

	// Seamless Travel: 넷드라이버·연결을 유지한 채 맵 전환.
	// hard travel 은 리슨 소켓을 파괴/재생성하는데, SteamSockets 는 이전 리슨 소켓
	// (P2P vport) 정리가 지연되어 재바인딩이 실패("Already have a listen socket")
	// → 트래블 실패 → 기본맵 폴백. 심리스로 전환하면 소켓을 그대로 쓰므로 해소되고,
	// 클라이언트도 트래블 중 끊김 없이 따라온다.
	bUseSeamlessTravel = true;

	GameStateClass = ATCLobbyGameState::StaticClass();
	PlayerStateClass = ATCPlayerState::StaticClass();
	PlayerControllerClass = ATCPlayerController::StaticClass();
}

void ATCLobbyGameMode::InitGameState()
{
	Super::InitGameState();

	// 호스트의 방 코드는 GameInstance(호스트 프로세스)에만 존재 → GameState 복제 변수로
	// 옮겨 클라이언트 UI 도 표시할 수 있게 한다. (없으면 빈 문자열 = UI 가 "오프라인" 표기)
	if (ATCLobbyGameState* LobbyGS = GetGameState<ATCLobbyGameState>())
	{
		if (UTCGameInstance* TCGI = Cast<UTCGameInstance>(GetGameInstance()))
		{
			LobbyGS->SetRoomCodeAuthoritative(TCGI->GetHostRoomCode());
			UE_LOG(LogTCNet, Log, TEXT("[Lobby] RoomCode 복제 주입: '%s'"), *TCGI->GetHostRoomCode());
		}

		// 선택된 스테이지도 RoomCode와 동일한 이유로 재주입이 필요하다: UTCSessionFlow::SelectedStageId
		// (GameInstance 소유)는 세션 내내 유지되지만, ATCLobbyGameState::SelectedStageId(Actor 복제
		// 변수)는 로비에 재진입할 때마다(Seamless Travel로 새 GameState가 생성됨) 0으로 리셋된다.
		// 이 재주입이 없으면 실제 이어하기 목적지(HostStartGame -> GetSelectedStageMapPath)는 마지막
		// 선택 그대로인데, 게시판 UI는 미선택 상태로 보이는 불일치가 생긴다.
		if (UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr)
		{
			LobbyGS->SetSelectedStageIdAuthoritative(Flow->GetSelectedStageId());
			UE_LOG(LogTCNet, Log, TEXT("[Lobby] SelectedStageId 복제 주입: %d"), Flow->GetSelectedStageId());
		}
	}
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

			// 접속 로그(명세 3장·4장-7).
			if (ATCLobbyGameState* LobbyGS = GetGameState<ATCLobbyGameState>())
			{
				LobbyGS->AddSessionLogEntry(FText::Format(NSLOCTEXT("SessionLog", "PlayerJoined", "{0}님이 입장했습니다."), FText::FromString(PS->GetPlayerName())));
			}
		}
	}
}

void ATCLobbyGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	// Seamless 도착 플레이어는 OnPostLogin 미호출 + PlayerState 재생성 과정에서
	// 커스텀 복제값(LobbySlotIndex)이 유실될 수 있다 → 미배정이면 여기서 배정.
	if (APlayerController* PC = Cast<APlayerController>(C))
	{
		if (ATCPlayerState* PS = PC->GetPlayerState<ATCPlayerState>())
		{
			if (PS->GetLobbySlotIndex() < 0)
			{
				PS->SetLobbySlotIndexAuthoritative(NextSlotIndex++);
				UE_LOG(LogTCNet, Log, TEXT("[Lobby] (Seamless) Slot %d 배정: %s"), PS->GetLobbySlotIndex(), *PS->GetPlayerName());
			}
		}
	}
}

void ATCLobbyGameMode::Logout(AController* Exiting)
{
	// 슬롯 인덱스는 회수하지 않는다(중간 퇴장 시 빈 슬롯 허용). UI 는 GameState 변경으로 갱신.
	if (ATCLobbyGameState* LobbyGS = GetGameState<ATCLobbyGameState>())
	{
		LobbyGS->NotifyLobbyChanged();

		// 접속 로그(명세 3장·4장-7). Super::Logout() 전이라 PlayerState 가 아직 유효하다.
		const FString PlayerName = Exiting && Exiting->GetPlayerState<APlayerState>() ? Exiting->GetPlayerState<APlayerState>()->GetPlayerName() : TEXT("Player");
		LobbyGS->AddSessionLogEntry(FText::Format(NSLOCTEXT("SessionLog", "PlayerLeft", "{0}님이 퇴장했습니다."), FText::FromString(PlayerName)));
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
