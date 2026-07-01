#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TCSaveGame.generated.h"

// 스테이지별 저장 데이터
USTRUCT(BlueprintType)
struct FStageRecord
{
	GENERATED_BODY()

	// 클리어 여부
	UPROPERTY(BlueprintReadOnly)
	bool bIsCleared = false;

	// 최고 별 개수
	UPROPERTY(BlueprintReadOnly)
	int32 BestStar = 0;

	// 최고 점수
	UPROPERTY(BlueprintReadOnly)
	int32 BestScore = 0;
};

UCLASS()
class TEAMCARRY_API UTCSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// 스테이지별 기록 (Key: 스테이지 이름)
	UPROPERTY(BlueprintReadOnly)
	TMap<FString, FStageRecord> StageRecords;

	// 마지막으로 플레이한 스테이지
	UPROPERTY(BlueprintReadOnly)
	FString LastPlayedStage;
};