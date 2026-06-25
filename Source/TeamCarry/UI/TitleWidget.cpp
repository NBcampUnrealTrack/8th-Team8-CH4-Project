// TitleWidget.cpp


#include "TeamCarry/UI/TitleWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/SaveGame.h"

void UTitleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Main Menu
	if (GameStartButton)
	{
		GameStartButton->OnClicked.AddUniqueDynamic(this, &UTitleWidget::HandleGameStartClicked);
	}
	if (OptionsButton)
	{
		OptionsButton->OnClicked.AddUniqueDynamic(this, &UTitleWidget::HandleOptionsClicked);
	}
	if (CreditsButton)
	{
		CreditsButton->OnClicked.AddUniqueDynamic(this, &UTitleWidget::HandleCreditsClicked);
	}
	if (ExitButton)
	{
		ExitButton->OnClicked.AddUniqueDynamic(this, &UTitleWidget::HandleExitClicked);
	}

	// Sub Menu
	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddUniqueDynamic(this, &UTitleWidget::HandleContinueClicked);
		ContinueButton->SetIsEnabled(CheckSaveGame());
	}
	if (NewGameButton)
	{
		NewGameButton->OnClicked.AddUniqueDynamic(this, &UTitleWidget::HandleNewGameClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.AddUniqueDynamic(this, &UTitleWidget::HandleBackClicked);
	}

	if (SubMenuPanel)
	{
		SubMenuPanel->SetVisibility(ESlateVisibility::Hidden);
	}

	SetIsFocusable(true);
	if (APlayerController* PC = GetOwningPlayer())
	{
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PC, this, EMouseLockMode::DoNotLock, false);
		PC->SetShowMouseCursor(true);
	}
}

void UTitleWidget::HandleGameStartClicked()
{
	if (SubMenuPanel)
	{
		SubMenuPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void UTitleWidget::HandleContinueClicked()
{
	//if (UUserWidget* SaveGame = Cast<UUserWidget>(UGameplayStatics::LoadGameFromSlot(SaveSlotName.ToString(), SaveUserIndex)))
	//{
	//	UGameplayStatics::OpenLevel(this, SaveGame->SavedLevelName);
	//}
}

void UTitleWidget::HandleNewGameClicked()
{
	if (CheckSaveGame())
	{
		FScriptDelegate OnConfirmDelegate;
		OnConfirmDelegate.BindUFunction(this, FName("HandleNewGameConfirm"));
		ShowConfirmPopup(NSLOCTEXT("TestUI", "NewGameOverwrite", "기존 진행도가 덮어쓰기 될 수 있습니다"), OnConfirmDelegate);
	}
	else
	{
		OpenCharacterSelect();
	}
}

void UTitleWidget::HandleOptionsClicked()
{
	if (SettingWidgetClass)
	{
		CreateWidget<UUserWidget>(GetOwningPlayer(), SettingWidgetClass)->AddToViewport(10);
	}
}

void UTitleWidget::HandleCreditsClicked()
{
	if (CreditsWidgetClass)
	{
		CreateWidget<UUserWidget>(GetOwningPlayer(), CreditsWidgetClass)->AddToViewport(10);
	}
}

void UTitleWidget::HandleExitClicked()
{
	FScriptDelegate OnConfirmDelegate;
	OnConfirmDelegate.BindUFunction(this, FName("HandleExitConfirm"));
	ShowConfirmPopup(NSLOCTEXT("TestUI", "QuitGameConfirm", "게임을 종료하시겠습니까?"), OnConfirmDelegate);
}

void UTitleWidget::HandleBackClicked()
{
	if (SubMenuPanel)
	{
		SubMenuPanel->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UTitleWidget::HandleNewGameConfirm()
{
	OpenCharacterSelect();
}

void UTitleWidget::HandleExitConfirm()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}

void UTitleWidget::ShowConfirmPopup(const FText& Message, const FScriptDelegate& OnConfirm)
{
	if (!ConfirmPopupWidgetClass)
	{
		return;
	}

	UUserWidget* Popup = CreateWidget<UUserWidget>(GetOwningPlayer(), ConfirmPopupWidgetClass);
	if (!Popup)
	{
		return;
	}

	//Popup->SetMessage(Message);
	//Popup->OnConfirmed.AddUnique(OnConfirm);

	if (PopupRoot)
	{
		PopupRoot->AddChild(Popup);
	}
	else
	{
		Popup->AddToViewport(100);
	}
}

bool UTitleWidget::CheckSaveGame() const
{
	return false;
	//return UGameplayStatics::DoesSaveGameExist(SaveSlotName.ToString(), SaveUserIndex);
}

void UTitleWidget::OpenCharacterSelect()
{
	if (CharacterSelectWidgetClass)
	{
		CreateWidget<UUserWidget>(GetOwningPlayer(), CharacterSelectWidgetClass)->AddToViewport();
	}
	else
	{
		UGameplayStatics::OpenLevel(this, FName("CharacterSelect"));
	}
}

void UTitleWidget::InitializeKeyboardFocus()
{
	if (GameStartButton)
	{
		GameStartButton->SetKeyboardFocus();
	}
}