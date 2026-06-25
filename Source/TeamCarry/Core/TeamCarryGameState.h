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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_TotalScore();

	UFUNCTION()
	void OnRep_RemainingFurniture();

	UFUNCTION()
	void OnRep_bIsGameFinished();
};