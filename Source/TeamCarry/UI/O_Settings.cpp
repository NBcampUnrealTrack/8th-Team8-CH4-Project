// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/O_Settings.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "GameFramework/GameUserSettings.h"
#include "Blueprint/UserWidget.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

void UO_Settings::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (ApplyButton)
	{
		ApplyButton->OnClicked.AddUniqueDynamic(this, &UO_Settings::ApplySettings);
	}
	if (BackButton)
	{
		BackButton->OnClicked.AddUniqueDynamic(this, &UO_Settings::CloseSettings);
	}
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UO_Settings::HandleMasterVolumeChanged);
	}
}

FReply UO_Settings::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseSettings();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UO_Settings::ApplySettings()
{
	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		if (GraphicsQualityComboBox)
		{
			const int32 Quality = FMath::Clamp(GraphicsQualityComboBox->GetSelectedIndex(), 0, 4);
			UserSettings->SetOverallScalabilityLevel(Quality);
		}

		if (FullscreenCheckBox)
		{
			UserSettings->SetFullscreenMode(FullscreenCheckBox->IsChecked() ? EWindowMode::Fullscreen : EWindowMode::Windowed);
		}

		UserSettings->ApplySettings(false);
		UserSettings->SaveSettings();
	}
}

void UO_Settings::CloseSettings()
{
	OnClosed.Broadcast();
	RemoveFromParent();
}

void UO_Settings::HandleMasterVolumeChanged(float Value)
{
	OnMasterVolumeChanged.Broadcast(Value);
}