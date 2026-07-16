// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/W_GameSlotCard_Saved.h"

void UW_GameSlotCard_Saved::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Delete)
	{
		Btn_Delete->OnClicked().RemoveAll(this);
		Btn_Delete->OnClicked().AddUObject(this, &UW_GameSlotCard_Saved::HandleDeleteClicked);
	}
}

void UW_GameSlotCard_Saved::HandleDeleteClicked()
{
	OnDeleteRequested.Broadcast(SlotName);
}
