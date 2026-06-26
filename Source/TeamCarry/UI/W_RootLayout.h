// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "W_RootLayout.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;

/**
 * UW_RootLayout - PlayerController 가 소유하는 단일 루트 레이아웃.
 *
 * CommonUI 의 UCommonActivatableWidgetStack 두 개를 레이어로 보유한다.
 *  - GameLayer (ZOrder 하단): 풀스크린 화면(S_*). 명세 5-1에 따라 교체(Replace) 방식.
 *  - MenuLayer (ZOrder 상단): 오버레이/모달(O_*). 누적(Push/Pop) 방식.
 *
 * 스택이 CreateWidget/제거/포커스/입력 컨텍스트 전환을 자동 처리하므로,
 * 본 클래스는 "어느 레이어에 무엇을 올릴지"만 노출한다. 클래스 결정은 PlayerController 가 한다.
 *
 * [UMG 제작 규칙] WBP_RootLayout 에 동일한 이름(GameLayer / MenuLayer)의
 *  Common Activatable Widget Stack 위젯 2개를 배치해야 BindWidget 이 성립한다.
 */
UCLASS()
class TEAMCARRY_API UW_RootLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 풀스크린 화면 교체: 게임 레이어 스택을 비우고 새 화면을 올린다(Replace).
	void ShowScreen(TSubclassOf<UCommonActivatableWidget> ScreenClass);

	// 오버레이 누적: 메뉴 레이어 스택에 Push. 생성된 위젯 반환(실패 시 nullptr).
	UCommonActivatableWidget* PushOverlay(TSubclassOf<UCommonActivatableWidget> OverlayClass);

	// 최상단 오버레이 Pop.
	void PopOverlay();

protected:
	// 풀스크린 화면 레이어(하단).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> GameLayer;

	// 오버레이/모달 레이어(상단).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> MenuLayer;
};
