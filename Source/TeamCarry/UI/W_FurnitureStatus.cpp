// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/W_FurnitureStatus.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

UW_FurnitureStatus::UW_FurnitureStatus()
{
	// 커서가 가구 위에 없는 기본 상태에서는 숨김으로 시작한다.
	SetVisibility(ESlateVisibility::Hidden);
}

void UW_FurnitureStatus::NativeConstruct()
{
	Super::NativeConstruct();

	// 초기 텍스트·게이지를 빈/최대 상태로 초기화한다.
	if (Txt_FurnitureName)
	{
		Txt_FurnitureName->SetText(FText::GetEmpty());
	}
	if (Bar_Durability)
	{
		Bar_Durability->SetPercent(1.0f);
	}
	TargetDurabilityPercent = 1.0f;
	DisplayedDurabilityPercent = 1.0f;
}

void UW_FurnitureStatus::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Bar_Durability)
	{
		return;
	}

	if (!FMath::IsNearlyEqual(DisplayedDurabilityPercent, TargetDurabilityPercent, 0.001f))
	{
		DisplayedDurabilityPercent = FMath::FInterpTo(DisplayedDurabilityPercent, TargetDurabilityPercent, InDeltaTime, 6.0f);
		Bar_Durability->SetPercent(DisplayedDurabilityPercent);
	}
}

void UW_FurnitureStatus::UpdateFurnitureStatus(const FString& Name, float CurrentDurability, float MaxDurability)
{
	if (Txt_FurnitureName)
	{
		Txt_FurnitureName->SetText(FText::FromString(Name));
	}

	// MaxDurability 가 0 이면 분모 0 방어. Clamp 로 [0, 1] 범위를 보장한다.
	// 실제 게이지 값은 즉시 적용하지 않고 목표값만 갱신 — NativeTick 이 서서히 보간해 반영한다.
	TargetDurabilityPercent = (MaxDurability > 0.0f)
		? FMath::Clamp(CurrentDurability / MaxDurability, 0.0f, 1.0f)
		: 0.0f;

	UE_LOG(LogTemp, Log, TEXT("[UI FurnitureStatus] Updated: Name=%s, Durability=%.1f/%.1f"),
		*Name, CurrentDurability, MaxDurability);
}

void UW_FurnitureStatus::ShowStatus()
{
	SetVisibility(ESlateVisibility::Visible);
	UE_LOG(LogTemp, Log, TEXT("[UI FurnitureStatus] Visibility -> Visible."));
}

void UW_FurnitureStatus::HideStatus()
{
	SetVisibility(ESlateVisibility::Hidden);
	UE_LOG(LogTemp, Log, TEXT("[UI FurnitureStatus] Visibility -> Hidden."));
}
