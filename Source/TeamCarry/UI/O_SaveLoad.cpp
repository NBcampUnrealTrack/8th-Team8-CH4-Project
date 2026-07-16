// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/O_SaveLoad.h"
#include "TeamCarry/UI/O_Confirm.h"
#include "TeamCarry/UI/W_GameSlotCard_Saved.h"
#include "CommonButtonBase.h"
#include "CommonAnimatedSwitcher.h"
#include "Components/HorizontalBox.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Network/Session/TCSessionFlow.h"
#include "Core/TeamCarryGameMode.h"
#include "Input/CommonUIInputTypes.h"
#include "Engine/World.h"

namespace
{
	// Switcher_X 의 자식 순서(WBP 디자이너에서 고정): 0=Card_New, 1=Card_Saved.
	constexpr int32 SwitcherIndex_New = 0;
	constexpr int32 SwitcherIndex_Saved = 1;
}

UO_SaveLoad::UO_SaveLoad()
{
	// 오버레이는 스택에 누적되며, Back(ESC) 입력을 직접 처리한다.
	bIsBackHandler = true;
	// 활성화 시 포커스를 가져와 하위 위젯이 입력 포커스를 잃도록 한다.
	bSupportsActivationFocus = true;
}

void UO_SaveLoad::NativeConstruct()
{
	Super::NativeConstruct();

	// 취소/닫기 버튼 바인딩.
	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked().RemoveAll(this);
		Btn_Cancel->OnClicked().AddUObject(this, &UO_SaveLoad::HandleCancelClicked);
	}

	if (!Box_SaveSlots)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI SaveLoad] Box_SaveSlots is not bound. Check the WBP hierarchy."));
	}

	RefreshSlotCards();
}

void UO_SaveLoad::RefreshSlotCards()
{
	UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr;
	if (!Flow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI SaveLoad] UTCSessionFlow 없음 — 슬롯 카드를 채울 수 없음"));
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
			Switchers[Index]->SetActiveWidgetIndex(Info.bHasSaveData ? SwitcherIndex_Saved : SwitcherIndex_New);
		}

		if (NewCards[Index])
		{
			NewCards[Index]->OnClicked().AddUObject(this, &UO_SaveLoad::HandleNewCardClicked, Index);
		}

		if (SavedCards[Index])
		{
			SavedCards[Index]->SlotName = Info.SlotName;
			SavedCards[Index]->OnClicked().AddUObject(this, &UO_SaveLoad::HandleSavedCardClicked, Index);
			SavedCards[Index]->OnDeleteRequested.AddUniqueDynamic(this, &UO_SaveLoad::HandleDeleteRequested);
		}
	}
}

TOptional<FUIInputConfig> UO_SaveLoad::GetDesiredInputConfig() const
{
	// Menu 모드 = UI Only. 오버레이가 떠 있는 동안 게임 입력이 차단된다(명세 5-2).
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

UWidget* UO_SaveLoad::NativeGetDesiredFocusTarget() const
{
	if (Btn_Cancel)
	{
		return Btn_Cancel;
	}

	return Super::NativeGetDesiredFocusTarget();
}

bool UO_SaveLoad::NativeOnHandleBackAction()
{
	// ESC = 취소/닫기와 동일. 라우터를 통해 닫아 상태/스택 동기화를 유지한다(명세 5-1).
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI SaveLoad] Back(ESC) handled. Popping via MockUIController."));
		MockController->PopCurrentOverlay();
	}
	// 처리했음을 알려 상위 스택으로 Back 전파를 막는다.
	return true;
}

void UO_SaveLoad::HandleCancelClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// 오버레이만 닫는다(하위 O_PauseMenu 로 복귀).
		UE_LOG(LogTemp, Log, TEXT("[UI SaveLoad] Cancel clicked. Popping overlay."));
		MockController->PopCurrentOverlay();
	}
}

void UO_SaveLoad::HandleNewCardClicked(int32 SlotIndex)
{
	PendingSlotIndex = SlotIndex;

	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UCommonActivatableWidget* OverlayWidget = MockController->PushOverlay(TEXT("O_Confirm"));

		if (UO_Confirm* ConfirmUI = Cast<UO_Confirm>(OverlayWidget))
		{
			FOnConfirmYesAction YesAction;
			YesAction.BindDynamic(this, &UO_SaveLoad::OnConfirmSaveToSlot);

			ConfirmUI->SetupConfirm(
				FText::FromString(TEXT("게임 저장")),
				FText::FromString(TEXT("현재 진행 상황을 이 슬롯에 저장하시겠습니까?")),
				YesAction
			);
		}
	}
}

void UO_SaveLoad::HandleSavedCardClicked(int32 SlotIndex)
{
	PendingSlotIndex = SlotIndex;

	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UCommonActivatableWidget* OverlayWidget = MockController->PushOverlay(TEXT("O_Confirm"));

		if (UO_Confirm* ConfirmUI = Cast<UO_Confirm>(OverlayWidget))
		{
			FOnConfirmYesAction YesAction;
			YesAction.BindDynamic(this, &UO_SaveLoad::OnConfirmSaveToSlot);

			// 기존 슬롯 위에 겹쳐 쓰는 파괴적 액션이므로, 새 슬롯 저장과 문구를 다르게 경고한다.
			ConfirmUI->SetupConfirm(
				FText::FromString(TEXT("덮어쓰기")),
				FText::FromString(TEXT("정말로 이 슬롯을 덮어쓰시겠습니까? 기존 저장 데이터가 사라집니다.")),
				YesAction
			);
		}
	}
}

void UO_SaveLoad::OnConfirmSaveToSlot()
{
	if (PendingSlotIndex == INDEX_NONE)
	{
		return;
	}

	if (ATeamCarryGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATeamCarryGameMode>() : nullptr)
	{
		GM->SaveGameToSlot(UTCSessionFlow::MakeSaveSlotName(PendingSlotIndex));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI SaveLoad] ATeamCarryGameMode 없음 — 저장 불가"));
	}

	PendingSlotIndex = INDEX_NONE;

	// 방금 저장한 슬롯이 New→Saved 로 바뀌었을 수 있으므로 스위처/카드를 다시 채운다.
	RefreshSlotCards();
}

void UO_SaveLoad::HandleDeleteRequested(const FString& SlotName)
{
	PendingDeleteSlotName = SlotName;

	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UCommonActivatableWidget* OverlayWidget = MockController->PushOverlay(TEXT("O_Confirm"));

		if (UO_Confirm* ConfirmUI = Cast<UO_Confirm>(OverlayWidget))
		{
			FOnConfirmYesAction YesAction;
			YesAction.BindDynamic(this, &UO_SaveLoad::OnConfirmDeleteSlot);

			ConfirmUI->SetupConfirm(
				FText::FromString(TEXT("저장 데이터 삭제")),
				FText::FromString(TEXT("정말로 이 저장 데이터를 삭제하시겠습니까? 되돌릴 수 없습니다.")),
				YesAction
			);
		}
	}
}

void UO_SaveLoad::OnConfirmDeleteSlot()
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

	RefreshSlotCards();
}
