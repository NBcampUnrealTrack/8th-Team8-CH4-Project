// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

// 운반 이벤트 로그 카테고리 — Output Log에서 "LogCarry"로 검색 (정의: FurnitureGrabSystem.cpp)
DECLARE_LOG_CATEGORY_EXTERN(LogCarry, Log, All);

// F9(TC.GrabDebug) 운반 시각화 게이트 — CVar는 TeamCarry 모듈이 등록하므로 조회만 한다 (null이면 재시도)
inline bool IsCarryDebugEnabled()
{
	static IConsoleVariable* CachedCVar = nullptr;
	if (!CachedCVar)
	{
		CachedCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("TC.GrabDebug"));
	}
	return CachedCVar && CachedCVar->GetInt() != 0;
}

// 전원 내려다봄 높이 하한 완화 방식 — 0=끔, 1=단일 임계(-25°), 2=이력(-25° 진입/-15° 이탈)
inline int32 GetCarryLookDownRelaxMode()
{
	static IConsoleVariable* CachedCVar = nullptr;
	if (!CachedCVar)
	{
		CachedCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("TC.Carry.LookDownRelax"));
	}
	return CachedCVar ? CachedCVar->GetInt() : 1;
}

// 가구 밑 지지면 스윕의 복합 콜리전 사용 여부 — 심플 콜리전 없는 그레이박스 바닥 대응
inline bool IsCarryFloorComplexEnabled()
{
	static IConsoleVariable* CachedCVar = nullptr;
	if (!CachedCVar)
	{
		CachedCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("TC.Carry.FloorComplex"));
	}
	return CachedCVar ? (CachedCVar->GetInt() != 0) : true;
}

// 운반 입력 게이트 판정 소스 — 0=가속도, 1=속도, 2=가속도 래치
inline int32 GetCarryInputGateMode()
{
	static IConsoleVariable* CachedCVar = nullptr;
	if (!CachedCVar)
	{
		CachedCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("TC.Carry.InputGate"));
	}
	return CachedCVar ? CachedCVar->GetInt() : 0;
}

// 운반 가중치 비율 스무딩 시정수(초). 0이면 감쇠 없음
inline float GetCarryWeightSmoothTau()
{
	static IConsoleVariable* CachedCVar = nullptr;
	if (!CachedCVar)
	{
		CachedCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("TC.Carry.WeightSmooth"));
	}
	return CachedCVar ? CachedCVar->GetFloat() : 0.0f;
}

// 조준 피치를 [-90,90]으로 접는다 — 원격 피치(RemoteViewPitch)는 ±90 밖 랩 값이 올 수 있음
inline float FoldAimPitch(float PitchDeg)
{
	PitchDeg = FRotator::NormalizeAxis(PitchDeg);
	if (PitchDeg > 90.0f)       PitchDeg = 180.0f - PitchDeg;
	else if (PitchDeg < -90.0f) PitchDeg = -180.0f - PitchDeg;
	return PitchDeg;
}
