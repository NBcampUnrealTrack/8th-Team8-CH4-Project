// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Player/Interface/TCInteractable.h"
#include "TCMovingTruck.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class ATCPlayerCharacter;

UCLASS()
class TEAMCARRY_API ATCMovingTruck : public APawn, public ITCInteractable
{
	GENERATED_BODY()

public:
	ATCMovingTruck();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 빙의 콜백 — 운전 IMC on/off
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;

	// ITCInteractable — 운전석 탑승
	virtual bool CanInteract_Implementation(ATCPlayerCharacter* Player) override;
	virtual void OnFocus_Implementation() override;
	virtual void OnUnfocus_Implementation() override;
	virtual void OnInteract_Implementation(ATCPlayerCharacter* Player) override;

	// 캐릭터(리스폰)가 읽어갈 짐칸 스폰포인트
	UFUNCTION(BlueprintCallable, Category = "Truck")
	FTransform GetCargoSpawnTransform(int32 Index) const;

	UFUNCTION(BlueprintCallable, Category = "Truck")
	int32 GetCargoSpawnCount() const { return CargoSpawnPoints.Num(); }

protected:
	virtual void BeginPlay() override;

	// 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = "Truck")
	TObjectPtr<UStaticMeshComponent> TruckBody;

	UPROPERTY(VisibleAnywhere, Category = "Truck")
	TObjectPtr<UStaticMeshComponent> CargoFloor;

	UPROPERTY(VisibleAnywhere, Category = "Truck")
	TObjectPtr<USceneComponent> DriverSeat;

	UPROPERTY(VisibleAnywhere, Category = "Truck")
	TObjectPtr<UBoxComponent> DriverInteractVolume;

	UPROPERTY(VisibleAnywhere, Category = "Truck")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Truck")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Truck")
	TArray<TObjectPtr<USceneComponent>> CargoSpawnPoints;

	// 입력
	UPROPERTY(EditDefaultsOnly, Category = "Truck|Input")
	TObjectPtr<UInputMappingContext> DriveMappingContext;
	UPROPERTY(EditDefaultsOnly, Category = "Truck|Input")
	TObjectPtr<UInputAction> ThrottleAction;
	UPROPERTY(EditDefaultsOnly, Category = "Truck|Input")
	TObjectPtr<UInputAction> SteerAction;
	UPROPERTY(EditDefaultsOnly, Category = "Truck|Input")
	TObjectPtr<UInputAction> ExitAction;

	UPROPERTY(EditAnywhere, Category = "Truck|Movement")
	float MaxSpeed = 1200.f;
	UPROPERTY(EditAnywhere, Category = "Truck|Movement")
	float Acceleration = 1500.f;
	UPROPERTY(EditAnywhere, Category = "Truck|Movement")
	float BrakeDecel = 2000.f;
	UPROPERTY(EditAnywhere, Category = "Truck|Movement")
	float TurnRateAtSpeed = 60.f;

	UPROPERTY(ReplicatedUsing = OnRep_Driver)
	TObjectPtr<ATCPlayerCharacter> CurrentDriver = nullptr;

private:
	// 입력 핸들러
	void OnThrottle(const FInputActionValue& V);
	void OnSteer(const FInputActionValue& V);
	void OnExit(const FInputActionValue& V);

	// 입력값을 서버로
	UFUNCTION(Server, Unreliable)
	void ServerSetInput(float InThrottle, float InSteer);

	// 하차 (서버 권위)
	UFUNCTION(Server, Reliable)
	void ServerExit();

	// 탑승 (서버 — OnInteract에서 호출, 이미 서버 컨텍스트)
	void ServerEnter(ATCPlayerCharacter* Player);

	UFUNCTION() 
	void OnRep_Driver();

	void AddDriveMappingContextFor(AController* InController, bool bAdd);

	float CurrentThrottle = 0.f;
	float CurrentSteer = 0.f;
	float CurrentSpeed = 0.f;
};