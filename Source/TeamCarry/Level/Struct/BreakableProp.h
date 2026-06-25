// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakableProp.generated.h"

class UStaticMeshComponent;

UCLASS()
class TEAMCARRY_API ABreakableProp : public AActor
{
	GENERATED_BODY()
	
public:
	ABreakableProp();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Breakable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, Category = "Breakable")
	float ImpactImpulseThreshold = 40000.f;

	UPROPERTY(EditAnywhere, Category = "Breakable")
	int32 MaxDamageLevel = 1;

	UPROPERTY(ReplicatedUsing = OnRep_DamageLevel)
	int32 DamageLevel = 0;

	UFUNCTION() void OnRep_DamageLevel();

	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	virtual void OnDamageChanged(int32 NewLevel) {} // 서브클래스가 시각 반응

private:
	bool IsFurniture(AActor* OtherActor) const;
};