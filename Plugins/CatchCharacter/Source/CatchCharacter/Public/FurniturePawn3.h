// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CatchCharacter/Public/FurnitureDataTable.h"
#include "FurniturePawn3.generated.h"

class UStaticMeshComponent;
class UFurnitureStat;
class ACharacter;

UCLASS()
class CATCHCHARACTER_API AFurniturePawn3 : public APawn
{
	GENERATED_BODY()

public:
	AFurniturePawn3();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버에서 실행될 상호작용 함수
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Grab(ACharacter* Grabber, UPrimitiveComponent* GrabberComponent);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Release(ACharacter* Grabber);

protected:

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

private:
	// 잡았을 때의 초기 오프셋을 저장 (위치 유지 및 소켓 부착 지원)
	struct FGrabInfo
	{
		UPrimitiveComponent* GrabberComponent;
		FVector RelativeLocation; // 가구 중심 - 컴포넌트 위치
		FRotator RelativeRotation;
	};

	TMap<ACharacter*, FGrabInfo> GrabInfos;

	// 이동 로직 처리 (서버 전용)
	void HandleMovement(float DeltaTime);

	// 잡고 있는 플레이어들의 위치를 가구에 맞게 강제 보정 (서버 전용)
	void SyncPlayersToFurniture();

public:
	UFurnitureStat* GetFurnitureStat() const { return FurnitureStat; }
};
