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
	virtual void OnPostLogin(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

private:
	// 다음에 배정할 로비 슬롯 인덱스(0부터 증가).
	int32 NextSlotIndex = 0;
};
