// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "O_Settings.generated.h"

class UCommonButtonBase;
class UCommonAnimatedSwitcher;
class UO_GraphicsSettings;
class UO_AudioSettings;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSettingClosed);

/**
 * UO_Settings - 설정 창 오버레이 (명세 ⑩ O_Settings).
 *
 * 명세 5-1 라우팅 규칙에 따라 오버레이 스택의 단위가 되므로 UCommonActivatableWidget 을 상속한다.
 * O_PauseMenu 등에서 UMockUIController::PushOverlay("O_Settings") 로 스택에 누적된다.
 *
 * [책임 범위]
 *  - 상단 탭 버튼(UCommonButtonBase)과 하단 UCommonAnimatedSwitcher 의 화면 전환만 관리한다.
 *  - 공통 버튼 [적용(Apply)] / [뒤로가기(Back)] 를 가진다.
 *  - 실제 설정값의 읽기/저장은 하위 탭(UO_GraphicsSettings / UO_AudioSettings)이 자체 캡슐화한다.
 *    [적용] 클릭 시 메인 UI 가 각 탭의 Apply 함수를 순차 호출한다(객체 지향적 위임).
 */
UCLASS()
class TEAMCARRY_API UO_Settings : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UO_Settings();

	// 설정 창이 닫힐 때 외부에 알린다(호출 측에서 포커스 복원 등에 사용).
	UPROPERTY(BlueprintAssignable, Category = "UI|Settings")
	FSettingClosed OnClosed;

	// [적용] 진입점: 모든 하위 탭의 Apply 함수를 순차 호출한다.
	UFUNCTION(BlueprintCallable, Category = "UI|Settings")
	void ApplyAllSettings();

	// [뒤로가기]/ESC 진입점: 스택에서 이 오버레이를 닫는다.
	UFUNCTION(BlueprintCallable, Category = "UI|Settings")
	void CloseSettings();

protected:
	virtual void NativeConstruct() override;

	// ESC(Back 액션) 처리. 명세 5-1: ESC = '한 단계 뒤로/닫기'.
	virtual bool NativeOnHandleBackAction() override;

	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// --- 상단 탭 버튼 (UCommonButtonBase 상속) ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_GraphicsTab;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_AudioTab;

	// --- 공통 버튼 ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Apply;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Back;

	// --- 컨텐츠 화면 전환기 ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonAnimatedSwitcher> ContentSwitcher;

	// --- 하위 탭 위젯 (Switcher 슬롯에 배치된 인스턴스를 바인딩) ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UO_GraphicsSettings> GraphicsTab;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UO_AudioSettings> AudioTab;

private:
	// 탭 버튼의 네이티브 OnClicked 이벤트에 페이로드(탭 인덱스)와 함께 바인딩되는 핸들러.
	void HandleTabClicked(int32 TabIndex);

	// UCommonButtonBase 네이티브 OnClicked 이벤트에 바인딩하는 어댑터들(UFUNCTION 불필요).
	void HandleApplyClicked();
	void HandleBackClicked();

	// 콘텐츠 화면 전환기의 활성 인덱스를 변경한다.
	void ShowTab(int32 TabIndex);
};
