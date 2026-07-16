// TCPlayerState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "TCPlayerState.generated.h"

/**
 * ATCPlayerState - 로비 복제 상태를 담는 PlayerState.
 *
 * 명세(세션 흐름) 4단계의 bIsReady(준비 상태) 복제를 담당한다.
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

	// GameMode 가 PostLogin 때 배정하는 로비 슬롯(0~3). UI 슬롯 위젯 매핑용.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Lobby")
	int32 GetLobbySlotIndex() const { return LobbySlotIndex; }

	// 선택된 캐릭터 외형(스킨) 인덱스. O_CharacterSelect 에서 선택.
	// 주의(3단계): 값 복제/조회만 제공하며, 실제 폰 외형 적용 훅은 이번 단계에서 의도적으로 생략했다.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Lobby")
	int32 GetCharacterIndex() const { return CharacterIndex; }

	// ColorIndex 추가
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Color")
	int32 GetColorIndex() const { return ColorIndex; }

	// ── 쓰기(서버 권위 전용) ──
	// HasAuthority() 가드 내장. 클라가 직접 부르면 무시된다(로그만).

	void SetReadyAuthoritative(bool bInReady);
	void SetLobbySlotIndexAuthoritative(int32 InSlot);
	void SetCharacterIndexAuthoritative(int32 InCharacterIndex);

	// ── 스테이지 맵 진입 전원 대기 게이트(서버 전용 북키핑, 로딩 화면 동기화 수정) ──
	// bIsReady와 달리 다른 클라이언트 UI에 보여줄 필요가 없고 서버만 알면 되므로 복제하지 않는다.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Loading")
	bool HasLoadedCurrentMap() const { return bHasLoadedCurrentMap; }

	void SetHasLoadedCurrentMapAuthoritative(bool bInLoaded);

	void SetColorIndexAuthoritative(int32 InColorIndex);

protected:
	// 준비 완료 플래그.
	UPROPERTY(ReplicatedUsing = OnRep_LobbyInfo, VisibleAnywhere, Category = "TeamCarry|Lobby")
	bool bIsReady = false;

	// 로비 슬롯 인덱스(서버 배정).
	UPROPERTY(ReplicatedUsing = OnRep_LobbyInfo, VisibleAnywhere, Category = "TeamCarry|Lobby")
	int32 LobbySlotIndex = -1;

	// 캐릭터 외형(스킨) 인덱스(클라 선택, 서버 권위 확정).
	UPROPERTY(ReplicatedUsing = OnRep_LobbyInfo, VisibleAnywhere, Category = "TeamCarry|Lobby")
	int32 CharacterIndex = 0;

	// ColorIndex 변수
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "TeamCarry|Color")
	int32 ColorIndex = -1;

	UFUNCTION()
	void OnRep_LobbyInfo();

	// 서버 전용 북키핑(비복제). ATeamCarryGameMode::PostLogin/HandleSeamlessTravelPlayer 가 스테이지
	// 맵 진입 시마다 false 로 리셋하고, ATCPlayerController::ServerReportMapLoaded() 가 true 로 설정한다.
	bool bHasLoadedCurrentMap = false;

private:
	// 복제값 변경 시 GameState 에 로비 변경을 알려 UI 갱신을 트리거한다(서버/클라 공통).
	void NotifyLobbyChanged();
};
