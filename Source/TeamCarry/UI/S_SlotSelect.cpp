// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/S_SlotSelect.h"
#include "TeamCarry/UI/O_Confirm.h"
#include "CommonButtonBase.h"
#include "Components/HorizontalBox.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Network/Session/TCSessionFlow.h"
#include "Engine/GameInstance.h"

void US_SlotSelect::NativeConstruct()
{
	Super::NativeConstruct();

	// CommonUI의 네이티브 바인딩 방식인 OnClicked().AddUObject 를 적용했습니다.
	if (Btn_Back)
	{
		Btn_Back->OnClicked().AddUObject(this, &US_SlotSelect::HandleBackClicked);
	}

	// 가로형 카드 컨테이너는 백엔드 연동 시 슬롯 카드 위젯으로 채워진다.
	// (프로토타입에서는 바인딩 유효성만 로깅한다.)
	if (!Box_SlotCards)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI SlotSelect] Box_SlotCards is not bound. Check the WBP hierarchy."));
	}

	if (Btn_TempEmptySlot)
	{
		Btn_TempEmptySlot->OnClicked().AddUObject(this, &US_SlotSelect::HandleTempEmptySlotClicked);
	}

	SetIsFocusable(true);
}

UWidget* US_SlotSelect::NativeGetDesiredFocusTarget() const
{
	// 화면 진입 시 첫 슬롯에 기본 포커스를 부여합니다.
	if (Btn_TempEmptySlot)
	{
		return Btn_TempEmptySlot;
	}
	else if (Btn_Back)
	{
		return Btn_Back;
	}
	return Super::NativeGetDesiredFocusTarget();
}

bool US_SlotSelect::NativeOnHandleBackAction()
{
	// 명세 5-1 및 3-2: ESC 입력 시 S_MainMenu로 복귀
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI SlotSelect] Returning to Main Menu."));
		MockController->ReplaceState(EE_UIState::MainMenu);
	}
	return true;
}

void US_SlotSelect::ConfirmSlotAndCreateRoom(const FString& SlotName, bool bContinue)
{	
	// 명세 호스트 2~3단계: 슬롯 확정 → 세이브 선택 저장 → 세션 생성 + 로비 ServerTravel.
	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI SlotSelect] ConfirmSlot: '%s' continue=%d → HostCreateRoom"), *SlotName, bContinue);
		Flow->SetSaveSelection(SlotName, bContinue);
		Flow->HostCreateRoom();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI SlotSelect] UTCSessionFlow 없음 — 방 생성 불가"));
	}
}

void US_SlotSelect::HandleBackClicked()
{
	NativeOnHandleBackAction();
}

void US_SlotSelect::HandleTempEmptySlotClicked()
{
	// 명세 3-2: 빈 슬롯 선택(또는 기존 데이터 삭제) 시 O_Confirm 팝업 호출
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI SlotSelect] Empty slot clicked. Pushing O_Confirm overlay..."));

		// 1. 팝업을 띄우고 생성된 위젯의 포인터를 받아옵니다.
		UCommonActivatableWidget* OverlayWidget = MockController->PushOverlay(TEXT("O_Confirm"));

		// 2. 해당 위젯을 UO_Confirm 타입으로 캐스팅합니다.
		if (UO_Confirm* ConfirmUI = Cast<UO_Confirm>(OverlayWidget))
		{
			// 3. 브릿지 함수를 델리게이트에 묶습니다.
			FOnConfirmYesAction YesAction;
			YesAction.BindDynamic(this, &US_SlotSelect::OnConfirmNewGame);

			// 4. 팝업에 제목, 내용, 그리고 실행할 액션을 주입합니다.
			ConfirmUI->SetupConfirm(
				FText::FromString(TEXT("새 게임")),
				FText::FromString(TEXT("새로운 게임을 생성하시겠습니까?")),
				YesAction
			);
		}
	}
}

// 팝업에서 '확인'을 누르면 이 함수가 호출됩니다.
void US_SlotSelect::OnConfirmNewGame()
{
	// 명세에 따라 슬롯 이름과 bContinue = false (새 게임) 값을 방 생성 로직에 넘깁니다.
	ConfirmSlotAndCreateRoom(TEXT("SaveSlot_Temp"), false);
}