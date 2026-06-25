// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Level/Struct/BreakableProp.h"
#include "DamageableWall.generated.h"

class UMaterialInterface;

/**
 * 
 */
UCLASS()
class TEAMCARRY_API ADamageableWall : public ABreakableProp
{
	GENERATED_BODY()

public:
	ADamageableWall();
protected:
	UPROPERTY(EditAnywhere, Category = "Wall")
	TArray<TObjectPtr<UMaterialInterface>> DamageMaterials;

	virtual void OnDamageChanged(int32 NewLevel) override;
};

