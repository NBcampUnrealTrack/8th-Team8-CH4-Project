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

	// 튜토리얼 완료 여부(명세 4장-6, 5장). true 가 되면 같은 방의 다음 HostStartGame() 은
	// 이어하기(스테이지 직행) 경로를 탄다.
	UPROPERTY(BlueprintReadOnly)
	bool bTutorialCompleted = false;
};