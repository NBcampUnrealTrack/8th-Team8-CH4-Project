// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TC_UsableItem.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UTC_UsableItem : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class TEAMCARRY_API ITC_UsableItem
{
	GENERATED_BODY()

public:
	// 들고 있는 아이템 사용 (F키 → GrabComponent의 ServerTryThrow에서 호출)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item")
	void OnUse(AActor* User);
};
