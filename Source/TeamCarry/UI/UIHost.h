// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TeamCarry/UI/MockUIController.h"   // EE_UIState
#include "UIHost.generated.h"

class UCommonActivatableWidget;

UINTERFACE(MinimalAPI)
class UUIHost : public UInterface
{
	GENERATED_BODY()
};

/**
 * IUIHost - 라우터(UMockUIController)와 실제 UI 생성 주체(PlayerController)를 분리하는 위임 인터페이스.
 *
 * 명세 5-1: 모든 화면 전환은 UMockUIController 를 통해서만 수행한다(단일 경로).
 * 단, "무엇을/어떤 순서로"(상태·스택)는 라우터가, "실제로 어떻게"(CreateWidget/AddToViewport)는
 * 호스트(PlayerController)가 책임진다. 라우터는 구체 PC 타입을 모르고 이 인터페이스만 안다.
 */
class TEAMCARRY_API IUIHost
{
	GENERATED_BODY()

public:
	// 풀스크린 화면 교체(Replace). 라우터의 ReplaceState 가 호출한다.
	virtual void ShowState(EE_UIState NewState) = 0;

	// 오버레이 1개 생성(Push). 성공 시 생성된 위젯, 실패 시 nullptr(라우터가 상태를 롤백).
	virtual UCommonActivatableWidget* ShowOverlay(FName OverlayId) = 0;

	// 최상단 오버레이 1개 제거(Pop).
	virtual void HideTopOverlay() = 0;
};
