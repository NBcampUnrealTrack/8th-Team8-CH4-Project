// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/S_CharacterSelect.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "TeamCarry/UI/MockUIController.h"

void US_CharacterSelect::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind buttons
	if (Btn_Ready)
	{
		Btn_Ready->OnClicked.AddUniqueDynamic(this, &US_CharacterSelect::HandleReadyClicked);
	}

	if (Btn_Start)
	{
		Btn_Start->OnClicked.AddUniqueDynamic(this, &US_CharacterSelect::HandleStartClicked);
		Btn_Start->SetIsEnabled(false); // Disabled initially until everyone is ready
	}

	if (Btn_Back)
	{
		Btn_Back->OnClicked.AddUniqueDynamic(this, &US_CharacterSelect::HandleBackClicked);
	}

	// Listen to Lobby slot update delegate from MockUIController
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->OnLobbySlotUpdated.AddUniqueDynamic(this, &US_CharacterSelect::HandleLobbySlotUpdated);
	}

	// Setup initial dummy slots visual representation
	if (Slot1_Name) Slot1_Name->SetText(FText::FromString(TEXT("Player_1 (You)")));
	if (Slot1_Status) Slot1_Status->SetText(FText::FromString(TEXT("NOT READY")));

	if (Slot2_Name) Slot2_Name->SetText(FText::FromString(TEXT("Player_2 (AI)")));
	if (Slot2_Status) Slot2_Status->SetText(FText::FromString(TEXT("READY")));

	bLocalPlayerReady = false;
	SetIsFocusable(true);
}

void US_CharacterSelect::NativeDestruct()
{
	// Clean up delegate listener
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->OnLobbySlotUpdated.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void US_CharacterSelect::HandleReadyClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// Toggle ready state
		bLocalPlayerReady = !bLocalPlayerReady;
		
		UE_LOG(LogTemp, Log, TEXT("[UI CharacterSelect] Btn_Ready Clicked. Ready State: %s"), bLocalPlayerReady ? TEXT("TRUE") : TEXT("FALSE"));
		
		// Set local player (slot 0) status
		MockController->SetLobbySlotReady(0, bLocalPlayerReady);

		// AI Single-player ready condition:
		// S_CharacterSelect에서 AI 혼자 시작할 수 있도록, Btn_Ready 클릭 시 즉시 조건을 만족시켜 Btn_Start를 활성화할 것
		if (Btn_Start)
		{
			Btn_Start->SetIsEnabled(bLocalPlayerReady);
			UE_LOG(LogTemp, Warning, TEXT("[UI CharacterSelect] Auto-satisfied start condition! Btn_Start Enabled: %s"), bLocalPlayerReady ? TEXT("True") : TEXT("False"));
		}
	}
}

void US_CharacterSelect::HandleStartClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI CharacterSelect] Host started the game. Transitioning to S_Tutorial..."));
		
		MockController->ReplaceState(EE_UIState::Tutorial);
	}
}

void US_CharacterSelect::HandleBackClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI CharacterSelect] Returning to Main Menu."));
		MockController->ReplaceState(EE_UIState::MainMenu);
	}
}

void US_CharacterSelect::HandleLobbySlotUpdated(int32 SlotIndex, FPlayerInfo PlayerInfo)
{
	FText StatusText = PlayerInfo.bIsReady ? FText::FromString(TEXT("READY")) : FText::FromString(TEXT("NOT READY"));

	UE_LOG(LogTemp, Log, TEXT("[UI CharacterSelect] Delegate Received -> Slot %d (%s): %s"), 
		SlotIndex, *PlayerInfo.PlayerName, PlayerInfo.bIsReady ? TEXT("READY") : TEXT("NOT READY"));

	if (SlotIndex == 0)
	{
		if (Slot1_Status) Slot1_Status->SetText(StatusText);
	}
	else if (SlotIndex == 1)
	{
		if (Slot2_Status) Slot2_Status->SetText(StatusText);
	}
}
