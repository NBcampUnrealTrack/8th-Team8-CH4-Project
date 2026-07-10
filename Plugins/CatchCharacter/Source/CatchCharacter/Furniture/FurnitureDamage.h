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

	// --- 무적 (충돌 데미지 면역) ---

	// Duration초 동안 무적. 재호출 시 타이머 갱신 (서버 전용)
	UFUNCTION(BlueprintCallable, Category = "Furniture|Damage")
	void SetInvincible(float Duration);

	UFUNCTION(BlueprintCallable, Category = "Furniture|Damage")
	void SetSuperInvincible(bool Active);

	// 무적 즉시 해제 (서버 전용)
	UFUNCTION(BlueprintCallable, Category = "Furniture|Damage")
	void DisableInvincible();

	UFUNCTION(BlueprintPure, Category = "Furniture|Damage")
	bool IsInvincible() const { return bIsInvincible; }

protected:

	// --- 데미지 처리 관련 ---

	// 데미지로 인정할 최소 충돌 속도(cm/s). 무게와 무관한 필터
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Damage")
	float MinImpactSpeedForDamage = 50.0f;

	// 충돌 속도(cm/s) → 데미지 환산 계수. 질량은 데미지에 영향 없음 (가구별 배율로 조절)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Damage")
	float DamagePerImpactSpeed = 0.05f;

	// 물리 충돌(공중 낙하·던짐) 전용 데미지 배율. 운반 중 부딪침(스윕)은 영향 없음.
	// 던지기/높은 곳 낙하 충격을 운반 부딪침보다 세게 만들 때 사용. 1이면 동일.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Damage")
	float PhysicsImpactDamageScale = 1.5f;

	// 이 속도(cm/s) 이상의 '물리 충돌'은 무적 중이어도 데미지 관통.
	// 던지기는 놓기와 같은 0.5초 무적이 걸리는데(GrabComponent 수정 불가), 세게 던진 충격은
	// 이 값을 넘겨 즉시 등록됨 → 던짐 무적을 사실상 짧게 만든 효과. 살짝 놓기·잔접촉은 여전히 보호.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Damage")
	float InvincibilityBypassSpeed = 400.0f;

	// --- 충격량 측정용 변수 ---
	// 서버에서만 연산하므로 복제 필요 없음
	FVector PreviousLocation;
	FVector CurrentVelocity;

	// 회전(3축 각속도) 추적: 제자리 회전 시 끝단 충돌 속도(ω × r) 반영용
	// Yaw뿐 아니라 Pitch/Roll 회전(AddFurnitureOffset 기울이기 등)도 포함
	FQuat PreviousQuat = FQuat::Identity;
	FVector CurrentAngularVelocityRad = FVector::ZeroVector;   // 라디안/초, 축 방향 벡터
	UPROPERTY()
	UFurnitureStat* FurnitureStat;

	// --- 무적 상태 (서버에서만 판정하므로 복제 불필요) ---
	bool bIsInvincible = false;
	bool bIsSuperInvincible = false;

	FTimerHandle InvincibilityTimerHandle;

private:
	void CalculateVelocity(float DeltaTime);
};
