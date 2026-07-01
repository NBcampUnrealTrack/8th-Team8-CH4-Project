// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/O_JoinRoom.h"
#include "Components/EditableTextBox.h"
#include "TeamCarry/UI/MockUIController.h"
#include "CommonButtonBase.h"
#include "Network/Session/TCSessionFlow.h"
#include "Input/CommonUIInputTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

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

	// CommonUI 네이티브 바인딩(OnClicked().AddUObject).
	if (Btn_CreateRoom)
	{
		Btn_CreateRoom->OnClicked().AddUObject(this, &UO_JoinRoom::HandleCreateRoomClicked);
	}
	if (Btn_JoinRoom)
	{
		Btn_JoinRoom->OnClicked().AddUObject(this, &UO_JoinRoom::HandleJoinRoomClicked);
	}
	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked().AddUObject(this, &UO_JoinRoom::HandleCancelClicked);
	}

	// 코드 입력 변경 감지(참가 버튼 활성/비활성 판단).
	if (Txt_Code)
	{
		Txt_Code->OnTextChanged.AddUniqueDynamic(this, &UO_JoinRoom::HandleCodeTextChanged);
	}

	// 초기 상태: 코드 미입력 → 참가 버튼 비활성.
	if (Btn_JoinRoom)
	{
		Btn_JoinRoom->SetIsEnabled(false);
	}

	// 세션 단계 통지 구독(검색/조인 진행·실패 → UI 반영). 세션 로직은 SessionFlow 담당.
	BindSessionFlow(true);
}

void UO_JoinRoom::NativeDestruct()
{
	BindSessionFlow(false);
	Super::NativeDestruct();
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
	return true;
}

void UO_JoinRoom::HandleCreateRoomClicked()
{
	// '방 만들기' → 팝업 닫고 세이브 슬롯 선택 화면으로 교체(실제 호스팅은 슬롯 확정 시).
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] Hosting room. Replacing state to SlotSelect..."));
		MockController->PopCurrentOverlay();
		MockController->ReplaceState(EE_UIState::SlotSelect);
	}
}

void UO_JoinRoom::HandleJoinRoomClicked()
{
	// '방 참가' → 코드로 세션 검색·조인. 검색/코드매칭/조인/ClientTravel 은 SessionFlow 가 수행.
	const FString Code = Txt_Code ? Txt_Code->GetText().ToString().TrimStartAndEnd() : FString();
	if (Code.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] 코드 미입력 — 참가 보류."));
		return;
	}

	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] JoinRoomByCode: %s"), *Code);
		SetBusy(true);
		Flow->JoinRoomByCode(Code);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI JoinRoom] UTCSessionFlow 없음 — 참가 불가."));
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

void UO_JoinRoom::HandleCodeTextChanged(const FText& Text)
{
	// 코드 입력란에 1글자 이상 존재할 때만 참가 버튼 활성화.
	if (Btn_JoinRoom)
	{
		Btn_JoinRoom->SetIsEnabled(!Text.IsEmpty());
	}
}

void UO_JoinRoom::HandleSessionPhaseChanged(ETCSessionPhase Phase, const FString& Message)
{
	switch (Phase)
	{
	case ETCSessionPhase::Searching:
	case ETCSessionPhase::Joining:
		SetBusy(true);
		break;

	case ETCSessionPhase::Joined:
		// 조인 성공 → SessionFlow/TCGameInstance 가 ClientTravel 수행. 대기.
		UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] 방 접속 완료 — ClientTravel 대기."));
		break;

	case ETCSessionPhase::Failed:
		UE_LOG(LogTemp, Warning, TEXT("[UI JoinRoom] 세션 실패: %s"), *Message);
		SetBusy(false);
		if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
		{
			MockController->PushOverlay(TEXT("O_Error"));
		}
		break;

	default:
		break;
	}
}

void UO_JoinRoom::SetBusy(bool bBusy)
{
	// 검색/조인 중 중복 요청 방지용 버튼 잠금.
	if (Btn_JoinRoom)
	{
		Btn_JoinRoom->SetIsEnabled(!bBusy);
	}
	if (Btn_CreateRoom)
	{
		Btn_CreateRoom->SetIsEnabled(!bBusy);
	}
}

void UO_JoinRoom::BindSessionFlow(bool bBind)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}
	UTCSessionFlow* Flow = GI->GetSubsystem<UTCSessionFlow>();
	if (!Flow)
	{
		return;
	}
	if (bBind)
	{
		Flow->OnSessionPhaseChanged.AddUniqueDynamic(this, &UO_JoinRoom::HandleSessionPhaseChanged);
	}
	else
	{
		Flow->OnSessionPhaseChanged.RemoveDynamic(this, &UO_JoinRoom::HandleSessionPhaseChanged);
	}
}
