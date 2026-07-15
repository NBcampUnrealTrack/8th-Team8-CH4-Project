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

// 조준 피치를 [-90,90]으로 접는다 — 원격 피치(RemoteViewPitch)는 ±90 밖 랩 값이 올 수 있음
inline float FoldAimPitch(float PitchDeg)
{
	PitchDeg = FRotator::NormalizeAxis(PitchDeg);
	if (PitchDeg > 90.0f)       PitchDeg = 180.0f - PitchDeg;
	else if (PitchDeg < -90.0f) PitchDeg = -180.0f - PitchDeg;
	return PitchDeg;
}
