// Fill out your copyright notice in the Description page of Project Settings.

#include "CatchCharacter/Furniture/FurnitureCarryDebug.h"
#include "CatchCharacter/Furniture/FurnitureCarryShared.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"

void FCarryDebugState::TrackInputGate(const TArray<ACharacter*>& Players, int32 N,
	TFunctionRef<bool(ACharacter*, const UCharacterMovementComponent*)> HasInput)
{
	if (!IsCarryDebugEnabled())
		return;

	uint8 Mask = 0;
	bool bFoundRemote = false;
	for (int32 i = 0; i < N && i < 8; ++i)
	{
		ACharacter* P = Players[i];
		const UCharacterMovementComponent* CMC = P ? P->GetCharacterMovement() : nullptr;
		if (HasInput(P, CMC))
		{
			Mask |= static_cast<uint8>(1 << i);
		}
		// 원격 운반자는 가속도가 0으로 읽히는 프레임이 있어 속도와 함께 비교한다
		if (!bFoundRemote && P && !P->IsLocallyControlled())
		{
			bFoundRemote = true;
			RemoteAccel  = CMC ? CMC->GetCurrentAcceleration().Size2D() : 0.0f;
			RemoteVel    = P->GetVelocity().Size2D();
		}
	}
	if (Mask != InputGateMask)
	{
		++InputGateFlips;
		InputGateMask = Mask;
	}
}

void FCarryDebugState::TrackTarget(const TArray<double>& Weights, int32 N, double WTotal,
	const FVector& TargetLoc)
{
	if (!IsCarryDebugEnabled())
		return;

	uint8 DragMask = 0;
	for (int32 i = 0; i < N && i < 8; ++i)
	{
		if (Weights[i] <= 0.0)
		{
			DragMask |= static_cast<uint8>(1 << i);
		}
	}
	if (DragMask != DraggedMask)
	{
		++DraggedFlips;
		DraggedMask = DragMask;
	}
	WeightShare0 = (WTotal > 0.0 && N > 0) ? static_cast<float>(Weights[0] / WTotal) : 0.0f;

	if (!PrevTargetLoc.IsZero())
	{
		MaxTargetJump = FMath::Max(MaxTargetJump,
			static_cast<float>(FVector::Dist2D(TargetLoc, PrevTargetLoc)));
	}
	PrevTargetLoc = TargetLoc;
}

void FCarryDebugState::TrackFloor(float FloorZ)
{
	if (!IsCarryDebugEnabled() || FloorZ >= FLT_MAX * 0.5f)
		return;

	if (LastFloorZ != 0.0f)
	{
		MaxFloorStep = FMath::Max(MaxFloorStep, FMath::Abs(FloorZ - LastFloorZ));
	}
	LastFloorZ = FloorZ;
}

void FCarryDebugState::TrackHeightOffset(float CurrentHeightOffset)
{
	if (!IsCarryDebugEnabled())
		return;

	MaxOffsetStep    = FMath::Max(MaxOffsetStep, FMath::Abs(CurrentHeightOffset - LastHeightOffset));
	LastHeightOffset = CurrentHeightOffset;
}

void FCarryDebugState::ReportJitter(const AActor* Owner, int32 N, bool bAllLookingDown, float WorstPitch)
{
	if (!IsCarryDebugEnabled() || !Owner)
		return;

	const double NowT = FPlatformTime::Seconds();
	if (NowT - LastJitterLog <= 1.0)
		return;
	LastJitterLog = NowT;

	UE_LOG(LogCarry, Warning,
		TEXT("[떨림] %s N=%d | 높이: 완화=%d 전환/초=%d 최악피치=%.1f° 오프셋점프=%.1f 바닥Z점프=%.1f")
		TEXT(" | 입력: 게이트=%d 전환/초=%d 원격가속=%.0f 원격속도=%.0f")
		TEXT(" | 수평: 목표점프=%.1f 피동=%d 피동전환/초=%d 가중치0=%.2f")
		TEXT(" | 모드 완화=%d 바닥=%d 게이트=%d 스무딩=%.2f"),
		*Owner->GetName(), N,
		bAllLookingDown ? 1 : 0, LookDownFlips, WorstPitch, MaxOffsetStep, MaxFloorStep,
		static_cast<int32>(InputGateMask), InputGateFlips, RemoteAccel, RemoteVel,
		MaxTargetJump, static_cast<int32>(DraggedMask), DraggedFlips, WeightShare0,
		GetCarryLookDownRelaxMode(), IsCarryFloorComplexEnabled() ? 1 : 0,
		GetCarryInputGateMode(), GetCarryWeightSmoothTau());

	LookDownFlips  = 0;
	MaxOffsetStep  = 0.0f;
	MaxFloorStep   = 0.0f;
	InputGateFlips = 0;
	MaxTargetJump  = 0.0f;
	DraggedFlips   = 0;
}

void FCarryDebugState::LogHeight(int32 N, bool bUnderManned, bool bUpright, float TiltDeg,
	float AimPitch0, float TargetOffset, float CurrentOffset, float ClampedCenterZ,
	float MinZ, float MaxZ)
{
	if (!IsCarryDebugEnabled())
		return;

	const double NowT = FPlatformTime::Seconds();
	if (NowT - LastHeightLog <= 0.5)
		return;
	LastHeightLog = NowT;

	UE_LOG(LogCarry, Log,
		TEXT("[높이] N=%d 미달=%d 정립=%d 기움=%.0f° 피치0=%.0f° 목표=%.0f 적용=%.0f 중심Z=%.0f (허용 %.0f~%.0f)"),
		N, bUnderManned ? 1 : 0, bUpright ? 1 : 0, TiltDeg, AimPitch0,
		TargetOffset, CurrentOffset, ClampedCenterZ, MinZ, MaxZ);
}

void FCarryDebugState::HeartBeat(const AActor* Owner, int32 N, bool bHasStat, bool bAuthority)
{
	if (!IsCarryDebugEnabled() || !Owner)
		return;

	static double GLastBeatLog = -10.0;
	const double NowT = FPlatformTime::Seconds();
	if (NowT - GLastBeatLog <= 2.0)
		return;
	GLastBeatLog = NowT;

	UE_LOG(LogCarry, Log, TEXT("[운반틱] %s N=%d 스탯=%d 권위=%d"),
		*Owner->GetName(), N, bHasStat ? 1 : 0, bAuthority ? 1 : 0);
}

void FCarryDebugState::WarnNoStat(const AActor* Owner)
{
	if (!Owner)
		return;

	// 스탯 소실은 운반 틱 전체가 조용히 죽는 치명 상태 — F9와 무관하게 기록한다
	static double GLastNoStatLog = -10.0;
	const double NowT = FPlatformTime::Seconds();
	if (NowT - GLastNoStatLog <= 2.0)
		return;
	GLastNoStatLog = NowT;

	UE_LOG(LogCarry, Warning, TEXT("[운반틱] %s FurnitureStat=nullptr — 운반 이동 전체 불가"),
		*Owner->GetName());
}
