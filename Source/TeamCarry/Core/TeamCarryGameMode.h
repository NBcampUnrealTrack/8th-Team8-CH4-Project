#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TC_DataTypes.h"
#include "TC_Interfaces.h"
#include "TeamCarryGameState.h"
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
class TEAMCARRY_API ATeamCarryGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATeamCarryGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// 가구 트럭 진입 시 호출
	UFUNCTION(BlueprintCallable)
	void OnFurnitureEnterTruck(FName RowName, float CurrentHealth, float MaxHealth, int32 BaseScore);

	// 가구 트럭 이탈 시 호출
	UFUNCTION(BlueprintCallable)
	void OnFurnitureExitTruck(FName RowName);

	// 스테이지 시작 시 옮겨야 할 가구 개수 설정
	UFUNCTION(BlueprintCallable)
	void SetTotalFurnitureCount(int32 Count);

	// 스테이지 제한 시간 (블루프린트에서 수정 가능)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float StageTotalTime = 600.0f; // 기본 10분

protected:
	// 점수 계산
	int32 CalculateScore(float CurrentHealth, float MaxHealth, int32 BaseScore);

	// 게임 종료 처리
	void FinishGame(bool bIsClear);

private:
	// 남은 시간
	float RemainingTime;

	// 총 옮겨야 할 가구 개수
	int32 TotalFurnitureCount;

	// 트럭 안 가구 목록
	TArray<FTruckFurnitureInfo> FurnitureInTruck;

	// 최종 점수 계산
	int32 CalculateFinalScore();
};