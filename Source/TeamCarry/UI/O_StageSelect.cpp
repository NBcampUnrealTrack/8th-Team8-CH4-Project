// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/O_StageSelect.h"
#include "Components/ListView.h"
#include "CommonButtonBase.h"
#include "Input/CommonUIInputTypes.h"
#include "TeamCarry/UI/MockUIController.h"
#include "TeamCarry/UI/StageListItemData.h"
#include "Network/Session/TCSessionFlow.h"

void UO_StageSelect::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Confirm)
	{
		Btn_Confirm->OnClicked().AddUObject(this, &UO_StageSelect::HandleConfirmClicked);
	}
	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked().AddUObject(this, &UO_StageSelect::HandleCancelClicked);
	}
	if (List_Stages)
	{
		List_Stages->OnItemClicked().AddUObject(this, &UO_StageSelect::HandleStageItemClicked);
	}

	SetIsFocusable(true);
	PendingSelectedStageId = 0;
	PopulateStageList();
}

void UO_StageSelect::NativeDestruct()
{
	if (List_Stages)
	{
		List_Stages->OnItemClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

TOptional<FUIInputConfig> UO_StageSelect::GetDesiredInputConfig() const
{
	// 명세 4장-5/6장-2: 활성화 중 게임 입력을 차단하고 UI 전용(Menu) 입력으로 전환한다.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

bool UO_StageSelect::NativeOnHandleBackAction()
{
	// ESC = 취소와 동일 처리(명세 4장-5).
	HandleCancelClicked();
	return true;
}

void UO_StageSelect::PopulateStageList()
{
	if (!List_Stages)
	{
		return;
	}

	List_Stages->ClearListItems();

	UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr;
	if (!Flow)
	{
		return;
	}

	for (const FStageInfo& Info : Flow->GetAllStageInfos())
	{
		UStageListItemData* Item = NewObject<UStageListItemData>(this);
		Item->StageId = Info.StageId;
		Item->DisplayName = Info.DisplayName;
		List_Stages->AddItem(Item);
	}
}

void UO_StageSelect::HandleStageItemClicked(UObject* Item)
{
	if (const UStageListItemData* StageItem = Cast<UStageListItemData>(Item))
	{
		PendingSelectedStageId = StageItem->StageId;
		// 시각적 하이라이트(확장 개방 — 실제 선택 스타일은 카드 위젯 쪽 추후 작업).
		List_Stages->SetSelectedItem(Item);
	}
}

void UO_StageSelect::HandleConfirmClicked()
{
	// 명세: 하이라이트된 스테이지가 있을 때만 확정. 없으면 변경 없이 닫혀 기존 선택(미선택 시 기본
	// 1스테이지)이 유지된다.
	if (PendingSelectedStageId > 0)
	{
		if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
		{
			Flow->SetStageSelection(PendingSelectedStageId);
		}
	}

	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->PopCurrentOverlay();
	}
}

void UO_StageSelect::HandleCancelClicked()
{
	// 선택을 변경하지 않고 닫는다 — SetStageSelection() 을 호출하지 않는다.
	// (S_Lobby 는 자신의 커서 모드 상태를 스스로 기억하므로 여기서 별도로 복원할 필요가 없다.)
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->PopCurrentOverlay();
	}
}
