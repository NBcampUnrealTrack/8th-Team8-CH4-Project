// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FurnitureDataTable.h"
#include "FurnitureStat.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFurnitureDamage, float, MaxHealth , float, OldHealth, float, NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFurnitureDestroy);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CATCHCHARACTER_API UFurnitureStat : public UActorComponent
{
	GENERATED_BODY()

public:	
	UFurnitureStat();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 데이터 테이블 데이터를 기반으로 스탯 초기화
	void InitializeStats(const FFurnitureData& Data);

	// 잡기 인원 업데이트 및 물리 상태 영향력 계산
	void UpdateGrabbedPlayers(int32 Count);

	// 현재 인원수가 요구 인원을 충족하는지 확인
	bool IsRequirementMet() const { return CurrentGrabbedPlayer >= RequiredPlayer; }

	// 데미지 처리
	UFUNCTION()
	void TakeDamage(AActor* DamagedActor,
		float Damage,
		const UDamageType* DamageType,
		AController* Instigator,
		AActor* Causer
	);

	// getter
	const FFurnitureData& GetFurnitureData() const { return DefaultStats; }
	int32 GetCurrentGrabbedPlayer() const { return CurrentGrabbedPlayer; }
	float GetBaseSpeed() const { return BaseSpeed; }
	float GetCurrentHealth() const { return CurrentHealth; }
	float GetMaxHealth() const { return MaxHealth; }
	int32 GetGrabbedPlayerNum() const { return CurrentGrabbedPlayer; }
	int32 GetRequiredPlayer() const { return RequiredPlayer; }
	float GetCollisionDamageMultiplier() const { return CollisionDamageMultiplier; }
	float GetMass() const { return Mass; }
	float GetFriction() const { return Friction; }
	float GetPrice() const { return Price; }

	UPROPERTY(BlueprintAssignable)
	FOnFurnitureDamage OnFurnitureDamage;

	UPROPERTY(BlueprintAssignable)
	FOnFurnitureDestroy OnFurnitureDestroy;

protected:
	virtual void BeginPlay() override;

	// CurrentHealth 복제 콜백: 클라에서도 체력 변화를 감지해 OnFurnitureDamage를 브로드캐스트
	// (금 표시 등 시각 처리를 모든 머신에서 하기 위함). OldHealth = 복제 직전 값.
	UFUNCTION()
	void OnRep_CurrentHealth(float OldHealth);

	// --- 설정값 (서버에서 데이터 테이블로부터 주입받음) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture|State")
	FFurnitureData DefaultStats;

	// --- 런타임 상태 (Replicated) ---
	// ReplicatedUsing: 체력이 바뀌면 서버·클라 모두 콜백이 돌아 시각 처리를 통일
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentHealth, Category = "Furniture|State")
	float CurrentHealth;

	// 최대 체력. 금 표시 %(CurrentHealth/MaxHealth) 계산에 클라에서도 필요하므로 복제.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Furniture|State")
	float MaxHealth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Furniture|State")
	int32 RequiredPlayer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Furniture|State")
	float BaseSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Furniture|State")
	float CollisionDamageMultiplier;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Furniture|State")
	int32 CurrentGrabbedPlayer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Furniture|State")
	float Mass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Furniture|State")
	float Friction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Furniture|State")
	int32 Price;

};
