// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/O_GraphicsSettings.h"

#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"

void UO_GraphicsSettings::NativeConstruct()
{
	Super::NativeConstruct();

	// 콤보 박스 항목이 비어 있으면 기본 품질 단계를 채워 넣는다.
	if (GraphicsQualityComboBox && GraphicsQualityComboBox->GetOptionCount() == 0)
	{
		GraphicsQualityComboBox->AddOption(TEXT("Low"));
		GraphicsQualityComboBox->AddOption(TEXT("Medium"));
		GraphicsQualityComboBox->AddOption(TEXT("High"));
		GraphicsQualityComboBox->AddOption(TEXT("Epic"));
		GraphicsQualityComboBox->AddOption(TEXT("Cinematic"));
	}

	SyncWidgetsFromCurrentSettings();
}

void UO_GraphicsSettings::SyncWidgetsFromCurrentSettings()
{
	UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!UserSettings)
	{
		return;
	}

	if (GraphicsQualityComboBox)
	{
		const int32 Quality = FMath::Clamp(UserSettings->GetOverallScalabilityLevel(), 0, 4);
		GraphicsQualityComboBox->SetSelectedIndex(Quality);
	}

	if (FullscreenCheckBox)
	{
		const bool bIsFullscreen = UserSettings->GetFullscreenMode() != EWindowMode::Windowed;
		FullscreenCheckBox->SetIsChecked(bIsFullscreen);
	}
}

void UO_GraphicsSettings::ApplyGraphicsSettings()
{
	UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!UserSettings)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI Settings|Graphics] GameUserSettings unavailable. Apply skipped."));
		return;
	}

	if (GraphicsQualityComboBox)
	{
		const int32 Quality = FMath::Clamp(GraphicsQualityComboBox->GetSelectedIndex(), 0, 4);
		UserSettings->SetOverallScalabilityLevel(Quality);
		UE_LOG(LogTemp, Log, TEXT("[UI Settings|Graphics] Scalability level set to %d."), Quality);
	}

	if (FullscreenCheckBox)
	{
		const EWindowMode::Type Mode = FullscreenCheckBox->IsChecked() ? EWindowMode::Fullscreen : EWindowMode::Windowed;
		UserSettings->SetFullscreenMode(Mode);
		UE_LOG(LogTemp, Log, TEXT("[UI Settings|Graphics] Fullscreen mode set to %d."), (int32)Mode);
	}

	// false: 해상도 등 즉시 반영 후 검증(원복) 단계는 메인 UI 의 15초 카운트다운 로직이 담당한다.
	UserSettings->ApplySettings(false);
	UserSettings->SaveSettings();
}
