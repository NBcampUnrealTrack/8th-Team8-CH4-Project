// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/W_StageBoardScreen.h"
#include "Components/TextBlock.h"
#include "Components/ListView.h"
#include "CommonButtonBase.h"
#include "Network/Session/TCLobbyGameState.h"
#include "Network/Session/TCSessionFlow.h"
#include "TeamCarry/UI/StageListItemData.h"
#include "Player/PlayerController/TCPlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

void UW_StageBoardScreen::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Confirm)
	{
		Btn_Confirm->OnClicked().AddUObject(this, &UW_StageBoardScreen::HandleConfirmClicked);
	}
	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked().AddUObject(this, &UW_StageBoardScreen::HandleCancelClicked);
	}
	if (List_Stages)
	{
		List_Stages->OnItemClicked().AddUObject(this, &UW_StageBoardScreen::HandleStageItemClicked);
	}

	PendingSelectedStageId = 0;
	PopulateStageList();

	if (!TryBindLobbyState())
	{
		// 클라이언트는 로비 GameState 복제가 1틱 늦게 도착할 수 있다(S_Lobby::TryBindNetworkLobby와 동일 사유).
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					TryBindLobbyState();
				}));
		}
	}
}

void UW_StageBoardScreen::NativeDestruct()
{
	if (List_Stages)
	{
		List_Stages->OnItemClicked().RemoveAll(this);
	}
	if (ATCLobbyGameState* LobbyGS = BoundLobbyState.Get())
	{
		LobbyGS->OnSelectedStageChanged.RemoveAll(this);
	}
	BoundLobbyState = nullptr;

	Super::NativeDestruct();
}

bool UW_StageBoardScreen::TryBindLobbyState()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	ATCLobbyGameState* LobbyGS = World->GetGameState<ATCLobbyGameState>();
	if (!LobbyGS)
	{
		return false;
	}
	LobbyGS->OnSelectedStageChanged.AddUniqueDynamic(this, &UW_StageBoardScreen::HandleSelectedStageChanged);
	BoundLobbyState = LobbyGS;
	RefreshDisplay(LobbyGS->GetSelectedStageId());
	return true;
}

void UW_StageBoardScreen::HandleSelectedStageChanged(int32 NewStageId)
{
	RefreshDisplay(NewStageId);
}

void UW_StageBoardScreen::RefreshDisplay(int32 RawStageId)
{
	// 미선택(0 이하)이면 기본 1스테이지로 폴백(UTCSessionFlow::GetSelectedStageId()와 동일 규칙).
	const int32 EffectiveStageId = RawStageId > 0 ? RawStageId : 1;

	FText StageName = FText::Format(NSLOCTEXT("StageBoardScreen", "StageFallbackFormat", "스테이지 {0}"), FText::AsNumber(EffectiveStageId));
	if (const UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr)
	{
		FStageInfo Info;
		if (Flow->FindStageInfo(EffectiveStageId, Info) && !Info.DisplayName.IsEmpty())
		{
			StageName = Info.DisplayName;
		}
	}

	if (Txt_StageName)
	{
		Txt_StageName->SetText(FText::Format(NSLOCTEXT("StageBoardScreen", "SelectedStageFormat", "선택됨: {0}"), StageName));
	}

	// 확정된 선택으로 목록 하이라이트도 동기화(아직 아무도 하이라이트를 바꾸지 않은 다른 클라이언트의
	// 화면에도 "현재 공식 선택"이 반영되도록).
	if (List_Stages)
	{
		for (UObject* ListItem : List_Stages->GetListItems())
		{
			if (const UStageListItemData* StageItem = Cast<UStageListItemData>(ListItem))
			{
				if (StageItem->StageId == EffectiveStageId)
				{
					List_Stages->SetSelectedItem(ListItem);
					break;
				}
			}
		}
	}
}

void UW_StageBoardScreen::PopulateStageList()
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

void UW_StageBoardScreen::HandleStageItemClicked(UObject* Item)
{
	if (const UStageListItemData* StageItem = Cast<UStageListItemData>(Item))
	{
		PendingSelectedStageId = StageItem->StageId;
		// 시각적 하이라이트(확장 개방 — 실제 선택 스타일은 카드 위젯 쪽 추후 작업).
		List_Stages->SetSelectedItem(Item);
	}
}

void UW_StageBoardScreen::HandleConfirmClicked()
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

	// 리슨 서버 구조상 게시판 클릭 모드는 호스트 자신만 진입하므로, 로컬(첫) 플레이어 컨트롤러가
	// 곧 그 호스트다(BP_StageSelectBoard::OnInteract_Implementation 과 동일 가정).
	if (const UWorld* World = GetWorld())
	{
		if (ATCPlayerController* PC = Cast<ATCPlayerController>(World->GetFirstPlayerController()))
		{
			PC->ExitBoardInteractionMode();
		}
	}
}

void UW_StageBoardScreen::HandleCancelClicked()
{
	// 선택을 변경하지 않고 닫는다 — SetStageSelection() 을 호출하지 않는다.
	if (const UWorld* World = GetWorld())
	{
		if (ATCPlayerController* PC = Cast<ATCPlayerController>(World->GetFirstPlayerController()))
		{
			PC->ExitBoardInteractionMode();
		}
	}
}
