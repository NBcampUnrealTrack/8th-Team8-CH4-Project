// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "O_Confirm.generated.h"

class UCommonButtonBase;
class UTextBlock;
class UWidget;

// '예/확인' 버튼을 눌렀을 때 실행될 동적 델리게이트 선언
DECLARE_DYNAMIC_DELEGATE(FOnConfirmYesAction);

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

	// ==========================================
	// 함수 (Functions) 영역
	// ==========================================
public:
	UO_Confirm();

	// 외부에서 팝업의 제목, 내용, 그리고 실행할 콜백 액션을 주입하는 초기화 함수
	UFUNCTION(BlueprintCallable, Category = "UI|Confirm")
	void SetupConfirm(const FText& InTitle, const FText& InMessage, FOnConfirmYesAction InOnYesAction);

protected:
	virtual void NativeConstruct() override;

	// 활성화 시 입력을 메뉴(UI Only) 컨텍스트로 제한하여 하위 위젯 입력을 차단한다.
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// 기본 포커스 대상을 "아니오"(Btn_Cancel)로 지정하여 오조작을 방지한다.
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// ESC = '한 단계 뒤로/닫기'(명세 5-1). 기본 동작(DeactivateWidget)은 라우터를 우회해
	// 상태가 어긋나므로, 닫기를 라우터(PopCurrentOverlay)로 위임한다. ESC 는 취소(No)로 간주.
	virtual bool NativeOnHandleBackAction() override;

private:
	// 근본 규칙에 따라 매개변수를 완전히 제거했습니다.
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	// ==========================================
	// 변수 (Variables) 영역
	// ==========================================
public:
	// (현재 public 변수는 없습니다)

protected:
	// --- Confirm Modal Texts ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Title;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Message;

	// --- Confirm Modal Buttons ---
	// 예 / 확인 (파괴적 액션 수행)
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Confirm;

	// 아니오 / 취소 (기본 포커스)
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Cancel;

private:
	// 외부에서 주입받은 콜백 함수를 보관하는 변수
	UPROPERTY()
	FOnConfirmYesAction OnYesAction;
};