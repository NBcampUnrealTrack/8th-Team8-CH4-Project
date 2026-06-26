// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "O_Confirm.generated.h"

class UButton;
class UWidget;

/**
 * UO_Confirm - Confirmation modal overlay (O_Confirm).
 *
 * 명세 3-2: 뒤로 가기 / 종료 / 덮어쓰기 등 파괴적 액션 전 호출되는 강제 모달.
 * - 활성화 시 입력을 Menu(UI Only) 컨텍스트로 제한해 하위 위젯 입력을 차단한다.
 * - 오조작 방지를 위해 기본 포커스는 "아니오"(Btn_Cancel) 버튼에 위치한다.
 */
UCLASS()
class TEAMCARRY_API UO_Confirm : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UO_Confirm();

protected:
	virtual void NativeConstruct() override;

	// 활성화 시 입력을 메뉴(UI Only) 컨텍스트로 제한하여 하위 위젯 입력을 차단한다.
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// 기본 포커스 대상을 "아니오"(Btn_Cancel)로 지정하여 오조작을 방지한다.
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// ESC = '한 단계 뒤로/닫기'(명세 5-1). 기본 동작(DeactivateWidget)은 라우터를 우회해
	// 상태가 어긋나므로, 닫기를 라우터(PopCurrentOverlay)로 위임한다. ESC 는 취소(No)로 간주.
	virtual bool NativeOnHandleBackAction() override;

	// --- Confirm Modal Buttons ---
	// 예 / 확인 (파괴적 액션 수행)
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Confirm;

	// 아니오 / 취소 (기본 포커스)
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Cancel;

private:
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();
};
