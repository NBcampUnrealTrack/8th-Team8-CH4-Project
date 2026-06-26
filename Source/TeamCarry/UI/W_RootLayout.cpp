// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/W_RootLayout.h"

#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

void UW_RootLayout::ShowScreen(TSubclassOf<UCommonActivatableWidget> ScreenClass)
{
	if (!GameLayer || !ScreenClass)
	{
		return;
	}

	// Replace 의미: 기존 풀스크린 화면을 모두 제거한 뒤 새 화면을 올린다.
	GameLayer->ClearWidgets();
	GameLayer->AddWidget(ScreenClass);
}

UCommonActivatableWidget* UW_RootLayout::PushOverlay(TSubclassOf<UCommonActivatableWidget> OverlayClass)
{
	if (!MenuLayer || !OverlayClass)
	{
		return nullptr;
	}

	// 스택이 CreateWidget + 활성화 + 입력 컨텍스트 전환을 자동 처리한다.
	return MenuLayer->AddWidget(OverlayClass);
}

void UW_RootLayout::PopOverlay()
{
	if (!MenuLayer)
	{
		return;
	}

	// 최상단(활성) 오버레이만 제거. 스택이 이전 오버레이를 자동 재활성화한다.
	if (UCommonActivatableWidget* Top = MenuLayer->GetActiveWidget())
	{
		MenuLayer->RemoveWidget(*Top);
	}
}
