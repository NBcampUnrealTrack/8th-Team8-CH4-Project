// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/O_KeyGuide.h"
#include "CommonButtonBase.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Input/CommonUIInputTypes.h"

UO_KeyGuide::UO_KeyGuide()
{
	// 오버레이는 스택에 누적되며 Back(ESC) 입력을 직접 처리한다.
	bIsBackHandler = true;
	// 활성화 시 포커스를 가져와 하위(O_PauseMenu 등) 위젯이 입력 포커스를 잃도록 한다.
	bSupportsActivationFocus = true;
}

void UO_KeyGuide::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Back)
	{
		Btn_Back->OnClicked().RemoveAll(this);
		Btn_Back->OnClicked().AddUObject(this, &UO_KeyGuide::HandleBackClicked);
	}
}

TOptional<FUIInputConfig> UO_KeyGuide::GetDesiredInputConfig() const
{
	// Menu 모드 = UI Only. 조작법 팝업이 떠 있는 동안 게임 입력이 통과하지 않는다.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

bool UO_KeyGuide::NativeOnHandleBackAction()
{
	// ESC = Btn_Back 과 동일하게 처리. 라우터를 통해 닫아 상태/스택 동기화를 유지한다.
	UE_LOG(LogTemp, Log, TEXT("[UI KeyGuide] Back(ESC) handled. Popping via MockUIController."));
	CloseKeyGuide();
	// 처리했음을 알려 상위 스택으로 Back 전파를 막는다.
	return true;
}

void UO_KeyGuide::HandleBackClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[UI KeyGuide] Btn_Back clicked. Closing key guide overlay."));
	CloseKeyGuide();
}

void UO_KeyGuide::CloseKeyGuide()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->PopCurrentOverlay();
	}
}
