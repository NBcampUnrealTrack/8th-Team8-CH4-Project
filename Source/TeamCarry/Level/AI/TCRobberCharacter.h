// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/TCStunnable.h"
#include "TCRobberCharacter.generated.h"

class USceneComponent;

UENUM(BlueprintType)
enum class ERobberState : uint8
{
	Normal      UMETA(DisplayName = "Normal"),    // 평상시(배회/추적)
	Carrying    UMETA(DisplayName = "Carrying"),  // 짐 들고 도주
	Stunned     UMETA(DisplayName = "Stunned")    // 기절(래그돌)
};

UCLASS()
class TEAMCARRY_API ATCRobberCharacter : public ACharacter, public ITCStunnable
{
	GENERATED_BODY()

public:
	ATCRobberCharacter();

	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void ReceiveStun_Implementation(float Duration, AActor* Instigator) override;

	// BT Task가 호출할 움직임
	void GrabFurniture(AActor* Furniture);
	void ReleaseFurniture();

	
	// 추후 방망이 기능을 위한 코드 플레이어가 ApplyStun 호출
	UFUNCTION(BlueprintCallable, Category = "Robber")
	void ApplyStun(float Duration);

	// BT에서 강도의 상태를 확인
	UFUNCTION(BlueprintPure, Category = "Robber")
	ERobberState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Robber")
	bool IsStunned() const { return State == ERobberState::Stunned; }

	UFUNCTION(BlueprintPure, Category = "Robber")
	bool IsCarrying() const { return CarriedFurniture != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Robber")
	AActor* GetCarriedFurniture() const { return CarriedFurniture; }

protected:
	// 짐을 붙일 위치
	UPROPERTY(VisibleAnywhere, Category = "Robber")
	TObjectPtr<USceneComponent> CarrySocket;

	// 기절 지속 시간 기본값
	UPROPERTY(EditAnywhere, Category = "Robber")
	float DefaultStunDuration = 3.f;

	// 복제 상태
	UPROPERTY(ReplicatedUsing = OnRep_State)
	ERobberState State = ERobberState::Normal;

	UPROPERTY(ReplicatedUsing = OnRep_Carried)
	TObjectPtr<AActor> CarriedFurniture = nullptr;

	UFUNCTION() void OnRep_State();
	UFUNCTION() void OnRep_Carried();

private:
	// Attach/Detach 실제 처리
	void AttachFurniture(AActor* Furniture);
	void DetachFurniture(AActor* Furniture);

	// 스턴시 레그돌 방식 채택 각 클라에서 시각 처리
	void SetRagdoll(bool bEnable);

	// 기절 회복
	void RecoverFromStun();

	// Carrying/Normal의 상태 전환은 서버에서 자동 판정
	void UpdateCarryState();

	UPROPERTY() TObjectPtr<AActor> PrevCarried = nullptr;
	FTimerHandle StunTimer;

	// 래그돌 진입 시 캡슐/무브먼트 원복용 캐시
	FVector CachedMeshRelLocation;
	FRotator CachedMeshRelRotation;
};