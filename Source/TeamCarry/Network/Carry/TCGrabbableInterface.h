// TCGrabbableInterface.h

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TCGrabbableInterface.generated.h"

class ATCPlayerCharacter;

// 운반 가능한 가구가 구현하는 서버 권위 계약.
// 잡기 판정·합산 물리는 서버에서만 호출되며, 복제 변수는 가구(Furniture 도메인)가 소유한다.
// ITCInteractable(포커스/상호작용 입력)과 병행 — 이쪽은 운반 상태/물리 전담.
UINTERFACE(MinimalAPI)
class UTCGrabbable : public UInterface
{
	GENERATED_BODY()
};

class TEAMCARRY_API ITCGrabbable
{
	GENERATED_BODY()

public:
	// 서버: 이 운반자를 추가할 수 있는 상태인지(정원·잡힘 상태 검증)
	virtual bool CanAddCarrier(ATCPlayerCharacter* Carrier) const = 0;

	// 서버: 운반자 등록 — 성공 시 bIsGrabbed/GrabbingPlayers 복제 갱신
	virtual void AddCarrier(ATCPlayerCharacter* Carrier) = 0;

	// 서버: 운반자 해제 — 마지막 한 명이 놓으면 bIsGrabbed=false
	virtual void RemoveCarrier(ATCPlayerCharacter* Carrier) = 0;

	// 서버: 합산된 운반 속도를 이번 틱 동안 가구에 적용
	virtual void ApplyCarryVelocity(const FVector& CombinedVelocity, float DeltaSeconds) = 0;

	// 무게 등급이 요구하는 최소 운반 인원
	virtual int32 GetRequiredCarriers() const = 0;
};
