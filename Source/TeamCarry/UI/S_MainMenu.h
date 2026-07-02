// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "S_MainMenu.generated.h"

class UButton;

/**
 * US_MainMenu - Main Menu Widget implementing CommonActivatableWidget
 */
UCLASS()
class TEAMCARRY_API US_MainMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	US_MainMenu();

	virtual void NativeConstruct() override;

	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// --- Main Menu Buttons (명세 3-1: Start / Options / Credits / Quit) ---
	// Btn_Start 는 통합 접속 팝업(O_JoinRoom)을 오버레이로 띄운다.
	// 방 만들기→S_SlotSelect / 방 참가→S_CharacterSelect 분기는 팝업 내부에서 결정된다.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Start;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Options;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Credits;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Quit;

private:
	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleOptionsClicked();

	UFUNCTION()
	void HandleCreditsClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void OnConfirmQuit();
};
