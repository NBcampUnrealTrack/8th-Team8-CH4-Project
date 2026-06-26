// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TCIDamageable.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UTCIDamageable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class TEAMCARRY_API ITCIDamageable
{
	GENERATED_BODY()

public:
	// 직접 데미지 (방망이 등 — 물리 충돌과 별개 경로)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damage")
	void ApplyDamage(int32 Amount, AActor* Instigator);
};
