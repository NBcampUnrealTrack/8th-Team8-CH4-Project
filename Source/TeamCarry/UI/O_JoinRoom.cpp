// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/O_JoinRoom.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Input/CommonUIInputTypes.h"

namespace
{
	// 모드 선택 시각 피드백용 색상(명세 3-8: 선택 항목 테두리/하이라이트 표시).
	const FLinearColor SelectedColor(0.20f, 0.55f, 1.0f, 1.0f);   // 선택됨(파란 하이라이트)
	const FLinearColor UnselectedColor(1.0f, 1.0f, 1.0f, 1.0f);   // 미선택(기본)
}

UO_JoinRoom::UO_JoinRoom()
{
	// 강제 모달: Back(ESC)을 이 위젯이 직접 처리하도록 잡아둔다.
	bIsBackHandler = true;
	// 활성화 시 포커스를 가져와 하위 위젯의 입력 포커스를 차단한다.
	bSupportsActivationFocus = true;
}

void UO_JoinRoom::NativeConstruct()
{
	Super::NativeConstruct();

	// 모드 선택 버튼 바인딩
	if (Btn_CreateRoom)
	{
		Btn_CreateRoom->OnClicked.AddUniqueDynamic(this, &UO_JoinRoom::HandleCreateRoomClicked);
	}

	if (Btn_JoinRoom)
	{
		Btn_JoinRoom->OnClicked.AddUniqueDynamic(this, &UO_JoinRoom::HandleJoinRoomClicked);
	}

	// 코드 입력 변경 감지(방 참가 시 확인 버튼 활성/비활성 판단)
	if (Txt_Code)
	{
		Txt_Code->OnTextChanged.AddUniqueDynamic(this, &UO_JoinRoom::HandleCodeTextChanged);
	}

	// 확정/취소 버튼 바인딩
	if (Btn_Confirm)
	{
		Btn_Confirm->OnClicked.AddUniqueDynamic(this, &UO_JoinRoom::HandleConfirmClicked);
	}

	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked.AddUniqueDynamic(this, &UO_JoinRoom::HandleCancelClicked);
	}

	// 초기 상태: 모드 미선택(None) → 확인 버튼 비활성화, 하이라이트 없음.
	CurrentMode = EJoinRoomMode::None;
	UpdateSelectionVisuals();
}

TOptional<FUIInputConfig> UO_JoinRoom::GetDesiredInputConfig() const
{
	// Menu 모드 = UI Only. 모달이 떠 있는 동안 하위 위젯/게임으로 입력이 가지 않는다.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

UWidget* UO_JoinRoom::NativeGetDesiredFocusTarget() const
{
	// 기본 포커스는 '방 만들기'에 둔다.
	if (Btn_CreateRoom)
	{
		return Btn_CreateRoom;
	}

	return Super::NativeGetDesiredFocusTarget();
}

bool UO_JoinRoom::NativeOnHandleBackAction()
{
	// ESC = 취소와 동일. 라우터를 통해 닫아 상태/스택 동기화를 유지한다(명세 5-1).
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] Back(ESC) handled as Cancel. Popping via MockUIController."));
		MockController->PopCurrentOverlay();
	}
	// 처리했음을 알려 상위 스택으로 Back 전파를 막는다.
	return true;
}

void UO_JoinRoom::HandleCreateRoomClicked()
{
	// '방 만들기' 선택. 코드 입력은 불필요하므로 확인 버튼 즉시 활성화 대상.
	CurrentMode = EJoinRoomMode::CreateRoom;
	UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] Mode selected: CreateRoom."));
	UpdateSelectionVisuals();
}

void UO_JoinRoom::HandleJoinRoomClicked()
{
	// '방 참가' 선택. 코드 입력란에 문자열이 있어야 확인 버튼이 활성화된다.
	CurrentMode = EJoinRoomMode::JoinRoom;
	UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] Mode selected: JoinRoom."));
	UpdateSelectionVisuals();
}

void UO_JoinRoom::HandleCodeTextChanged(const FText& /*Text*/)
{
	// 코드 입력 변경 시(주로 JoinRoom 모드) 확인 버튼 활성화 상태를 재평가한다.
	UpdateSelectionVisuals();
}

void UO_JoinRoom::HandleConfirmClicked()
{
	// 비활성 상태에서 호출되는 경우를 방어한다.
	if (!ShouldEnableConfirm())
	{
		return;
	}

	UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>();
	if (!MockController)
	{
		return;
	}

	// 모드에 따라 분기 후, 팝업을 닫고 풀스크린 화면을 교체한다(명세 3-8).
	switch (CurrentMode)
	{
	case EJoinRoomMode::CreateRoom:
		// 방 만들기 → 세이브 슬롯 관리 화면으로 교체.
		UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] Confirm(CreateRoom). Closing popup, replacing to SlotSelect."));
		MockController->PopCurrentOverlay();
		MockController->ReplaceState(EE_UIState::SlotSelect);
		break;

	case EJoinRoomMode::JoinRoom:
		// 방 참가 → (코드 검증은 백엔드 연동 단계에서 처리) 캐릭터 선택/로비 화면으로 교체.
		UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] Confirm(JoinRoom). Code='%s'. Closing popup, replacing to CharacterSelect."),
			Txt_Code ? *Txt_Code->GetText().ToString() : TEXT(""));
		MockController->PopCurrentOverlay();
		MockController->ReplaceState(EE_UIState::CharacterSelect);
		break;

	default:
		break;
	}
}

void UO_JoinRoom::HandleCancelClicked()
{
	// 취소: 팝업만 닫는다(하위 풀스크린은 이미 S_MainMenu 이므로 복귀됨, 명세 3-8).
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] Cancel clicked. Closing popup, returning to MainMenu."));
		MockController->PopCurrentOverlay();
	}
}

bool UO_JoinRoom::ShouldEnableConfirm() const
{
	switch (CurrentMode)
	{
	case EJoinRoomMode::CreateRoom:
		// 방 만들기는 코드가 필요 없으므로 즉시 활성화.
		return true;

	case EJoinRoomMode::JoinRoom:
		// 방 참가는 코드 입력란에 공백이 아닌 문자열이 존재할 때만 활성화.
		return Txt_Code && !Txt_Code->GetText().IsEmptyOrWhitespace();

	case EJoinRoomMode::None:
	default:
		// 미선택 상태에서는 비활성화.
		return false;
	}
}

void UO_JoinRoom::UpdateSelectionVisuals()
{
	// 1) 모드 버튼 하이라이트로 현재 선택 상태를 안내한다.
	if (Btn_CreateRoom)
	{
		Btn_CreateRoom->SetBackgroundColor(CurrentMode == EJoinRoomMode::CreateRoom ? SelectedColor : UnselectedColor);
	}

	if (Btn_JoinRoom)
	{
		Btn_JoinRoom->SetBackgroundColor(CurrentMode == EJoinRoomMode::JoinRoom ? SelectedColor : UnselectedColor);
	}

	// 2) 확인 버튼 활성화 상태 갱신.
	if (Btn_Confirm)
	{
		Btn_Confirm->SetIsEnabled(ShouldEnableConfirm());
	}
}
