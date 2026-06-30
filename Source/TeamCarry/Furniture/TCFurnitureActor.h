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

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void DestroyFuniture();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_DestroyFurniture();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
    TObjectPtr<UStaticMesh> BrokenMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
    TObjectPtr<USoundBase> BreakSound;

};
