// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CatchCharacter/Public/FurnitureDataTable.h"
#include "FurnitureActor.generated.h"

class UStaticMeshComponent;
class UFurnitureStat;
class UFurnitureGrabSystem;
class UFurnitureDamage;

UCLASS()
class CATCHCHARACTER_API AFurnitureActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AFurnitureActor();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* FurnitureMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UFurnitureStat* FurnitureStat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UFurnitureGrabSystem* GrabSystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UFurnitureDamage* DamageSystem;

	// --- 설정 데이터 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Setup")
	FDataTableRowHandle FurnitureDataRow;

	// 첫 그랩 시 가구를 살짝 들어올리는 오프셋 (인터페이스 어댑터의 Grab 호출에 넘김)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Setup")
	FVector GrabLiftHeight = FVector(0.0f, 0.0f, 40.0f);

public:
	virtual void Tick(float DeltaTime) override;

	UFurnitureGrabSystem* GetGrabSystem() const { return GrabSystem; }
	UFurnitureStat* GetFurnitureStat() const { return FurnitureStat; }

	// 외곽선 하이라이트 (현재는 더미데이터)
	UFUNCTION(BlueprintCallable, Category = "Furniture")
	void SetHighlight(bool bEnabled);
};
