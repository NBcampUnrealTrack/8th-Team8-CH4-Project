// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "O_Settings.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class USlider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSettingAction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSettingVolumeChanged, float, Value);

UCLASS()
class TEAMCARRY_API UO_Settings : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UI|Settings")
	FSettingAction OnClosed;

	UPROPERTY(BlueprintAssignable, Category = "UI|Settings")
	FSettingVolumeChanged OnMasterVolumeChanged;

	UFUNCTION(BlueprintCallable, Category = "UI|Settings")
	void ApplySettings();

	UFUNCTION(BlueprintCallable, Category = "UI|Settings")
	void CloseSettings();

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<USlider> MasterVolumeSlider;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UComboBoxString> GraphicsQualityComboBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCheckBox> FullscreenCheckBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> ApplyButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> BackButton;

private:
	UFUNCTION()
	void HandleMasterVolumeChanged(float Value);
};