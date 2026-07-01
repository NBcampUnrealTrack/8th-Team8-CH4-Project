// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/O_JoinRoom.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Network/Session/TCSessionFlow.h"
#include "Input/CommonUIInputTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

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

	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked.AddUniqueDynamic(this, &UO_JoinRoom::HandleCancelClicked);
	}

	// 초기 상태: 모드 미선택(None) → 확인 버튼 비활성화, 하이라이트 없음.
	CurrentMode = EJoinRoomMode::None;
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
}

void UO_JoinRoom::HandleJoinRoomClicked()
{
	// '방 참가' 선택. 코드 입력란에 문자열이 있어야 확인 버튼이 활성화된다.
	CurrentMode = EJoinRoomMode::JoinRoom;
	UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] Mode selected: JoinRoom."));

	// 코드가 있으면 실제 세션 검색→조인을 시작(클라이언트 경로).
	const FString Code = Txt_Code ? Txt_Code->GetText().ToString().TrimStartAndEnd() : FString();
	if (Code.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] 코드 미입력 — 검색 보류."));
		return;
	}
	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] JoinRoomByCode: %s"), *Code);
		Flow->JoinRoomByCode(Code);
	}
}

void UO_JoinRoom::HandleCodeTextChanged(const FText& /*Text*/)
{
	// 코드 입력 변경 시(주로 JoinRoom 모드) 확인 버튼 활성화 상태를 재평가한다.
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