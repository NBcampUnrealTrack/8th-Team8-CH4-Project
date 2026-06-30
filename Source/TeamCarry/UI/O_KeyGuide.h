// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "O_KeyGuide.generated.h"

class UCommonButtonBase;

/**
 * UO_KeyGuide - 조작법 가이드 오버레이 팝업 (명세 13 O_KeyGuide).
 *
 * 명세 5-1: 독립 오버레이 팝업이므로 UCommonActivatableWidget 을 상속한다.
 * O_PauseMenu 의 Btn_KeyGuide 클릭 시 UMockUIController::PushOverlay("O_KeyGuide") 로 스택에 Push 된다.
 *
 * 활성화 시 GetDesiredInputConfig() 를 통해 게임 입력을 차단하고 UI 전용(Menu) 입력으로 전환한다.
 * Btn_Back 클릭 또는 ESC 키 입력(NativeOnHandleBackAction) 시
 * UMockUIController::PopCurrentOverlay() 를 호출하여 O_PauseMenu 로 복귀한다.
 */
UCLASS()
class TEAMCARRY_API UO_KeyGuide : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UO_KeyGuide();

protected:
	virtual void NativeConstruct() override;

	// 활성화 시 입력을 메뉴(UI Only) 컨텍스트로 제한하여 게임 입력을 차단한다.
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// ESC = '한 단계 뒤로/닫기' (명세 5-1).
	// 기본 동작(DeactivateWidget)은 라우터를 우회하므로 PopCurrentOverlay() 로 위임한다.
	virtual bool NativeOnHandleBackAction() override;

	// 닫기 버튼 (명세 5-5: 버튼 클래스는 UCommonButtonBase 로 통일).
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Back;

private:
	// Btn_Back 클릭 및 ESC 공통 종료 로직: 라우터를 통해 팝업을 닫는다.
	void CloseKeyGuide();

	void HandleBackClicked();
};
