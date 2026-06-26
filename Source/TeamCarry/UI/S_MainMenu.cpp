// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/S_MainMenu.h"
#include "Components/Button.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Kismet/GameplayStatics.h"

US_MainMenu::US_MainMenu()
{
	bSupportsActivationFocus = true;

	UE_LOG(LogTemp, Error, TEXT("US_MainMenu Constructor"));
}

void US_MainMenu::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Start)
	{
		Btn_Start->SetKeyboardFocus();

		UE_LOG(LogTemp, Error, TEXT("Force Focus Start"));
	}

	UE_LOG(LogTemp, Error, TEXT("US_MainMenu NativeConstruct"));

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

UWidget* US_MainMenu::NativeGetDesiredFocusTarget() const
{
	UE_LOG(LogTemp, Error, TEXT("US_MainMenu Focus Target"));

	return Btn_Start;
}

void US_MainMenu::HandleStartClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// 명세 3-1 / 3-8: 게임 시작 → 통합 접속 팝업(O_JoinRoom)을 오버레이로 띄운다(PushOverlay).
		// 방 만들기/방 참가 분기와 S_SlotSelect·S_CharacterSelect 전환은 팝업 내부에서 결정된다.
		UE_LOG(LogTemp, Log, TEXT("[UI MainMenu] Start clicked. Pushing O_JoinRoom overlay..."));
		MockController->PushOverlay(TEXT("O_JoinRoom"));
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
