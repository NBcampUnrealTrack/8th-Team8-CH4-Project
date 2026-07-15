// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/O_PauseMenu.h"
#include "Components/Button.h"
#include "CommonButtonBase.h"
#include "TeamCarry/UI/MockUIController.h"
#include "TeamCarry/UI/O_Confirm.h"
#include "Network/Session/TCSessionFlow.h"
#include "Input/CommonUIInputTypes.h"
#include "Engine/GameInstance.h"

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
		Btn_Resume->OnClicked().RemoveAll(this);
		Btn_Resume->OnClicked().AddUObject(this, &UO_PauseMenu::HandleResumeClicked);
	}

	// Bind Settings Button (설정창 푸시)
	if (Btn_Settings)
	{
		Btn_Settings->OnClicked().RemoveAll(this);
		Btn_Settings->OnClicked().AddUObject(this, &UO_PauseMenu::HandleSettingsClicked);
	}

	if (Btn_Save)
	{
		Btn_Save->OnClicked().RemoveAll(this);
		Btn_Save->OnClicked().AddUObject(this, &UO_PauseMenu::HandleSaveClicked);
	}

	if (Btn_ToLobby)
	{
		Btn_ToLobby->OnClicked().RemoveAll(this);
		Btn_ToLobby->OnClicked().AddUObject(this, &UO_PauseMenu::HandleToLobbyClicked);
	}

	if (Btn_Reset)
	{
		Btn_Reset->OnClicked().RemoveAll(this);
		Btn_Reset->OnClicked().AddUObject(this, &UO_PauseMenu::HandleResetClicked);
	}

	if (Btn_LeaveRoom)
	{
		Btn_LeaveRoom->OnClicked().RemoveAll(this);
		Btn_LeaveRoom->OnClicked().AddUObject(this, &UO_PauseMenu::HandleLeaveRoomClicked);
	}

	// 호출 컨텍스트(Lobby/Tutorial/InGame)에 따라 버튼 노출을 갱신한다(명세 4장-12).
	RefreshContextVisibility();
}

TOptional<FUIInputConfig> UO_PauseMenu::GetDesiredInputConfig() const
{
	// Menu 모드 = UI Only. 마우스는 캡처하지 않아 커서로 메뉴를 조작한다.
	// 이 위젯이 활성화된 동안 하위(게임) 입력으로 라우팅되지 않는다.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

bool UO_PauseMenu::NativeOnHandleBackAction()
{
	// ESC = Resume(닫기)과 동일. 라우터를 통해 닫아 상태/스택 동기화를 유지한다.
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI PauseMenu] Back(ESC) handled. Popping pause menu via MockUIController."));
		MockController->PopCurrentOverlay();
	}
	// 처리했음을 알려 상위 스택으로 Back 전파를 막는다.
	return true;
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
	// (호스트 권한 검사는 RefreshContextVisibility()의 버튼 표시 단계에서 이미 수행된 상태를 전제)
	// 실제 디스크 저장 로직은 백엔드(USaveGame) 연동 단계에서 O_SaveLoad 내부에 구현한다.
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI PauseMenu] Save clicked. Pushing O_SaveLoad overlay. (Host-only)"));
		MockController->PushOverlay(TEXT("O_SaveLoad"));
	}
}

void UO_PauseMenu::HandleToLobbyClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// 파괴적 액션(진행 중인 스테이지 즉시 이탈)이므로 직접 전환하지 않고 확인 모달을 푸시한다.
		UE_LOG(LogTemp, Log, TEXT("[UI PauseMenu] ToLobby clicked. Pushing O_Confirm overlay."));

		UCommonActivatableWidget* OverlayWidget = MockController->PushOverlay(TEXT("O_Confirm"));

		if (UO_Confirm* ConfirmUI = Cast<UO_Confirm>(OverlayWidget))
		{
			FOnConfirmYesAction YesAction;
			YesAction.BindDynamic(this, &UO_PauseMenu::OnConfirmReturnToLobby);

			ConfirmUI->SetupConfirm(
				FText::FromString(TEXT("로비로 복귀")),
				FText::FromString(TEXT("정말로 로비로 복귀하시겠습니까? 진행 중인 스테이지를 즉시 이탈합니다.")),
				YesAction
			);
		}
	}
}

void UO_PauseMenu::OnConfirmReturnToLobby()
{
	// UTCSessionFlow::HostReturnToLobby() 가 세션을 유지한 채 로비로 재트래블한다.
	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI PauseMenu] ToLobby confirmed. Requesting Return To Lobby."));
		Flow->HostReturnToLobby();
	}
}

