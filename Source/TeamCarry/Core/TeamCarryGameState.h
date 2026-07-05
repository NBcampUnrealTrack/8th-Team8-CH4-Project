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

	// 남은 가구 개수
	UPROPERTY(ReplicatedUsing = OnRep_RemainingFurniture, BlueprintReadOnly)
	int32 RemainingFurniture;

	// 스톱워치 경과 시간 (초)
	UPROPERTY(Replicated, BlueprintReadOnly)
	float ElapsedTime;

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
	void OnRep_StarCount();

	UFUNCTION()
	void OnRep_CurrentPhase();

	// 접속 로그 항목(입장/퇴장 등). 항상 뒤에 추가만 되고 삭제/재정렬되지 않는다.
	UPROPERTY(ReplicatedUsing = OnRep_SessionLogEntries, BlueprintReadOnly)
	TArray<FText> SessionLogEntries;

	// 이전 값(OldSessionLogEntries) 대비 새로 추가된 항목만 골라 OnSessionLogAdded 로 Broadcast.
	UFUNCTION()
	void OnRep_SessionLogEntries(const TArray<FText>& OldSessionLogEntries);
};