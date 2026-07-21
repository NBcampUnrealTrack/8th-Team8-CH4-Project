// TCLobbyGameMode.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TCLobbyGameMode.generated.h"

class ATCPlayerController;

/**
 * ATCLobbyGameMode - 로비(S_CharacterSelect) 레벨 GameMode.
 *
 * 접속 플레이어에게 로비 슬롯을 배정하고, 전원 준비 시 호스트의 시작 요청으로
 * 게임플레이 레벨로 ServerTravel(UTCSessionFlow 위임)한다.
 * 기본값으로 TC 계열 GameState/PlayerState/PlayerController 를 지정한다.
 */
UCLASS()
class TEAMCARRY_API ATCLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATCLobbyGameMode();

	// 호스트 PC 의 시작 요청(서버 권위). 전원 준비 검증 후 트래블.
	void StartGameFromLobby(ATCPlayerController* RequestingPC);

protected:
	// GameState 생성 직후 호스트의 방 코드를 복제 변수로 주입(클라 UI 표시용).
	virtual void InitGameState() override;

	virtual void OnPostLogin(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	// Seamless travel 로 도착한 플레이어는 OnPostLogin 을 타지 않으므로
	// 여기서 로비 슬롯을 배정한다(게임→로비 복귀 등).
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

private:
	// 다음에 배정할 로비 슬롯 인덱스(0부터 증가).
	int32 NextSlotIndex = 0;
};
