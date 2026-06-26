// TCCarrySpeedComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TCCarrySpeedComponent.generated.h"

/**
 * 가구 운반 중 인원비례 이동속도를 관리하는 컴포넌트.
 * TCPlayerCharacter에 부착되며, TCFurnitureActor가 Grab/Release 후 서버에서 호출한다.
 *
 * 속도 공식 (TCFurnitureActor에서 계산 후 전달):
 *   ratio < 1  → BaseSpeed * ratio       (정원 미달 — 인원 비례 감속)
 *   ratio == 1 → BaseSpeed               (정원 충족 — 정상)
 *   ratio > 1  → BaseSpeed * (1 - 0.15 * 초과인원), 최소 50%  (초과 — 페널티)
 *
 * 복제 흐름:
 *   서버: SetCarrySpeed(speed) → CarrySpeed Replicated → OnRep_CarrySpeed → CMC 적용
 *   운반 해제: ClearCarrySpeed() → 원래 MaxWalkSpeed 복원
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TEAMCARRY_API UTCCarrySpeedComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTCCarrySpeedComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버에서 호출: 운반 속도 설정 (인원비례 계산 결과값)
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|CarrySpeed")
	void SetCarrySpeed(float NewSpeed);

	// 서버에서 호출: 운반 해제 — 원래 이동속도 복원
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|CarrySpeed")
	void ClearCarrySpeed();

	// 현재 운반 중인지 여부
	UFUNCTION(BlueprintPure, Category = "TeamCarry|CarrySpeed")
	bool IsCarrying() const { return bIsCarrying; }

protected:
	virtual void BeginPlay() override;

private:
	// 운반 중 적용할 속도 (서버→클라 복제)
	UPROPERTY(ReplicatedUsing = OnRep_CarrySpeed)
	float CarrySpeed = 0.0f;

	// 운반 중 여부 (서버→클라 복제)
	UPROPERTY(ReplicatedUsing = OnRep_CarrySpeed)
	bool bIsCarrying = false;

	// CarrySpeed/bIsCarrying 변경 시 클라이언트에서 CMC 즉시 반영
	UFUNCTION()
	void OnRep_CarrySpeed();

	// 원래 MaxWalkSpeed 백업 (운반 해제 시 복원용, 서버/로컬 각자 보관)
	float OriginalMaxWalkSpeed = 0.0f;

	// CMC에 속도 적용하는 내부 헬퍼
	void ApplySpeedToCMC(float Speed);
};
