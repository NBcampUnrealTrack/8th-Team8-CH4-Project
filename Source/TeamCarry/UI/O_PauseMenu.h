// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "O_PauseMenu.generated.h"

class UButton;

/**
 * UO_PauseMenu - In-game pause menu overlay (O_PauseMenu).
 *
 * 명세 3-1: S_InGame / S_Tutorial 에서 ESC 입력 시 스택에 Push 되는 오버레이.
 * UCommonActivatableWidget 을 상속하며, 활성화 시 입력 컨텍스트를 Menu(UI Only)로
 * 전환하여 하위 위젯으로의 입력을 차단한다. (전역 일시정지는 사용하지 않음)
 */
UCLASS()
class TEAMCARRY_API UO_PauseMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UO_PauseMenu();

protected:
	virtual void NativeConstruct() override;

	// 활성화 시 입력을 메뉴(UI Only) 컨텍스트로 제한하여 하위 위젯 입력을 차단한다.
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// --- Pause Menu Buttons ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Resume;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Settings;

	// 수동 저장 버튼: 호스트 전용 (명세 3-1). 프로토타입에서는 권한 로직 위치만 표시한다.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Save;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_ToTitle;

private:
	// Btn_Resume: 메뉴 닫기 → MockController->PopCurrentOverlay()
	UFUNCTION()
	void HandleResumeClicked();

	// Btn_Settings: 설정창 푸시 → MockController->PushOverlay("O_Settings")
	UFUNCTION()
	void HandleSettingsClicked();

	// Btn_Save: 수동 저장 (호스트 전용, 프로토타입은 로그만)
	UFUNCTION()
	void HandleSaveClicked();

	// Btn_ToTitle: 타이틀 복귀 (파괴적 액션 → O_Confirm 경유)
	UFUNCTION()
	void HandleToTitleClicked();
};
