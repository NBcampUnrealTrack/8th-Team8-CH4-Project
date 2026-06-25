// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CatchCharacter/Furniture/FurnitureActor.h"
#include "Player/Interface/TCInteractable.h"
#include "TCFurnitureActor.generated.h"

/**
 * 
 */
UCLASS()
class TEAMCARRY_API ATCFurnitureActor : public AFurnitureActor, public ITCInteractable
{
    GENERATED_BODY()

public:
    virtual bool CanInteract_Implementation(ATCPlayerCharacter* Player) override;
    virtual void OnInteract_Implementation(ATCPlayerCharacter* Player) override;
    virtual void OnFocus_Implementation() override;
    virtual void OnUnfocus_Implementation() override;

};
