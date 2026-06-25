#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CatchCharacter/Public/FurnitureDataTable.h"
#include "FurniturePawn2.generated.h"

class UStaticMeshComponent;
class UFurnitureStat;
class ACharacter;

UCLASS()
class CATCHCHARACTER_API AFurniturePawn2 : public APawn
{
	GENERATED_BODY()

public:
	AFurniturePawn2();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Grab(ACharacter* Grabber, FVector height, UPrimitiveComponent* GrabberComponent);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Release(ACharacter* Grabber);

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* FurnitureMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UFurnitureStat* FurnitureStat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Setup")
	FDataTableRowHandle FurnitureDataRow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_GrabbedPlayers, Category = "Furniture|State")
	TArray<ACharacter*> GrabbedPlayers;

	UFUNCTION()
	void OnRep_GrabbedPlayers();

	UPROPERTY(Replicated)
	FVector ServerLocation;

	UPROPERTY(Replicated)
	FRotator ServerRotation;

private:
	TMap<ACharacter*, FVector> PreviousPlayerLocations;
	TMap<ACharacter*, float> PreviousPlayerYaws;

	TMap<ACharacter*, FVector> InitialVectors;
	TMap<ACharacter*, float> InitialYaws;

	void HandleMovement(float DeltaTime);

	// 클라이언트 보간용
	FVector PreviousClientLoc;
	FRotator PreviousClientRot;

	void UpdateClientInterpolation(float DeltaTime);

public:
	UFurnitureStat* GetFurnitureStat() const { return FurnitureStat; }

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ForcePlayerPositionAndRotation(ACharacter* PlayerToTarget, FVector LocToSet, float YawToSet);
};
