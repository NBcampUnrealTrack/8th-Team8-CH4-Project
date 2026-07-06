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

	// ── 방 코드 (호스트 GameInstance 에만 있던 값을 클라에도 복제) ──
	// 호스트만 HostRoomCode 를 갖고 있어 클라 UI 가 "오프라인"으로 뜨던 문제 해결.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "TeamCarry|Lobby")
	FString GetRoomCode() const { return RoomCode; }

	// 서버 권위 전용(GameMode 가 로비 시작 시 주입).
	void SetRoomCodeAuthoritative(const FString& InCode);

	// ── 선택된 스테이지(호스트가 O_StageSelect 에서 확정, 참가자는 로비에서 조회만) ──
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Lobby")
	int32 GetSelectedStageId() const { return SelectedStageId; }

	// 서버 권위 전용(UTCSessionFlow::SetStageSelection 이 주입).
	void SetSelectedStageIdAuthoritative(int32 InStageId);

	// ── 접속 로그(명세 3장·4장-7·7장-4) ──
	// 서버 권위 전용. GameMode 의 PostLogin/Logout 이 호출한다. 새로 추가된 항목만
	// UMockUIController::OnSessionLogAdded 로 Broadcast 한다(OnRep_SessionLogEntries 에서 처리).
	void AddSessionLogEntry(const FText& NewEntry);

protected:
	// 세션 방 코드. 호스트가 set, 클라는 복제 수신.
	UPROPERTY(ReplicatedUsing = OnRep_RoomCode, VisibleAnywhere, Category = "TeamCarry|Lobby")
	FString RoomCode;

	// 클라에 코드 도착 → UI 갱신 트리거(위젯이 OnLobbyPlayersChanged 를 이미 구독 중).
	UFUNCTION()
	void OnRep_RoomCode();

	// 방장이 O_StageSelect 에서 확정한 스테이지 ID(0 = 미선택 → 참가자 표시상으로도 기본 1스테이지 취급은
	// UI 쪽에서 UTCSessionFlow::GetSelectedStageId() 의 폴백 규칙을 따른다).
	UPROPERTY(ReplicatedUsing = OnRep_SelectedStageId, VisibleAnywhere, Category = "TeamCarry|Lobby")
	int32 SelectedStageId = 0;

	UFUNCTION()
	void OnRep_SelectedStageId();

	// 접속 로그 항목(입장/퇴장 등). 항상 뒤에 추가만 되고 삭제/재정렬되지 않는다.
	UPROPERTY(ReplicatedUsing = OnRep_SessionLogEntries, VisibleAnywhere, Category = "TeamCarry|Lobby")
	TArray<FText> SessionLogEntries;

	// 이전 값(OldSessionLogEntries) 대비 새로 추가된 항목만 골라 OnSessionLogAdded 로 Broadcast.
	UFUNCTION()
	void OnRep_SessionLogEntries(const TArray<FText>& OldSessionLogEntries);
};
