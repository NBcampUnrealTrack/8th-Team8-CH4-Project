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
};