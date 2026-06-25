// TitleWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TitleWidget.generated.h"

class UButton;
class UPanelWidget;
class UUserWidget;

UCLASS()
class TEAMCARRY_API UTitleWidget : public UUserWidget
{
	GENERATED_BODY()

public: 
	UFUNCTION(BlueprintCallable, Category = "UI")
	void InitializeKeyboardFocus();

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Title")
	TSubclassOf<UUserWidget> CharacterSelectWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Title")
	TSubclassOf<UUserWidget> SettingWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Title")
	TSubclassOf<UUserWidget> CreditsWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Title")
	TSubclassOf<UUserWidget> ConfirmPopupWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Title")
	FName SaveSlotName = TEXT("ManualSave");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Title")
	int32 SaveUserIndex = 0;

protected:
	virtual void NativeConstruct() override;

	// Main Menu Buttons
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> GameStartButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> OptionsButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> CreditsButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> ExitButton;

	// Sub Menu (Continue / New Game)
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UPanelWidget> SubMenuPanel;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> NewGameButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> BackButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UPanelWidget> PopupRoot;

private:
	UFUNCTION()
	void HandleGameStartClicked();

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleNewGameClicked();

	UFUNCTION()
	void HandleOptionsClicked();

	UFUNCTION()
	void HandleCreditsClicked();

	UFUNCTION()
	void HandleExitClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleNewGameConfirm();

	UFUNCTION()
	void HandleExitConfirm();

	void ShowConfirmPopup(const FText& Message, const FScriptDelegate& OnConfirm);

	bool CheckSaveGame() const;
	void OpenCharacterSelect();
};