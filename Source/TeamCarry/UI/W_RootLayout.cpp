// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/W_RootLayout.h"

#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

void UW_RootLayout::NativeConstruct()
{
	Super::NativeConstruct();

	// UCommonActivatableWidgetContainerBase 의 기본 TransitionDuration(0.4초)은 이전 위젯을
	// 즉시 Deactivate 한 뒤, out-transition 이 절반(기본 0.2초) 진행되고 나서야 새 위젯을
	// Activate 한다. 그 공백 동안 leaf-most 활성 위젯이 일시적으로 GameLayer(예: S_InGame)로
	// 떨어지면서 게임 뷰포트의 마우스 캡처 모드가 실제로 토글되는데, 이 타이밍에 마우스
	// 클릭이 겹치면 Slate 입력/포커스 상태가 꼬여 이후 오버레이를 전부 닫아도 캐릭터/마우스
	// 조작이 복구되지 않는 문제가 있었다(O_PauseMenu 위에서 O_Settings 등을 열었다 닫을 때
	// 재현). 전환을 즉시(0초)로 만들어 Deactivate/Activate 사이의 공백 자체를 없앤다.
	if (GameLayer)
	{
		GameLayer->SetTransitionDuration(0.0f);
	}
	if (MenuLayer)
	{
		MenuLayer->SetTransitionDuration(0.0f);
	}
}

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

void UW_RootLayout::ClearOverlays()
{
	if (MenuLayer)
	{
		MenuLayer->ClearWidgets();
	}
}
