#pragma once

#include "CoreMinimal.h"
#include "TC_DataTypes.generated.h"

// 가구 등급
UENUM(BlueprintType)
enum class EFurnitureGrade : uint8
{
	Normal  UMETA(DisplayName = "Normal"),
	Rare    UMETA(DisplayName = "Rare"),
	Epic    UMETA(DisplayName = "Epic")
};

// 가구 정산 결과 구조체
USTRUCT(BlueprintType)
struct FFurnitureScoreResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName FurnitureRowName;         // 가구 DataTable RowName

	UPROPERTY(BlueprintReadOnly)
	EFurnitureGrade Grade;          // 가구 등급

	UPROPERTY(BlueprintReadOnly)
	int32 FinalScore;               // 최종 지급 점수($)
};

// 스테이지 결과 구조체
USTRUCT(BlueprintType)
struct FStageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 TotalMoney;               // 최종 보유 $

	UPROPERTY(BlueprintReadOnly)
	float ElapsedTime;              // 소요 시간 (초)

	UPROPERTY(BlueprintReadOnly)
	bool bIsClear;                  // 클리어 여부
};