// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/W_StageListItem.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "TeamCarry/UI/StageListItemData.h"

namespace
{
	// List_Stages 에서 항목이 선택되었을 때 Border_Selected 에 적용하는 골드 강조색.
	const FLinearColor SelectedBorderColor(1.f, 0.83f, 0.f, 1.f);
}

void UW_StageListItem::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	const UStageListItemData* StageItem = Cast<UStageListItemData>(ListItemObject);
	if (!StageItem)
	{
		return;
	}

	if (TextBlock_StageName)
	{
		TextBlock_StageName->SetText(StageItem->DisplayName);
	}
	// Image_Stage: FStageInfo 에 썸네일 필드가 아직 없어 현재는 채우지 않는다(7장-3, 확장 개방).
}

void UW_StageListItem::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);

	if (Border_Selected)
	{
		Border_Selected->SetBrushColor(bIsSelected ? SelectedBorderColor : FLinearColor::Transparent);
	}
}
