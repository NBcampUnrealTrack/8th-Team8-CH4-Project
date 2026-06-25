// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/DataTable.h"
#include "FurnitureDataTable.generated.h"

USTRUCT(BlueprintType)
struct FFurnitureData : public FTableRowBase
{
    GENERATED_BODY()

public:
	// 가구의 최대체력
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHealth = 100.f;

	// 안정적으로 옳기는데 필요 플레이어수
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequiredPlayer = 1;

	// 들어올릴시 플레이어의 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BaseSpeed = 100.f;

	// 부딫칠시 가구에 입혀지는 데미지 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CollisionDamageMultiplier = 10.f;

	// 가구 무게 (F=ma 계산용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Mass = 200.f;

	// 마찰력 (감속 비율)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Friction = 4.f;
};
