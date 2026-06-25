// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/O_PauseMenu.h"
#include "Components/Button.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Input/CommonUIInputTypes.h"

UO_PauseMenu::UO_PauseMenu()
{
	// 오버레이는 스택에 누적되며, Back(ESC) 입력을 직접 처리한다.
	bIsBackHandler = true;
	// 활성화 시 포커스를 가져와 하위 위젯이 입력 포커스를 잃도록 한다.
	bSupportsActivationFocus = true;
}

void UO_PauseMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind Resume Button (메뉴 닫기)
	if (Btn_Resume)
	{
		Btn_Resume->OnClicked.AddUniqueDynamic(this, &UO_PauseMenu::HandleResumeClicked);
	}

	// Bind Settings Button (설정창 푸시)
	if (Btn_Settings)
	{
		Btn_Settings->OnClicked.AddUniqueDynamic(this, &UO_PauseMenu::HandleSettingsClicked);
	}

	// Bind Save Button (호스트 전용 수동 저장)
	if (Btn_Save)
	{
		Btn_Save->OnClicked.AddUniqueDynamic(this, &UO_PauseMenu::HandleSaveClicked);

		// =====================================================================
		// [호스트 권한 처리 위치 - 프로토타입 단계에서는 미구현]
		// 멀티플레이 연동 시, 아래 위치에서 로컬 플레이어가 호스트(방장)인지
		// 판별하여 Btn_Save 의 노출/활성화를 제어한다.
		//   const bool bIsHost = GetOwningPlayer() && GetOwningPlayer()->HasAuthority();
		//   Btn_Save->SetIsEnabled(bIsHost);                 // 클라이언트: 비활성
		//   Btn_Save->SetVisibility(bIsHost ? Visible : Collapsed); // 또는 숨김
		// 또한 튜토리얼(S_Tutorial) 진입 시에도 비활성 처리한다.
		// =====================================================================
	}

	// Bind ToTitle Button (타이틀 복귀)
	if (Btn_ToTitle)
	{
		Btn_ToTitle->OnClicked.AddUniqueDynamic(this, &UO_PauseMenu::HandleToTitleClicked);
	}
}

TOptional<FUIInputConfig> UO_PauseMenu::GetDesiredInputConfig() const
{
	// Menu 모드 = UI Only. 마우스는 캡처하지 않아 커서로 메뉴를 조작한다.
	// 이 위젯이 활성화된 동안 하위(게임) 입력으로 라우팅되지 않는다.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UO_PauseMenu::HandleResumeClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI PauseMenu] Resume clicked. Closing pause menu overlay."));
		MockController->PopCurrentOverlay();
	}
}

void UO_PauseMenu::HandleSettingsClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI PauseMenu] Settings clicked. Pushing O_Settings overlay."));
		MockController->PushOverlay(TEXT("O_Settings"));
	}
}

void UO_PauseMenu::HandleSaveClicked()
{
	// 명세 3-1 / O_SaveLoad: [수동 저장] → 수동 저장/불러오기 관리 오버레이를 스택에 Push.
	// (호스트 권한 검사는 NativeConstruct 의 버튼 표시 단계에서 이미 수행된 상태를 전제)
	// 실제 디스크 저장 로직은 백엔드(USaveGame) 연동 단계에서 O_SaveLoad 내부에 구현한다.
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI PauseMenu] Save clicked. Pushing O_SaveLoad overlay. (Host-only)"));
		MockController->PushOverlay(TEXT("O_SaveLoad"));
	}
}

void UO_PauseMenu::HandleToTitleClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// 파괴적 액션이므로 직접 전환하지 않고 확인 모달(O_Confirm)을 푸시한다.
		UE_LOG(LogTemp, Log, TEXT("[UI PauseMenu] ToTitle clicked. Pushing O_Confirm overlay."));
		MockController->PushOverlay(TEXT("O_Confirm"));
	}
}
