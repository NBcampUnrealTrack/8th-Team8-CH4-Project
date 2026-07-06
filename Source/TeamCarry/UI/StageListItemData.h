// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StageListItemData.generated.h"

/**
 * UStageListItemData - O_StageSelect 의 UListView 아이템(UObject 래퍼).
 *
 * UListView 는 UObject* 항목만 받을 수 있어, UTCSessionFlow::GetAllStageInfos() 가 반환하는
 * FStageInfo(USTRUCT) 를 화면에 뿌리기 위한 최소 래퍼로 사용한다.
 */
UCLASS(BlueprintType)
class TEAMCARRY_API UStageListItemData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "UI|StageSelect")
	int32 StageId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UI|StageSelect")
	FText DisplayName;
};
