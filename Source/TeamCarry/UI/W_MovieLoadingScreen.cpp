// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/W_MovieLoadingScreen.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UW_MovieLoadingScreen::NativeConstruct()
{
	Super::NativeConstruct();

	ElapsedSeconds = 0.0f;
	ApplyLoadingProgress(0.0f);
}

void UW_MovieLoadingScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ElapsedSeconds += InDeltaTime;
	ApplyLoadingProgress(ComputeTimeBasedProgress(ElapsedSeconds));
}

float UW_MovieLoadingScreen::ComputeTimeBasedProgress(float InElapsedSeconds) const
{
	if (PseudoProgressDuration <= 0.0f)
	{
		return PseudoProgressCap;
	}
	return FMath::Clamp(InElapsedSeconds / PseudoProgressDuration, 0.0f, 1.0f) * PseudoProgressCap;
}

void UW_MovieLoadingScreen::ApplyLoadingProgress(float Alpha01)
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

void UW_MovieLoadingScreen::SetStatusText(const FText& NewText)
{
	if (Txt_Status)
	{
		Txt_Status->SetText(NewText);
	}
}
