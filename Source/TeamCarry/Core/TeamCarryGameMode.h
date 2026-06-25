#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TC_DataTypes.h"
#include "TC_Interfaces.h"
#include "TeamCarryGameState.h"
#include "TeamCarryGameMode.generated.h"

UCLASS()
class TEAMCARRY_API ATeamCarryGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATeamCarryGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// 가구가 트럭에 실렸을 때 호출 (홍민기님 가구에서 호출)
	UFUNCTION(BlueprintCallable)
	void OnFurnitureLoaded(FName RowName, float CurrentHealth, float MaxHealth, int32 BaseScore);

	// 스테이지 시작 시 옮겨야 할 가구 개수 설정
	UFUNCTION(BlueprintCallable)
	void SetTotalFurnitureCount(int32 Count);

protected:
	// 점수 계산
	int32 CalculateScore(float CurrentHealth, float MaxHealth, int32 BaseScore);

	// 승패 판정
	void CheckGameFinished();

	// 게임 종료 처리
	void FinishGame(bool bIsClear);

private:
	// 스톱워치 작동 여부
	bool bIsStopWatchRunning;

	// 총 옮겨야 할 가구 개수
	int32 TotalFurnitureCount;
};