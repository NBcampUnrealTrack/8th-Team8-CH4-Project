// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FurnitureGrabSystem.generated.h"


class UStaticMeshComponent;
class UFurnitureStat;
class ACharacter;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CATCHCHARACTER_API UFurnitureGrabSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	UFurnitureGrabSystem();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Grab(ACharacter* Grabber, FVector height, UPrimitiveComponent* GrabberComponent = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Release(ACharacter* Grabber);

	void Setup(UStaticMeshComponent* InMesh, UFurnitureStat* InStat);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsGrabbedBy(ACharacter* Player) const { return Player != nullptr && GrabbedPlayers.Contains(Player); }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool CanAcceptGrab() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	UStaticMeshComponent* FurnitureMesh;

	UPROPERTY()
	UFurnitureStat* FurnitureStat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_GrabbedPlayers, Category = "Furniture|State")
	TArray<ACharacter*> GrabbedPlayers;

	UFUNCTION()
	void OnRep_GrabbedPlayers();

	UPROPERTY(Replicated)
	FVector ServerLocation;

	UPROPERTY(Replicated)
	FRotator ServerRotation;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float ClientInterpSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float MaxGrabSeparationDistance = 300.0f;

	// 피동 플레이어를 끌어당기는 최대 속도 (cm/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float MaxCorrectionSpeed = 2000.0f;

	// 이보다 작은 위치 오차는 무시 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float CorrectionDeadzone = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float YawCorrectionDeadzone = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	bool bBlockedCarrierStopsFurniture = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float BlockStopThreshold = 1.0f;

private:
	struct FGrabAnchor
	{
		FVector InitialOffset        = FVector::ZeroVector;
		float   InitialFurnitureYaw  = 0.0f;
		float   InitialPlayerYaw     = 0.0f;
	};
	TMap<ACharacter*, FGrabAnchor> Anchors;

	// 서버: 그랩 전 MaxWalkSpeed 원본값 (Release 시 복원)
	TMap<ACharacter*, float> OriginalMaxWalkSpeeds;

	// 이전 틱에 피동(끌어당김) 상태였던 플레이어 집합
	// GetCurrentAcceleration()/bFurnitureMoving 대신 드래그 여부로 능동/피동 판별
	TSet<ACharacter*> DraggedLastTick;

	void HandleMovement(float DeltaTime);
	FVector GetAttachedLocation(ACharacter* Player, const FVector& FurnitureLoc, float FurnitureYaw) const;
	float   GetDesiredYaw(ACharacter* Player, float FurnitureYaw) const;

	// CarryVelocity: 피동=끌어당기는 속도, 정지=FVector::ZeroVector
	// 능동(직접 걷는 중)일 때는 Multicast를 보내지 않는다.
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ApplyPlayerCorrection(ACharacter* Player, FVector CarryVelocity, float TargetYaw);

	// --- 클라 보간 (가구) ---
	void UpdateClientInterpolation(float DeltaTime);
	FVector  PreviousClientLoc   = FVector::ZeroVector;
	FRotator PreviousClientRot   = FRotator::ZeroRotator;
	bool     bHasClientInterpInit = false;

	// 클라: 잡힌 플레이어 추적
	UPROPERTY()
	TArray<ACharacter*> ClientTrackedPlayers;

	// 클라: 로컬 플레이어 MaxWalkSpeed 원본값 (OnRep Release 시 복원)
	float LocalOriginalMaxWalkSpeed = 0.0f;
	bool  bLocalCMCModified         = false;
	bool  bLocalSpeedReduced        = false;

	void UpdateLocalWalkSpeed();
	void SetGrabCollisionState(ACharacter* Player, bool bEnable);
};
