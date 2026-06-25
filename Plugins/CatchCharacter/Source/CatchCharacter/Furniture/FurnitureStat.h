// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FurnitureDataTable.h"
#include "FurnitureStat.generated.h"

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

	// getter
	const FFurnitureData& GetFurnitureData() const { return DefaultStats; }
	int32 GetCurrentGrabbedPlayer() const { return CurrentGrabbedPlayer; }
	float GetBaseSpeed() const { return BaseSpeed; }
	float GetCurrentHealth() const { return CurrentHealth; }
	int32 GetGrabbedPlayerNum() const { return CurrentGrabbedPlayer; }
	int32 GetRequiredPlayer() const { return RequiredPlayer; }
	float GetMass() const { return Mass; }
	float GetFriction() const { return Friction; }

protected:
	virtual void BeginPlay() override;

	// --- 설정값 (서버에서 데이터 테이블로부터 주입받음) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture|State")
	FFurnitureData DefaultStats;

	// --- 런타임 상태 (Replicated) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Furniture|State")
	float CurrentHealth;

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
};
