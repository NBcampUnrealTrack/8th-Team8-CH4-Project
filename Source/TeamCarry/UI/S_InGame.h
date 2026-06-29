// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "S_InGame.generated.h"

class UTextBlock;
class UProgressBar;
class UTexture2D;
class UImage;
class UButton;

/**
 * US_InGame - In-Game HUD Screen implementing CommonActivatableWidget
 */
UCLASS()
class TEAMCARRY_API US_InGame : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// --- HUD Bound Widgets ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> TextBlock_Score;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> TextBlock_Timer;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> TextBlock_InteractPrompt;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UImage> Image_Map;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Map")
	TObjectPtr<UTexture2D> MapTexture;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Menu;

	//부서진 정도 알려줌
	//UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	//TObjectPtr<UTextBlock> TextBlock_WarnPlayers

private:
	// --- Delegate Listeners ---
	UFUNCTION()
	void HandleTeamMoneyUpdated(int32 NewTotalMoney);

	//UFUNCTION()
	//void HandleFurnitureSettled(int32 AddedMoney, int32 Grade);

	UFUNCTION()
	void HandleInteractTargetChanged(AActor* Target, FString Key);

	//UFUNCTION()
	//void HandleDurabilityChanged(float Current, float Max);

	UFUNCTION()
	void HandleMenuClicked();
};
