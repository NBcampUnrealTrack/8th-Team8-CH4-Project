// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/MovableFurniture.h"
#include "TCStubFurniture.generated.h"

class UStaticMeshComponent;

UCLASS()
class TEAMCARRY_API ATCStubFurniture : public AActor, public IMovableFurniture
{
	GENERATED_BODY()
	
public:
	ATCStubFurniture();
	virtual bool IsGrabbed_Implementation() const override { return bIsGrabbed; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Furniture")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, Category = "Furniture")
	bool bIsGrabbed = false;
};