void UO_PauseMenu::HandleResetClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// 파괴적 액션(진행 상황 초기화)이므로 직접 전환하지 않고 확인 모달을 푸시한다.
		UE_LOG(LogTemp, Log, TEXT("[UI PauseMenu] Reset clicked. Pushing O_Confirm overlay."));

		UCommonActivatableWidget* OverlayWidget = MockController->PushOverlay(TEXT("O_Confirm"));

		if (UO_Confirm* ConfirmUI = Cast<UO_Confirm>(OverlayWidget))
		{
			FOnConfirmYesAction YesAction;
			YesAction.BindDynamic(this, &UO_PauseMenu::OnConfirmResetStage);

			ConfirmUI->SetupConfirm(
				FText::FromString(TEXT("스테이지 초기화")),
				FText::FromString(TEXT("정말로 스테이지를 초기화하시겠습니까? 진행 상황이 모두 사라집니다.")),
				YesAction
			);
		}
	}
}

void UO_PauseMenu::OnConfirmResetStage()
{
	// UTCSessionFlow::RestartStage() 가 현재 선택된 스테이지 맵으로 재트래블한다.
	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI PauseMenu] Reset confirmed. Requesting Restart Stage."));
		Flow->RestartStage();
	}
}

void UO_PauseMenu::HandleLeaveRoomClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// 파괴적 액션(세션 파기)이므로 직접 전환하지 않고 확인 모달을 푸시한다.
		UE_LOG(LogTemp, Log, TEXT("[UI PauseMenu] LeaveRoom clicked. Pushing O_Confirm overlay."));

		UCommonActivatableWidget* OverlayWidget = MockController->PushOverlay(TEXT("O_Confirm"));

		if (UO_Confirm* ConfirmUI = Cast<UO_Confirm>(OverlayWidget))
		{
			FOnConfirmYesAction YesAction;
			YesAction.BindDynamic(this, &UO_PauseMenu::OnConfirmLeaveRoom);

			ConfirmUI->SetupConfirm(
				FText::FromString(TEXT("방 나가기")),
				FText::FromString(TEXT("정말로 방을 나가시겠습니까?")),
				YesAction
			);
		}
	}
}

void UO_PauseMenu::OnConfirmLeaveRoom()
{
	// UTCSessionFlow::LeaveToTitle() 이 세션 파기와 타이틀 레벨 이동을 함께 처리한다.
	// 방장뿐 아니라 참가자도 방을 나갈 수 있으므로 IsHost() 게이팅을 받지 않는다.
	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI PauseMenu] LeaveRoom confirmed. Requesting Leave To Title."));
		Flow->LeaveToTitle();
	}
}

void UO_PauseMenu::RefreshContextVisibility()
{
	// 컨텍스트 판정(v3 내부 개정): 새 enum/SetupContext() 없이 기존 MockUIController::GetCurrentState()를
	// 그대로 재사용한다 — PushOverlay는 CurrentState(Lobby/Tutorial/InGame)를 바꾸지 않으므로, 이
	// 오버레이가 어떤 화면 위에 떠 있는지를 그대로 알려준다(UI_Technical_Spec.md 4장-12).
	UMockUIController* MockController = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr;
	const EE_UIState CurrentState = MockController ? MockController->GetCurrentState() : EE_UIState::InGame;

	const UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr;
	const bool bIsHost = Flow && Flow->IsHost();

	if (CurrentState == EE_UIState::Lobby)
	{
		// 로비에는 저장할 진행 중인 스테이지도, 복귀할 다른 곳도, 리셋할 스테이지도 없다.
		if (Btn_Save) Btn_Save->SetVisibility(ESlateVisibility::Collapsed);
		if (Btn_ToLobby) Btn_ToLobby->SetVisibility(ESlateVisibility::Collapsed);
		if (Btn_Reset) Btn_Reset->SetVisibility(ESlateVisibility::Collapsed);
		if (Btn_LeaveRoom) Btn_LeaveRoom->SetVisibility(ESlateVisibility::Visible);
	}
	else if (CurrentState == EE_UIState::Tutorial)
	{
		// 튜토리얼은 CompleteTutorial() 단일 경로로만 로비 복귀(명세 4장-6).
		if (Btn_Save)
		{
			Btn_Save->SetVisibility(ESlateVisibility::Visible);
			Btn_Save->SetIsEnabled(false);
		}
		if (Btn_ToLobby) Btn_ToLobby->SetVisibility(ESlateVisibility::Collapsed);
		if (Btn_Reset) Btn_Reset->SetVisibility(ESlateVisibility::Collapsed);
		if (Btn_LeaveRoom) Btn_LeaveRoom->SetVisibility(ESlateVisibility::Collapsed);
	}
	else // EE_UIState::InGame (기본값)
	{
		// 멀티플레이 동기화를 위해 호스트(방장) 전용. 노출 분기는 편의일 뿐이므로 실제 트래블/
		// 저장/리셋 실행은 서버 측에서 재검증한다(명세 6장-8).
		const ESlateVisibility HostVis = bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		if (Btn_Save) Btn_Save->SetVisibility(HostVis);
		if (Btn_ToLobby) Btn_ToLobby->SetVisibility(HostVis);
		if (Btn_Reset) Btn_Reset->SetVisibility(HostVis);
		if (Btn_LeaveRoom) Btn_LeaveRoom->SetVisibility(ESlateVisibility::Collapsed);
	}
}
