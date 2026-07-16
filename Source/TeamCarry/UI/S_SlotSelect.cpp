// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/S_SlotSelect.h"
#include "TeamCarry/UI/O_Confirm.h"
#include "TeamCarry/UI/W_GameSlotCard_Saved.h"
#include "CommonButtonBase.h"
#include "CommonAnimatedSwitcher.h"
#include "Components/HorizontalBox.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Network/Session/TCSessionFlow.h"
#include "Engine/GameInstance.h"

namespace
{
	// Switcher_X 의 자식 순서(WBP 디자이너에서 고정): 0=Card_New, 1=Card_Saved.
	constexpr int32 SlotSelect_SwitcherIndex_New = 0;
	constexpr int32 SlotSelect_SwitcherIndex_Saved = 1;
}

void US_SlotSelect::NativeConstruct()
{
	Super::NativeConstruct();

	// CommonUI의 네이티브 바인딩 방식인 OnClicked().AddUObject 를 적용했습니다.
	if (Btn_Back)
	{
		Btn_Back->OnClicked().AddUObject(this, &US_SlotSelect::HandleBackClicked);
	}

	if (!Box_SlotCards)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI SlotSelect] Box_SlotCards is not bound. Check the WBP hierarchy."));
	}

	RefreshSlotCards();

	SetIsFocusable(true);
}

void US_SlotSelect::RefreshSlotCards()
{
	UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr;
	if (!Flow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI SlotSelect] UTCSessionFlow 없음 — 슬롯 카드를 채울 수 없음"));
		return;
	}

	const TArray<FSaveSlotInfo> SlotInfos = Flow->GetAllSaveSlotInfos();

	UCommonAnimatedSwitcher* Switchers[] = { Switcher_0, Switcher_1, Switcher_2, Switcher_3 };
	UCommonButtonBase* NewCards[] = { Card_New_0, Card_New_1, Card_New_2, Card_New_3 };
	UW_GameSlotCard_Saved* SavedCards[] = { Card_Saved_0, Card_Saved_1, Card_Saved_2, Card_Saved_3 };

	const int32 NumSlots = FMath::Min(SlotInfos.Num(), 4);
	for (int32 Index = 0; Index < NumSlots; ++Index)
	{
		const FSaveSlotInfo& Info = SlotInfos[Index];

		if (Switchers[Index])
		{
			Switchers[Index]->SetActiveWidgetIndex(Info.bHasSaveData ? SlotSelect_SwitcherIndex_Saved : SlotSelect_SwitcherIndex_New);
		}

		if (NewCards[Index])
		{
			NewCards[Index]->OnClicked().AddUObject(this, &US_SlotSelect::HandleNewCardClicked, Index);
		}

		if (SavedCards[Index])
		{
			SavedCards[Index]->SlotName = Info.SlotName;
			SavedCards[Index]->OnClicked().AddUObject(this, &US_SlotSelect::HandleSavedCardClicked, Index);
			SavedCards[Index]->OnDeleteRequested.AddUniqueDynamic(this, &US_SlotSelect::HandleDeleteRequested);
		}
	}
}

UWidget* US_SlotSelect::NativeGetDesiredFocusTarget() const
{
	// 화면 진입 시 첫 슬롯에 기본 포커스를 부여합니다.
	if (Switcher_0)
	{
		return Switcher_0->GetActiveWidget();
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

void US_SlotSelect::HandleNewCardClicked(int32 SlotIndex)
{
	PendingSlotIndex = SlotIndex;

	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI SlotSelect] New card %d clicked. Pushing O_Confirm overlay."), SlotIndex);

		UCommonActivatableWidget* OverlayWidget = MockController->PushOverlay(TEXT("O_Confirm"));

		if (UO_Confirm* ConfirmUI = Cast<UO_Confirm>(OverlayWidget))
		{
			FOnConfirmYesAction YesAction;
			YesAction.BindDynamic(this, &US_SlotSelect::OnConfirmNewGame);

			ConfirmUI->SetupConfirm(
				FText::FromString(TEXT("새 게임")),
				FText::FromString(TEXT("새로운 게임을 생성하시겠습니까?")),
				YesAction
			);
		}
	}
}

void US_SlotSelect::OnConfirmNewGame()
{
	if (PendingSlotIndex == INDEX_NONE)
	{
		return;
	}
	ConfirmSlotAndCreateRoom(UTCSessionFlow::MakeSaveSlotName(PendingSlotIndex), false);
	PendingSlotIndex = INDEX_NONE;
}

void US_SlotSelect::HandleSavedCardClicked(int32 SlotIndex)
{
	PendingSlotIndex = SlotIndex;

	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI SlotSelect] Saved card %d clicked. Pushing O_Confirm overlay."), SlotIndex);

		UCommonActivatableWidget* OverlayWidget = MockController->PushOverlay(TEXT("O_Confirm"));

		if (UO_Confirm* ConfirmUI = Cast<UO_Confirm>(OverlayWidget))
		{
			FOnConfirmYesAction YesAction;
			YesAction.BindDynamic(this, &US_SlotSelect::OnConfirmContinueGame);

			ConfirmUI->SetupConfirm(
				FText::FromString(TEXT("이어하기")),
				FText::FromString(TEXT("이 게임을 이어하시겠습니까?")),
				YesAction
			);
		}
	}
}

void US_SlotSelect::OnConfirmContinueGame()
{
	if (PendingSlotIndex == INDEX_NONE)
	{
		return;
	}
	ConfirmSlotAndCreateRoom(UTCSessionFlow::MakeSaveSlotName(PendingSlotIndex), true);
	PendingSlotIndex = INDEX_NONE;
}

void US_SlotSelect::HandleDeleteRequested(const FString& SlotName)
{
	PendingDeleteSlotName = SlotName;

	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI SlotSelect] Delete requested for slot '%s'. Pushing O_Confirm overlay."), *SlotName);

		UCommonActivatableWidget* OverlayWidget = MockController->PushOverlay(TEXT("O_Confirm"));

		if (UO_Confirm* ConfirmUI = Cast<UO_Confirm>(OverlayWidget))
		{
			FOnConfirmYesAction YesAction;
			YesAction.BindDynamic(this, &US_SlotSelect::OnConfirmDeleteSlot);

			ConfirmUI->SetupConfirm(
				FText::FromString(TEXT("저장 데이터 삭제")),
				FText::FromString(TEXT("정말로 이 저장 데이터를 삭제하시겠습니까? 되돌릴 수 없습니다.")),
				YesAction
			);
		}
	}
}

void US_SlotSelect::OnConfirmDeleteSlot()
{
	if (PendingDeleteSlotName.IsEmpty())
	{
		return;
	}

	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		Flow->DeleteSaveSlot(PendingDeleteSlotName);
	}
	PendingDeleteSlotName.Empty();

	// 삭제 후 스위처/카드 상태를 실제 저장 데이터 유무에 맞게 다시 채운다(삭제된 슬롯은 New로 전환).
	RefreshSlotCards();
}
