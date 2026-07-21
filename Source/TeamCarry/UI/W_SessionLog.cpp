// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/W_SessionLog.h"
#include "Components/TextBlock.h"
#include "TeamCarry/UI/MockUIController.h"

void UW_SessionLog::NativeConstruct()
{
	Super::NativeConstruct();

	if (UMockUIController* MockController = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr)
	{
		MockController->OnSessionLogAdded.AddUniqueDynamic(this, &UW_SessionLog::HandleSessionLogAdded);
	}
}

void UW_SessionLog::NativeDestruct()
{
	if (UMockUIController* MockController = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr)
	{
		MockController->OnSessionLogAdded.RemoveDynamic(this, &UW_SessionLog::HandleSessionLogAdded);
	}

	Super::NativeDestruct();
}

void UW_SessionLog::HandleSessionLogAdded(FText LogMessage)
{
	DisplayedLines.Add(LogMessage);
	while (DisplayedLines.Num() > MaxDisplayedLines)
	{
		DisplayedLines.RemoveAt(0);
	}
	RefreshDisplayText();
}

void UW_SessionLog::RefreshDisplayText()
{
	if (!Txt_Log)
	{
		return;
	}

	FString Joined;
	for (int32 i = 0; i < DisplayedLines.Num(); ++i)
	{
		Joined += DisplayedLines[i].ToString();
		if (i < DisplayedLines.Num() - 1)
		{
			Joined += TEXT("\n");
		}
	}
	Txt_Log->SetText(FText::FromString(Joined));
}
