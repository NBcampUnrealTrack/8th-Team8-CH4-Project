// TCCarryStatics.cpp

#include "Network/Carry/TCCarryStatics.h"

// 정원 대비 현재 인원 비율을 0~1로 클램프. 정원이 0이면 항상 1.
float UTCCarryStatics::CarrierFulfillRatio(int32 NumCarriers, int32 RequiredCarriers)
{
	if (RequiredCarriers <= 0)
	{
		return 1.0f;
	}

	return FMath::Clamp(static_cast<float>(NumCarriers) / static_cast<float>(RequiredCarriers), 0.0f, 1.0f);
}

// 운반자 입력 벡터를 합산 → 평균 방향 도출 → 정원 비율로 감속.
// 1차 모델: 방향은 합벡터 정규화, 속력은 BaseSpeed * 정원비율.
FVector UTCCarryStatics::CombineCarryVelocity(const TArray<FVector>& MoveInputs, int32 RequiredCarriers, float BaseSpeed)
{
	const int32 NumCarriers = MoveInputs.Num();
	if (NumCarriers == 0)
	{
		return FVector::ZeroVector;
	}

	// 운반자 의도 벡터 합산
	FVector Sum = FVector::ZeroVector;
	for (const FVector& Input : MoveInputs)
	{
		Sum += Input;
	}

	// 합벡터가 거의 0이면(서로 반대로 당김) 가구는 멈춤
	const FVector Direction = Sum.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const float Ratio = CarrierFulfillRatio(NumCarriers, RequiredCarriers);
	return Direction * BaseSpeed * Ratio;
}
