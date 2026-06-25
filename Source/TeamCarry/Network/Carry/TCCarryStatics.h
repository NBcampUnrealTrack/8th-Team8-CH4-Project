// TCCarryStatics.h

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TCCarryStatics.generated.h"

// 다인 동시 운반의 합산 속도 계산 — 서버 권위에서만 호출하는 순수 유틸
UCLASS()
class TEAMCARRY_API UTCCarryStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 운반자들의 의도 이동 벡터를 합산해 가구의 최종 이동 속도를 도출.
	// MoveInputs   : 각 운반자가 이번 틱에 원하는 이동 벡터(방향*입력세기)
	// RequiredCarriers : 무게 등급이 요구하는 최소 인원(미달이면 감속)
	// BaseSpeed    : 가구 기본 운반 속도(cm/s)
	// 반환         : 서버가 가구에 적용할 합산 속도 벡터
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Carry")
	static FVector CombineCarryVelocity(const TArray<FVector>& MoveInputs, int32 RequiredCarriers, float BaseSpeed);

	// 정원 충족 비율(0~1). 미달이면 1 미만으로 감속 계수 역할.
	UFUNCTION(BlueprintPure, Category = "TeamCarry|Carry")
	static float CarrierFulfillRatio(int32 NumCarriers, int32 RequiredCarriers);
};
