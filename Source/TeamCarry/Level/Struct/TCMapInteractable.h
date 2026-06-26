// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/Interface/TCInteractable.h"
#include "TCMapInteractable.generated.h"

class UStaticMeshComponent;
class ATCPlayerCharacter;

UCLASS()
class TEAMCARRY_API ATCMapInteractable : public AActor, public ITCInteractable
{
	GENERATED_BODY()
	
public:
	ATCMapInteractable();

	// ITCInteractable
	virtual bool CanInteract_Implementation(ATCPlayerCharacter* Player) override;
	virtual void OnFocus_Implementation() override;
	virtual void OnUnfocus_Implementation() override;
	virtual void OnInteract_Implementation(ATCPlayerCharacter* Player) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Map")
	TObjectPtr<UStaticMeshComponent> Mesh;  
};