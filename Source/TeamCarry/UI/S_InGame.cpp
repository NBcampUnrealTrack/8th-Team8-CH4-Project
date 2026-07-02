// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/S_InGame.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Engine/Texture2D.h"
#include "Components/Image.h"
#include "Components/Button.h"

void US_InGame::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	// Setup initial placeholder values
	if (TextBlock_Score)
	{
		TextBlock_Score->SetText(FText::FromString(TEXT("$100")));
	}
	if (TextBlock_Timer)
	{
		TextBlock_Timer->SetText(FText::FromString(TEXT("00 : 00")));
	}
	if (TextBlock_InteractPrompt)
	{
		TextBlock_InteractPrompt->SetText(FText::GetEmpty());
	}
	if (Image_Map)
	{
		if (MapTexture)
		{
			// 위젯에 텍스처를 브러시로 설정
			Image_Map->SetBrushFromTexture(MapTexture);
			// 위젯 자체를 표시
			Image_Map->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			// 텍스처가 할당되지 않았다면 위젯 숨김
			Image_Map->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (Btn_Menu)
	{
		Btn_Menu->OnClicked.AddUniqueDynamic(this, &US_InGame::HandleMenuClicked);
	}


	// Subscribe to MockUIController delegates
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->OnTeamMoneyUpdated.AddUniqueDynamic(this, &US_InGame::HandleTeamMoneyUpdated);
		//MockController->OnFurnitureSettled.AddUniqueDynamic(this, &US_InGame::HandleFurnitureSettled);
		MockController->OnInteractTargetChanged.AddUniqueDynamic(this, &US_InGame::HandleInteractTargetChanged);
		//MockController->OnDurabilityChanged.AddUniqueDynamic(this, &US_InGame::HandleDurabilityChanged);

		UE_LOG(LogTemp, Log, TEXT("[UI InGameHUD] Successfully bound to MockUIController delegates."));
	}
}

void US_InGame::NativeDestruct()
{
	// Unsubscribe from MockUIController delegates
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->OnTeamMoneyUpdated.RemoveAll(this);
		//MockController->OnFurnitureSettled.RemoveAll(this);
		MockController->OnInteractTargetChanged.RemoveAll(this);
		//MockController->OnDurabilityChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

FReply US_InGame::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
		{
			UE_LOG(LogTemp, Warning, TEXT("[UI InGameHUD] Escape pressed! Triggering Pause Menu overlay."));
			MockController->PushOverlay(TEXT("O_PauseMenu"));
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

TOptional<FUIInputConfig> US_InGame::GetDesiredInputConfig() const
{
	// TOptional 객체로 감싸서 반환합니다.
	return TOptional<FUIInputConfig>(FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently, true));
}

void US_InGame::HandleTeamMoneyUpdated(int32 NewTotalMoney)
{
	UE_LOG(LogTemp, Log, TEXT("[UI InGameHUD] HUD Received Team Money Update: $%d"), NewTotalMoney);
	if (TextBlock_Score)
	{
		TextBlock_Score->SetText(FText::Format(NSLOCTEXT("InGameUI", "MoneyFormat", "${0}"), FText::AsNumber(NewTotalMoney)));
	}
}

//void US_InGame::HandleFurnitureSettled(int32 AddedMoney, int32 Grade)
//{
//	UE_LOG(LogTemp, Warning, TEXT("[UI InGameHUD] HUD Received Furniture Settled Event: +$%d, Grade: %d"), AddedMoney, Grade);
//	
//	// Temporarily display a settle notification on screen for visual feedback
//	if (TextBlock_WarnPlayers)
//	{
//		FText SettleText = FText::Format(NSLOCTEXT("InGameUI", "FurnitureSettleFormat", "+${0} (Perfect!)"), FText::AsNumber(AddedMoney));
//		TextBlock_WarnPlayers->SetText(SettleText);
//		TextBlock_WarnPlayers->SetVisibility(ESlateVisibility::Visible);
//	}
//}

void US_InGame::HandleInteractTargetChanged(AActor* Target, FString Key)
{
	UE_LOG(LogTemp, Log, TEXT("[UI InGameHUD] HUD Received Interact Target Change: %s"), *Key);
	if (TextBlock_InteractPrompt)
	{
		if (Key.IsEmpty())
		{
			TextBlock_InteractPrompt->SetText(FText::GetEmpty());
		}
		else
		{
			TextBlock_InteractPrompt->SetText(FText::FromString(Key));
		}
	}
}

//void US_InGame::HandleDurabilityChanged(float Current, float Max)
//{
//	if (ProgressBar_Durability)
//	{
//		float Percent = Max > 0.0f ? (Current / Max) : 0.0f;
//		ProgressBar_Durability->SetPercent(Percent);
//		
//		// If durability is low, show low durability warning
//		if (TextBlock_WarnPlayers && Percent < 0.3f && Percent > 0.0f)
//		{
//			TextBlock_WarnPlayers->SetText(NSLOCTEXT("InGameUI", "LowDurabilityWarning", "주의: 가구 부서짐 위험!"));
//			TextBlock_WarnPlayers->SetVisibility(ESlateVisibility::Visible);
//		}
//		else if (TextBlock_WarnPlayers && (Percent >= 0.3f || Percent == 0.0f))
//		{
//			TextBlock_WarnPlayers->SetVisibility(ESlateVisibility::Collapsed);
//		}
//	}
//}

void US_InGame::HandleMenuClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI InGame] Open Main Menu."));
	}
}