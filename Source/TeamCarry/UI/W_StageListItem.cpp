// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/W_StageListItem.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "TeamCarry/UI/StageListItemData.h"

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
