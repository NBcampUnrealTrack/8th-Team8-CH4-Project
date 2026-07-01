// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/S_SlotSelect.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Network/Session/TCSessionFlow.h"

void US_SlotSelect::NativeConstruct()
{
	Super::NativeConstruct();

	// 뒤로 가기 버튼 바인딩(메인 메뉴 복귀).
	if (Btn_Back)
	{
		Btn_Back->OnClicked.AddUniqueDynamic(this, &US_SlotSelect::HandleBackClicked);
	}

	// 가로형 카드 컨테이너는 백엔드 연동 시 슬롯 카드 위젯으로 채워진다.
	// (프로토타입에서는 바인딩 유효성만 로깅한다.)
	if (!Box_SlotCards)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI SlotSelect] Box_SlotCards is not bound. Check the WBP hierarchy."));
	}
}

UWidget* US_SlotSelect::NativeGetDesiredFocusTarget() const
{
	if (Btn_Back)
	{
		return Btn_Back;
	}

	return Super::NativeGetDesiredFocusTarget();
}

bool US_SlotSelect::NativeOnHandleBackAction()
{
	// ESC = 뒤로 가기 버튼과 동일하게 메인 메뉴로 복귀.
	HandleBackClicked();
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
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// 명세 1 흐름: S_SlotSelect ──뒤로(ESC)──▶ S_MainMenu (풀스크린 교체).
		UE_LOG(LogTemp, Log, TEXT("[UI SlotSelect] Back clicked. Replacing to MainMenu."));
		MockController->ReplaceState(EE_UIState::MainMenu);
	}
}
