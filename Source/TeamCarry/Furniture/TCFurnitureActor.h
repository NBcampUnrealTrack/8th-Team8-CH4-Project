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
    ATCFurnitureActor();

    virtual bool CanInteract_Implementation(ATCPlayerCharacter* Player) override;
    virtual void OnInteract_Implementation(ATCPlayerCharacter* Player) override;
    virtual void OnFocus_Implementation() override;
    virtual void OnUnfocus_Implementation() override;

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void DestroyFurniture();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_DestroyFurniture();

    // 가구 파괴용 메쉬
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Destruction")
    TObjectPtr<UGeometryCollectionComponent> GeometryCollectionComp;

    // 파괴 사운드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
    TObjectPtr<USoundBase> BreakSound;

    // 가구가 파괴되었는지 여부를 저장하는 플래그
    //UPROPERTY(Replicated) 서버에서만 처리하면되니 필요없을거라 판단.
    bool bIsFurnitureDestroyed = false;

};
