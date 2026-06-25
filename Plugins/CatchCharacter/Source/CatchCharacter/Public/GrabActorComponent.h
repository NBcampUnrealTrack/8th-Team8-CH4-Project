// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "GrabActorComponent.generated.h"

class ACharacter;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CATCHCHARACTER_API UGrabActorComponent : public USphereComponent
{
	GENERATED_BODY()

public:	
	UGrabActorComponent();

	// 입력 이벤트에서 호출될 함수
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ToggleGrab();

protected:
	// --- Server RPCs ---
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRPCGrab(AActor* Furniture);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRPCRelease();

public:
	// 서버가 계산한 위치를 클라이언트에게 강제 주입하는 RPC (폐기 예정, 하위 호환 유지)
	UFUNCTION(Client, Unreliable)
	void ClientRPC_ForcePosition(FVector TargetLocation, FRotator TargetRotation);


protected:
	// 현재 잡고 있는 가구 (두 버전 모두 지원하기 위해 AActor로 변경)
	UPROPERTY()
	AActor* HeldFurniture;
		
};
