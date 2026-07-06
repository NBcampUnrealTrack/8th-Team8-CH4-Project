// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/W_StageCard.h"
#include "Components/TextBlock.h"
#include "TeamCarry/UI/StageListItemData.h"

void UW_StageCard::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	const UStageListItemData* StageItem = Cast<UStageListItemData>(ListItemObject);
	if (Txt_DisplayName && StageItem)
	{
		Txt_DisplayName->SetText(StageItem->DisplayName);
	}
}
