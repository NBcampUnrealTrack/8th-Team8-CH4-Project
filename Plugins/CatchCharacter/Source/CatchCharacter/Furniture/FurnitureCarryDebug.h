// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class AActor;
class ACharacter;
class UCharacterMovementComponent;

// 운반 진단 상태·출력을 한곳에 모은다. 모든 함수는 F9(TC.GrabDebug)가 꺼져 있으면 즉시 반환하므로
// 호출부에 별도 가드가 필요 없다. 진단을 걷어낼 때는 이 파일과 호출 한 줄씩만 지우면 된다.
struct FCarryDebugState
{
	// 매 틱 누적 → 1초마다 ReportJitter에서 출력하고 리셋
	void OnLookDownFlip() { ++LookDownFlips; }
	void TrackInputGate(const TArray<ACharacter*>& Players, int32 N,
		TFunctionRef<bool(ACharacter*, const UCharacterMovementComponent*)> HasInput);
	void TrackTarget(const TArray<double>& Weights, int32 N, double WTotal, const FVector& TargetLoc);
	void TrackFloor(float FloorZ);
	void TrackHeightOffset(float CurrentHeightOffset);

	// 1초 간격 종합 출력 (수직·수평·입력 지표 + 현재 모드값)
	void ReportJitter(const AActor* Owner, int32 N, bool bAllLookingDown, float WorstPitch);

	// 0.5초 간격 높이 파이프라인 추적 (피치 → 목표 → 클램프 중 어디서 막히는지)
	void LogHeight(int32 N, bool bUnderManned, bool bUpright, float TiltDeg, float AimPitch0,
		float TargetOffset, float CurrentOffset, float ClampedCenterZ, float MinZ, float MaxZ);

	// 2초 간격 운반 틱 생존 확인 (플러그인 로그 전체가 침묵하는 사고와 구분용)
	static void HeartBeat(const AActor* Owner, int32 N, bool bHasStat, bool bAuthority);
	static void WarnNoStat(const AActor* Owner);

private:
	// 수직
	int32  LookDownFlips    = 0;
	float  LastFloorZ       = 0.0f;
	float  LastHeightOffset = 0.0f;
	float  MaxFloorStep     = 0.0f;
	float  MaxOffsetStep    = 0.0f;

	// 입력 게이트
	int32  InputGateFlips   = 0;
	uint8  InputGateMask    = 0;
	float  RemoteAccel      = 0.0f;   // 첫 원격 운반자의 가속도 크기 (0이면 서버가 못 읽은 것)
	float  RemoteVel        = 0.0f;

	// 수평
	FVector PrevTargetLoc   = FVector::ZeroVector;
	float   MaxTargetJump   = 0.0f;
	uint8   DraggedMask     = 0;
	int32   DraggedFlips    = 0;
	float   WeightShare0    = 0.0f;

	double LastJitterLog    = -10.0;   // 가구별 스로틀
	double LastHeightLog    = -10.0;
};
