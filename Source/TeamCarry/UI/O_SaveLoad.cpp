// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/O_SaveLoad.h"
#include "CommonButtonBase.h"
#include "CommonAnimatedSwitcher.h"
#include "Components/HorizontalBox.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Input/CommonUIInputTypes.h"

UO_SaveLoad::UO_SaveLoad()
{
	// 오버레이는 스택에 누적되며, Back(ESC) 입력을 직접 처리한다.
	bIsBackHandler = true;
	// 활성화 시 포커스를 가져와 하위 위젯이 입력 포커스를 잃도록 한다.
	bSupportsActivationFocus = true;
}

void UO_SaveLoad::NativeConstruct()
{
	Super::NativeConstruct();

	// 취소/닫기 버튼 바인딩.
	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked().RemoveAll(this);
		Btn_Cancel->OnClicked().AddUObject(this, &UO_SaveLoad::HandleCancelClicked);
	}

	// 저장 슬롯 카드는 백엔드(USaveGame) 연동 시 Box_SaveSlots 에 채워진다.
	// (프로토타입에서는 바인딩 유효성만 로깅한다.)
	if (!Box_SaveSlots)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI SaveLoad] Box_SaveSlots is not bound. Check the WBP hierarchy."));
	}
}

TOptional<FUIInputConfig> UO_SaveLoad::GetDesiredInputConfig() const
{
	// Menu 모드 = UI Only. 오버레이가 떠 있는 동안 게임 입력이 차단된다(명세 5-2).
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

UWidget* UO_SaveLoad::NativeGetDesiredFocusTarget() const
{
	if (Btn_Cancel)
	{
		return Btn_Cancel;
	}

	return Super::NativeGetDesiredFocusTarget();
}

bool UO_SaveLoad::NativeOnHandleBackAction()
{
	// ESC = 취소/닫기와 동일. 라우터를 통해 닫아 상태/스택 동기화를 유지한다(명세 5-1).
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI SaveLoad] Back(ESC) handled. Popping via MockUIController."));
		MockController->PopCurrentOverlay();
	}
	// 처리했음을 알려 상위 스택으로 Back 전파를 막는다.
	return true;
}

void UO_SaveLoad::HandleCancelClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// 오버레이만 닫는다(하위 O_PauseMenu 로 복귀).
		UE_LOG(LogTemp, Log, TEXT("[UI SaveLoad] Cancel clicked. Popping overlay."));
		MockController->PopCurrentOverlay();
	}
}
