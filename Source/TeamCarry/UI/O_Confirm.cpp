// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/O_Confirm.h"
#include "Components/Button.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Input/CommonUIInputTypes.h"

UO_Confirm::UO_Confirm()
{
	// 강제 모달: Back(ESC)을 이 위젯이 직접 처리하도록 잡아둔다.
	// (배경 클릭/뒤로가기로 자동 닫힘을 막으려면 NativeOnHandleBackAction()을
	//  오버라이드해 true를 반환하고 Deactivate를 호출하지 않으면 된다 — 프로토타입은 생략)
	bIsBackHandler = true;
	// 활성화 시 포커스를 가져와 하위 위젯의 입력 포커스를 차단한다.
	bSupportsActivationFocus = true;
}

void UO_Confirm::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind Confirm Button (예 / 확인)
	if (Btn_Confirm)
	{
		Btn_Confirm->OnClicked.AddUniqueDynamic(this, &UO_Confirm::HandleConfirmClicked);
	}

	// Bind Cancel Button (아니오 / 취소)
	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked.AddUniqueDynamic(this, &UO_Confirm::HandleCancelClicked);
	}
}

TOptional<FUIInputConfig> UO_Confirm::GetDesiredInputConfig() const
{
	// Menu 모드 = UI Only. 모달이 떠 있는 동안 하위 위젯/게임으로 입력이 가지 않는다.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

UWidget* UO_Confirm::NativeGetDesiredFocusTarget() const
{
	// 오조작 방지: 기본 포커스를 "아니오"(취소) 버튼에 둔다.
	if (Btn_Cancel)
	{
		return Btn_Cancel;
	}

	// 취소 버튼이 바인딩되지 않은 경우 기본 동작으로 폴백.
	return Super::NativeGetDesiredFocusTarget();
}

void UO_Confirm::HandleConfirmClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// 프로토타입: 호출 측이 위임한 파괴적 액션은 백엔드 연동 단계에서 처리한다.
		// 여기서는 모달을 닫는 것까지만 수행한다.
		UE_LOG(LogTemp, Log, TEXT("[UI Confirm] Confirm(Yes) clicked. Closing modal."));
		MockController->PopCurrentOverlay();
	}
}

void UO_Confirm::HandleCancelClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI Confirm] Cancel(No) clicked. Closing modal without action."));
		MockController->PopCurrentOverlay();
	}
}
