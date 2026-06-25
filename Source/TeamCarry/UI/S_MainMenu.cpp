// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/S_MainMenu.h"
#include "Components/Button.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Kismet/GameplayStatics.h"

void US_MainMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind Start Button (게임 시작 → 세이브 슬롯 선택)
	if (Btn_Start)
	{
		Btn_Start->OnClicked.AddUniqueDynamic(this, &US_MainMenu::HandleStartClicked);
	}

	// Bind Options Button
	if (Btn_Options)
	{
		Btn_Options->OnClicked.AddUniqueDynamic(this, &US_MainMenu::HandleOptionsClicked);
	}

	// Bind Credits Button
	if (Btn_Credits)
	{
		Btn_Credits->OnClicked.AddUniqueDynamic(this, &US_MainMenu::HandleCreditsClicked);
	}

	// Bind Quit Button
	if (Btn_Quit)
	{
		Btn_Quit->OnClicked.AddUniqueDynamic(this, &US_MainMenu::HandleQuitClicked);
	}

	// Set focus
	SetIsFocusable(true);
}

void US_MainMenu::HandleStartClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// 명세 ③①: 게임 시작 → 세이브 슬롯 관리 화면(S_SlotSelect)으로 전환.
		// 이어하기/새 게임 분기는 슬롯 선택 시점에 결정된다.
		UE_LOG(LogTemp, Log, TEXT("[UI MainMenu] Start clicked. Transitioning to S_SlotSelect..."));
		MockController->ReplaceState(EE_UIState::SlotSelect);
	}
}

void US_MainMenu::HandleCreditsClicked()
{
	// 명세 ①: 크레딧 스크롤 화면(ESC로 복귀). 현재 프로토타입에는 별도 State가 없어
	//          연동 위치만 표시한다. 크레딧 화면 구현 시 여기서 전환을 트리거한다.
	UE_LOG(LogTemp, Log, TEXT("[UI MainMenu] Credits clicked. (Credits scroll - prototype stub)"));
}

void US_MainMenu::HandleOptionsClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI MainMenu] Options clicked. Opening Settings overlay..."));
		MockController->PushOverlay(TEXT("O_Settings"));
	}
}

void US_MainMenu::HandleQuitClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI MainMenu] Quit clicked. Opening Exit Confirm overlay..."));
		MockController->PushOverlay(TEXT("O_Confirm"));
	}
}
