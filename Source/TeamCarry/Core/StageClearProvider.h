// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "StageClearProvider.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UStageClearProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class TEAMCARRY_API IStageClearProvider
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual bool IsStageCleared() const = 0;
	virtual int32 GetDeliveredCount() const = 0;
	virtual int32 GetTargetCount() const = 0;
};
