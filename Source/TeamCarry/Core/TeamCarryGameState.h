#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TC_DataTypes.h"
#include "TeamCarryGameState.generated.h"

UCLASS()
class TEAMCARRY_API ATeamCarryGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ATeamCarryGameState();

	// 팀 누적 점수
	UPROPERTY(ReplicatedUsing = OnRep_TotalScore, BlueprintReadOnly)
	int32 TotalScore;

	// 이 스테이지에서 획득 가능한 전체 목표 값어치(가구 BaseScore 합산). 스테이지 시작 시
	// GameMode가 1회 산정해 복제하며, 스테이지 중 불변이다. S_InGame의 팀 값어치 게이지(PB_TeamMoney)의
	// Max 값으로 쓰인다(UI_Technical_Spec.md 4장-7).
	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 TotalLevelValue;

	// 남은 가구 개수(이동 가능한 개수 — 트럭 적재/파괴 시 감소)
	UPROPERTY(ReplicatedUsing = OnRep_RemainingFurniture, BlueprintReadOnly)
	int32 RemainingFurniture;

	// 스톱워치 경과 시간 (초) — UI에 표시되는 값. Playing 단계에서 매 프레임 증가한다.
	UPROPERTY(Replicated, BlueprintReadOnly)
	float ElapsedTime;

	// 파괴된 가구 개수 — OnFurnitureDestroyed 호출 횟수. 핫타임 비율 계산에 사용.
	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 DestroyedFurnitureCount;

	// 핫타임 활성 여부 — TCFeedbackSubsystem 이 트럭 비율 기준으로 설정.
	// TCFeedbackComponent 가 가구 아웃라인(빨간 링) 표시 여부 판단에 사용한다.
	UPROPERTY(ReplicatedUsing = OnRep_bIsHotTime, BlueprintReadOnly)
	bool bIsHotTime;

	// 게임 종료 여부
	UPROPERTY(ReplicatedUsing = OnRep_bIsGameFinished, BlueprintReadOnly)
	bool bIsGameFinished;
	
	// 현재 게임 단계
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase, BlueprintReadOnly)
	EGamePhase CurrentPhase;
	
	// 별 개수
	UPROPERTY(ReplicatedUsing = OnRep_StarCount, BlueprintReadOnly)
	int32 StarCount;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 리슨 서버(호스트)는 자기 자신에게 OnRep이 호출되지 않으므로,
	// GameMode가 서버 권한으로 값을 직접 수정한 직후 해당 OnRep을 수동으로 호출할 수 있도록 허용한다.
	friend class ATeamCarryGameMode;

	// 리슨 서버 호스트용 수동 갱신 함수. bIsHotTime을 서버 권한으로 직접 수정한 직후
	// 호출하여 로컬(호스트 자신) UI/연출을 즉시 갱신한다. (OnRep은 protected로 유지)
	void NotifyHotTimeChanged() { OnRep_bIsHotTime(); }

	// ── 접속 로그(명세 3장·4장-7·7장-4) ──
	// 서버 권위 전용. GameMode 의 PostLogin/Logout 이 호출한다. 새로 추가된 항목만
	// UMockUIController::OnSessionLogAdded 로 Broadcast 한다(OnRep_SessionLogEntries 에서 처리).
	void AddSessionLogEntry(const FText& NewEntry);

protected:
	UFUNCTION()
	void OnRep_TotalScore();

	UFUNCTION()
	void OnRep_RemainingFurniture();

	UFUNCTION()
	void OnRep_bIsGameFinished();

	UFUNCTION()
	void OnRep_bIsHotTime();

	UFUNCTION()
	void OnRep_StarCount();

	UFUNCTION()
	void OnRep_CurrentPhase();

	// 접속 로그 항목(입장/퇴장 등). 항상 뒤에 추가만 되고 삭제/재정렬되지 않는다.
	UPROPERTY(ReplicatedUsing = OnRep_SessionLogEntries, BlueprintReadOnly)
	TArray<FText> SessionLogEntries;

	// 이전 값(OldSessionLogEntries) 대비 새로 추가된 항목만 골라 OnSessionLogAdded 로 Broadcast.
	UFUNCTION()
	void OnRep_SessionLogEntries(const TArray<FText>& OldSessionLogEntries);

private:
	// MockUIController 캐시 (OnRep 함수마다 GetSubsystem 호출 방지)
	UPROPERTY()
	mutable class UMockUIController* CachedMockController = nullptr;

	// MockUIController 캐시 가져오기
	UMockUIController* GetCachedMockController() const;
};
