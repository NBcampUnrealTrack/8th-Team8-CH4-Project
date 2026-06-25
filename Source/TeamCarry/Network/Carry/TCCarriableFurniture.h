// TCCarriableFurniture.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Network/Carry/TCGrabbableInterface.h"
#include "Player/Interface/TCInteractable.h"
#include "TCCarriableFurniture.generated.h"

class UStaticMeshComponent;
class ATCPlayerCharacter;

// 운반 가능한 가구의 C++ 베이스.
// ITCGrabbable(운반 계약) + ITCInteractable(잡기 진입)을 모두 구현해
// GrabComponent의 잡기 흐름 → 운반자 등록/해제 → 서버 합산 물리까지 연결한다.
// 디자이너는 이 클래스를 상속한 BP_Furniture_*에서 메시·무게·필요인원만 설정한다.
UCLASS()
class TEAMCARRY_API ATCCarriableFurniture : public AActor, public ITCGrabbable, public ITCInteractable
{
	GENERATED_BODY()

public:
	ATCCarriableFurniture();

	// 서버 권위에서 운반자 입력을 합산해 가구를 이동시킨다
	virtual void Tick(float DeltaSeconds) override;

	// 복제 변수 등록
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//~ Begin ITCGrabbable
	virtual bool CanAddCarrier(ATCPlayerCharacter* Carrier) const override;
	virtual void AddCarrier(ATCPlayerCharacter* Carrier) override;
	virtual void RemoveCarrier(ATCPlayerCharacter* Carrier) override;
	virtual void ApplyCarryVelocity(const FVector& InCombinedVelocity, float DeltaSeconds) override;
	virtual int32 GetRequiredCarriers() const override;
	//~ End ITCGrabbable

	//~ Begin ITCInteractable
	virtual bool CanInteract_Implementation(ATCPlayerCharacter* Player) override;
	virtual void OnFocus_Implementation() override;
	virtual void OnUnfocus_Implementation() override;
	virtual void OnInteract_Implementation(ATCPlayerCharacter* Player) override;
	//~ End ITCInteractable

protected:
	virtual void BeginPlay() override;

	// 가구 본체 메시 (루트)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 무게 등급이 요구하는 최소 운반 인원 (BP에서 가구별 조정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	int32 RequiredCarriers = 2;

	// 기본 운반 속도 cm/s (BP에서 가구별 조정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	float BaseSpeed = 300.0f;

	// === 복제 변수 3종 (가구 도메인 소유, 서버에서만 갱신) ===

	// 현재 잡힘 상태 — 한 명이라도 운반 중이면 true
	UPROPERTY(ReplicatedUsing = OnRep_IsGrabbed, BlueprintReadOnly, Category = "Furniture|Net")
	bool bIsGrabbed = false;

	// 현재 이 가구를 운반 중인 플레이어 목록
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Furniture|Net")
	TArray<TObjectPtr<ATCPlayerCharacter>> GrabbingPlayers;

	// 이번 틱 서버가 합산한 운반 속도 (디버그/클라 표시용)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Furniture|Net")
	FVector CombinedVelocity = FVector::ZeroVector;

	// bIsGrabbed 복제 도착 시 클라 후처리 훅 (BP에서 확장 가능)
	UFUNCTION()
	void OnRep_IsGrabbed();
};
