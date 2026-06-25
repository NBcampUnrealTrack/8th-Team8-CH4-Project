// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BreakableProp.h"
#include "BreakableWindow.generated.h"

class USoundBase;

UCLASS()
class TEAMCARRY_API ABreakableWindow : public ABreakableProp
{
	GENERATED_BODY()
	
public:
	ABreakableWindow();
protected:
	UPROPERTY(EditAnywhere, Category = "Window")
	TObjectPtr<UStaticMesh> BrokenMesh;

	UPROPERTY(EditAnywhere, Category = "Window")
	TObjectPtr<USoundBase> BreakSound;

	virtual void OnDamageChanged(int32 NewLevel) override;
};
