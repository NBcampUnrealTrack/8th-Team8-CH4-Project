// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/Interface/TCInteractable.h"
#include "InteractableDoor.generated.h"

class UStaticMeshComponent;
class ATCPlayerCharacter;

UCLASS()
class TEAMCARRY_API AInteractableDoor : public AActor, public ITCInteractable
{
	GENERATED_BODY()
	
public:
	AInteractableDoor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract_Implementation(ATCPlayerCharacter* Player) override;
	virtual void OnFocus_Implementation() override;
	virtual void OnUnfocus_Implementation() override;
	virtual void OnInteract_Implementation(ATCPlayerCharacter* Player) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<USceneComponent> DoorRoot;

	// 경첩을 모서리에 맞춘 메시 — 직접 회전
	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(EditAnywhere, Category = "Door")
	FRotator ClosedRotation = FRotator(0.f, 0.f, 0.f);

	// 미는 문 = -90 (당기는 문이면 90)
	UPROPERTY(EditAnywhere, Category = "Door")
	FRotator OpenRotation = FRotator(0.f, -90.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "Door")
	float OpenSpeed = 120.f;

	UPROPERTY(ReplicatedUsing = OnRep_IsOpen)
	bool bIsOpen = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsInteracting)
	bool bIsInteracting = false;

private:
	bool bAnimating = false;

	UFUNCTION() void OnRep_IsOpen();
	UFUNCTION() void OnRep_IsInteracting();

	void HandleInteract();
	void StartAnimation();
};