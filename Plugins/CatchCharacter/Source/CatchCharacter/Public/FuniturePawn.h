// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FurnitureDataTable.h"
#include "Net/UnrealNetwork.h"
#include "FuniturePawn.generated.h"

class UStaticMeshComponent;
class UPhysicsConstraintComponent;
class ACharacter;
class UFurnitureStat;

UCLASS()
class CATCHCHARACTER_API AFuniturePawn : public APawn
{
	GENERATED_BODY()

public:
	AFuniturePawn();

	virtual void Tick(float DeltaTime) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버에서 실행될 상호작용 함수
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Grab(ACharacter* Grabber, UPrimitiveComponent* GrabberComponent);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Release(ACharacter* Grabber);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* FurnitureMesh;

	// 스탯 관리를 위한 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UFurnitureStat* FurnitureStat;

	// --- 설정 데이터 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Setup")
	FDataTableRowHandle FurnitureDataRow;

	// --- 상태 변수 (Replicated) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_GrabbedPlayers, Category = "Furniture|State")
	TArray<ACharacter*> GrabbedPlayers;

	UFUNCTION()
	void OnRep_GrabbedPlayers();

	// 서버 전용: 컨스트레인트 관리
	UPROPERTY()
	TMap<ACharacter*, UPhysicsConstraintComponent*> ActiveConstraints;

	// 물리 상태 업데이트 (스탯 컴포넌트의 데이터를 기반으로 함)
	void UpdatePhysicsState();

	// 컨스트레인트 세부 설정 (무게나 상태에 따라 조절 가능하도록 분리)
	void ConfigureConstraint(UPhysicsConstraintComponent* Constraint);

public:
	UFurnitureStat* GetFurnitureStat() const { return FurnitureStat; }
};
