// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/O_Settings.h"

#include "CommonButtonBase.h"
#include "CommonAnimatedSwitcher.h"
#include "TeamCarry/UI/O_GraphicsSettings.h"
#include "TeamCarry/UI/O_AudioSettings.h"
#include "TeamCarry/UI/MockUIController.h"

UO_Settings::UO_Settings()
{
	// 오버레이는 스택에 누적되며 Back(ESC) 입력을 직접 처리한다.
	bIsBackHandler = true;
	// 활성화 시 포커스를 가져와 하위(일시정지 메뉴 등) 위젯이 입력 포커스를 잃도록 한다.
	bSupportsActivationFocus = true;

	SetIsFocusable(true);
}

void UO_Settings::NativeConstruct()
{
	Super::NativeConstruct();

	// --- 탭 버튼 → UCommonAnimatedSwitcher 인덱스 전환 바인딩 ---
	// 탭 인덱스는 Switcher 슬롯 순서(0: 그래픽, 1: 오디오)와 일치시킨다.
	if (Btn_GraphicsTab)
	{
		Btn_GraphicsTab->OnClicked().RemoveAll(this);
		Btn_GraphicsTab->OnClicked().AddUObject(this, &UO_Settings::HandleTabClicked, 0);
	}
	if (Btn_AudioTab)
	{
		Btn_AudioTab->OnClicked().RemoveAll(this);
		Btn_AudioTab->OnClicked().AddUObject(this, &UO_Settings::HandleTabClicked, 1);
	}

	// --- 공통 버튼 바인딩 ---
	if (Btn_Apply)
	{
		Btn_Apply->OnClicked().RemoveAll(this);
		Btn_Apply->OnClicked().AddUObject(this, &UO_Settings::HandleApplyClicked);
	}
	if (Btn_Back)
	{
		Btn_Back->OnClicked().RemoveAll(this);
		Btn_Back->OnClicked().AddUObject(this, &UO_Settings::HandleBackClicked);
	}

	// 진입 시 첫 번째(그래픽) 탭을 표시한다.
	ShowTab(0);
}

UWidget* UO_Settings::NativeGetDesiredFocusTarget() const
{
	return Btn_Back;
}

void UO_Settings::HandleTabClicked(int32 TabIndex)
{
	ShowTab(TabIndex);
}

void UO_Settings::ShowTab(int32 TabIndex)
{
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(TabIndex);
		UE_LOG(LogTemp, Log, TEXT("[UI Settings] Switched to tab index %d."), TabIndex);
	}
}

void UO_Settings::HandleApplyClicked()
{
	ApplyAllSettings();
}

void UO_Settings::HandleBackClicked()
{
	CloseSettings();
}

void UO_Settings::ApplyAllSettings()
{
	// 적용 로직 캡슐화: 메인 UI 는 "무엇을 적용할지"를 모르고, 각 탭에 위임만 한다.
	// 새 설정 탭이 추가되어도 메인 UI 코드는 해당 탭의 Apply 호출 한 줄만 늘면 된다.
	if (GraphicsTab)
	{
		GraphicsTab->ApplyGraphicsSettings();
	}
	if (AudioTab)
	{
		AudioTab->ApplyAudioSettings();
	}

	UE_LOG(LogTemp, Log, TEXT("[UI Settings] Apply clicked. All tab settings have been applied."));
}

void UO_Settings::CloseSettings()
{
	OnClosed.Broadcast();

	// 명세 5-1: 모든 화면 전환/스택 조작은 UMockUIController 를 통해서만 수행한다.
	// 실제 위젯 제거(MenuLayer->RemoveWidget)는 라우터가 PC(IUIHost)에 위임하므로,
	// 여기서 DeactivateWidget() 을 직접 호출하면 이중 제거가 된다 → 호출하지 않는다.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UMockUIController* MockController = GameInstance->GetSubsystem<UMockUIController>())
		{
			UE_LOG(LogTemp, Log, TEXT("[UI Settings] Closing settings. Popping overlay via MockUIController."));
			MockController->PopCurrentOverlay();
		}
	}
}

bool UO_Settings::NativeOnHandleBackAction()
{
	// ESC = 닫기. 처리했음을 알려 상위 스택으로 Back 이 전파되지 않게 한다.
	DeactivateWidget();
	return true;
}

TOptional<FUIInputConfig> UO_Settings::GetDesiredInputConfig() const
{
	// 이 위젯이 켜지면 마우스를 숨기지 않고 메뉴 모드로 전환함
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, true);
}

