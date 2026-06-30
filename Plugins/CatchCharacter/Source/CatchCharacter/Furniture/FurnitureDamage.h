// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FurnitureDamage.generated.h"

class UFurnitureStat;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CATCHCHARACTER_API UFurnitureDamage : public UActorComponent
{
	GENERATED_BODY()

public:	
	UFurnitureDamage();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Setup(UFurnitureStat* InStat);
public:
	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);
protected:

	// --- 데미지 처리 변수 ---

	// 이정도의 충력량이 나와야 데미지 입음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Damage")
	float MinImpactSpeedForDamage = 50.0f;
	// 데미지 계산시 데미지 배율을 그대로 함녀 너무크니 보정용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Damage")
	float DamagePerImpactSpeed = 0.05f;

	// --- 충격량 계산을 위한 변수------
	// 어차피 서버에서 연산할거라 복제는 필요없음
	FVector PreviousLocation;
	FVector CurrentVelocity;
	UPROPERTY()
	UFurnitureStat* FurnitureStat;

private:
	void CalculateVelocity(float DeltaTime);
};
