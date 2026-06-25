// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TCStunnable.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UTCStunnable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class TEAMCARRY_API ITCStunnable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Stun")
	void ReceiveStun(float Duration, AActor* Instigator);
};
