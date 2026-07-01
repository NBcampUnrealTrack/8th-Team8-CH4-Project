// TCPlayerState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "TCPlayerState.generated.h"

/**
 * ATCPlayerState - 로비 복제 상태를 담는 PlayerState.
 *
 * 명세(세션 흐름) 4단계의 "캐릭터 선택 + bIsReady 복제"를 담당한다.
 * UI 레이어에 의존하지 않는다(원시 복제 변수 + 변경 알림만 제공). UI(S_CharacterSelect)는
 * ATCLobbyGameState 의 OnLobbyPlayersChanged 를 구독해 PlayerArray 를 읽어 슬롯을 갱신한다.
 *
 * set 은 서버 권위에서만(컨트롤러의 Server RPC 경유). 클라는 복제값을 읽기만 한다.
 */
UCLASS()
class TEAMCARRY_API ATCPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ATCPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── 읽기(클라/서버 공용) ──

	UFUNCTION(BlueprintPure, Category = "TeamCarry|Lobby")
	bool IsReady() const { return bIsReady; }

	UFUNCTION(BlueprintPure, Category = "TeamCarry|Lobby")
	int32 GetCharacterIndex() const { return CharacterIndex; }

	// GameMode 가 PostLogin 때 배정하는 로비 슬롯(0~3). UI 슬롯 위젯 매핑용.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Lobby")
	int32 GetLobbySlotIndex() const { return LobbySlotIndex; }

	// ── 쓰기(서버 권위 전용) ──
	// HasAuthority() 가드 내장. 클라가 직접 부르면 무시된다(로그만).

	void SetReadyAuthoritative(bool bInReady);
	void SetCharacterIndexAuthoritative(int32 InIndex);
	void SetLobbySlotIndexAuthoritative(int32 InSlot);

protected:
	// 준비 완료 플래그.
	UPROPERTY(ReplicatedUsing = OnRep_LobbyInfo, VisibleAnywhere, Category = "TeamCarry|Lobby")
	bool bIsReady = false;

	// 선택한 캐릭터 인덱스.
	UPROPERTY(ReplicatedUsing = OnRep_LobbyInfo, VisibleAnywhere, Category = "TeamCarry|Lobby")
	int32 CharacterIndex = 0;

	// 로비 슬롯 인덱스(서버 배정).
	UPROPERTY(ReplicatedUsing = OnRep_LobbyInfo, VisibleAnywhere, Category = "TeamCarry|Lobby")
	int32 LobbySlotIndex = -1;

	UFUNCTION()
	void OnRep_LobbyInfo();

private:
	// 복제값 변경 시 GameState 에 로비 변경을 알려 UI 갱신을 트리거한다(서버/클라 공통).
	void NotifyLobbyChanged();
};
