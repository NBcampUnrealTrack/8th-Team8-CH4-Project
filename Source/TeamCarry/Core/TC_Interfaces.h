#pragma once

#include "CoreMinimal.h"
#include "TC_DataTypes.h"
#include "TC_Interfaces.generated.h"

// GameMode 정산 인터페이스
UINTERFACE(MinimalAPI, BlueprintType)
class UScorableInterface : public UInterface
{
	GENERATED_BODY()
};

class IScorableInterface
{
	GENERATED_BODY()

public:
	// 가구가 트럭에 실렸을 때 GameMode에 정산 요청
	UFUNCTION(BlueprintNativeEvent)
	void OnFurnitureLoaded(FName RowName, float CurrentHealth, float MaxHealth, int32 BaseScore);
};

// GameMode → UI 점수 갱신 인터페이스
UINTERFACE(MinimalAPI, BlueprintType)
class UScoreObserverInterface : public UInterface
{
	GENERATED_BODY()
};

class IScoreObserverInterface
{
	GENERATED_BODY()

public:
	// 점수 갱신 시 UI에 알림
	UFUNCTION(BlueprintNativeEvent)
	void OnMoneyUpdated(int32 NewTotalMoney);

	// 스테이지 종료 시 UI에 알림
	UFUNCTION(BlueprintNativeEvent)
	void OnStageFinished(FStageResult Result);
};