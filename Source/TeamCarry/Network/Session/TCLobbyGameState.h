// TCLobbyGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TCLobbyGameState.generated.h"

// 로비 인원/준비 상태가 바뀔 때마다 브로드캐스트(플레이어 입퇴장·Ready·캐릭터 변경).
// UI(S_CharacterSelect)가 구독해 PlayerArray 를 다시 읽어 슬롯을 갱신한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLobbyPlayersChanged);

/**
 * ATCLobbyGameState - 로비 레벨의 복제 상태.
 *
 * PlayerArray(엔진 복제) 위에 "로비 변경 알림"과 "전원 준비 판정"을 얹는다.
 * 별도 복제 변수는 두지 않고, 각 ATCPlayerState 의 OnRep 이 NotifyLobbyChanged 를 호출한다.
 */
UCLASS()
class TEAMCARRY_API ATCLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	// 로비 상태 변경 이벤트(서버/클라 공통). UI 가 구독.
	UPROPERTY(BlueprintAssignable, Category = "TeamCarry|Lobby")
	FOnLobbyPlayersChanged OnLobbyPlayersChanged;

	// 접속 인원 전원이 Ready 인가(시작 게이팅). 인원 0이면 false.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Lobby")
	bool AreAllPlayersReady() const;

	// 준비 완료 인원 수.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Lobby")
	int32 GetReadyCount() const;

	// PlayerState OnRep / GameMode 가 호출. OnLobbyPlayersChanged 를 한 번 쏜다.
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Lobby")
	void NotifyLobbyChanged();

	// PlayerArray 변경(입퇴장)도 로비 변경으로 간주.
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
};
