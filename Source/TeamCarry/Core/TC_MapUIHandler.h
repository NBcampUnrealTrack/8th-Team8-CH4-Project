// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TC_MapUIHandler.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UTC_MapUIHandler : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class TEAMCARRY_API ITC_MapUIHandler
{
	GENERATED_BODY()

	
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Map")
	void OpenStageMap();
};
