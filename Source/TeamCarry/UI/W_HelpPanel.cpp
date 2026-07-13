// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/W_HelpPanel.h"
#include "Components/TextBlock.h"

void UW_HelpPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (Txt_HelpContent)
	{
		FString Joined;
		for (const FText& Line : HelpLines)
		{
			if (!Joined.IsEmpty())
			{
				Joined += TEXT("\n");
			}
			Joined += Line.ToString();
		}
		Txt_HelpContent->SetText(FText::FromString(Joined));
	}
}
