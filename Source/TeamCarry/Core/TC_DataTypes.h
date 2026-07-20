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
	EFurnitureGrade Grade = EFurnitureGrade::Normal;    // 가구 등급

	UPROPERTY(BlueprintReadOnly)
	int32 FinalScore = 0;           // 최종 지급 점수
};

// 스테이지 결과 구조체
USTRUCT(BlueprintType)
struct FStageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 TotalMoney = 0;           // 최종 보유

	UPROPERTY(BlueprintReadOnly)
	float ElapsedTime = 0.0f;       // 소요 시간 (초)

	UPROPERTY(BlueprintReadOnly)
	bool bIsClear = false;          // 클리어 여부
};

// 게임 진행 단계
UENUM(BlueprintType)
enum class EGamePhase : uint8
{
	WaitingToStart  UMETA(DisplayName = "WaitingToStart"),  // 대기 중
	Countdown       UMETA(DisplayName = "Countdown"),       // 카운트다운
	Playing         UMETA(DisplayName = "Playing"),         // 게임 진행 중
	Result          UMETA(DisplayName = "Result")           // 결과창
};

// 스테이지별 게임 설정 DataTable 행 구조체
// DT_StageConfig 에 맵 이름을 키로 등록해 스테이지마다 값을 다르게 설정한다.
USTRUCT(BlueprintType)
struct FStageConfig : public FTableRowBase
{
	GENERATED_BODY()

	// 게임 제한시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TimeLimitSeconds = 300.0f; // 기본 5분

	// 핫타임 시작 경과시간 (초) — 이 시간 이후 트럭 비율 20% 이하면 핫타임
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float HotTimeElapsedThreshold = 180.0f; // 기본 3분

	// 전체 목표 값어치 (UI 게이지 Max 값)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 TotalLevelValue = 10000;
};