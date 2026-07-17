// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/S_Loading.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Input/CommonUIInputTypes.h"

void US_Loading::NativeConstruct()
{
	Super::NativeConstruct();

	ElapsedSeconds = 0.0f;
	ApplyLoadingProgress(0.0f);
}

void US_Loading::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ElapsedSeconds += InDeltaTime;
	ApplyLoadingProgress(ComputeTimeBasedProgress(ElapsedSeconds));
}

TOptional<FUIInputConfig> US_Loading::GetDesiredInputConfig() const
{
	// 명세 4장-9: 로딩 중 모든 입력 무시.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

float US_Loading::ComputeTimeBasedProgress(float InElapsedSeconds) const
{
	if (PseudoProgressDuration <= 0.0f)
	{
		return PseudoProgressCap;
	}

	const float Alpha = FMath::Clamp(InElapsedSeconds / PseudoProgressDuration, 0.0f, 1.0f);
	return Alpha * PseudoProgressCap;
}

void US_Loading::ApplyLoadingProgress(float Alpha01)
{
	if (PB_Loading)
	{
		PB_Loading->SetPercent(Alpha01);
	}

	if (Txt_LoadingGauge)
	{
		Txt_LoadingGauge->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Alpha01 * 100.0f))));
	}
}
