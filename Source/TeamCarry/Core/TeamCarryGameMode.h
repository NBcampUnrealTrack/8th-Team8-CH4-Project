#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TC_DataTypes.h"
#include "TC_Interfaces.h"
#include "TeamCarryGameState.h"
#include "StageClearProvider.h"
#include "StageHost.h"
#include "TeamCarryGameMode.generated.h"

// 트럭 안 가구 정보 구조체
struct FTruckFurnitureInfo
{
	FName RowName;
	float CurrentHealth;
	float MaxHealth;
	int32 BaseScore;
};

UCLASS()
class TEAMCARRY_API ATeamCarryGameMode : public AGameModeBase, public IStageClearProvider, public IStageHost
{
	GENERATED_BODY()

public:
	ATeamCarryGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	
	// IStageClearProvider
	virtual bool IsStageCleared() const override;
	virtual int32 GetDeliveredCount() const override;
	virtual int32 GetTargetCount() const override;

	// IStageHost
	virtual void OnStageCleared_Implementation() override;

	// 가구 트럭 진입 시 호출
	UFUNCTION(BlueprintCallable)
	void OnFurnitureEnterTruck(FName RowName, float CurrentHealth, float MaxHealth, int32 BaseScore);

	// 가구 트럭 이탈 시 호출
	UFUNCTION(BlueprintCallable)
	void OnFurnitureExitTruck(FName RowName);

	// 스테이지 시작 시 옮겨야 할 가구 개수 설정
	UFUNCTION(BlueprintCallable)
	void SetTotalFurnitureCount(int32 Count);
	
	// 게임 단계 전환
	UFUNCTION(BlueprintCallable)
	void SetGamePhase(EGamePhase NewPhase);

	// 카운트다운 시작
	void StartCountdown();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float StarThreeTime = 180.0f; // 기본 3분

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float StarTwoTime = 600.0f; // 기본 10분

protected:
	// 점수 계산
	int32 CalculateScore(float CurrentHealth, float MaxHealth, int32 BaseScore);

	// 게임 종료 처리
	void FinishGame(bool bIsClear);
	
	int32 CalculateStar(float ElapsedTime);

private:
	// 총 옮겨야 할 가구 개수
	int32 TotalFurnitureCount;

	// 트럭 안 가구 목록
	TArray<FTruckFurnitureInfo> FurnitureInTruck;

	// 최종 점수 계산
	int32 CalculateFinalScore();
	
	// 카운트다운 남은 시간
	float CountdownTime;

	// 카운트다운 타이머 핸들
	FTimerHandle CountdownTimerHandle;
};