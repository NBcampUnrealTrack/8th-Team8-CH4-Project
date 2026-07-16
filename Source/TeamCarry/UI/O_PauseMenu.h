// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "O_PauseMenu.generated.h"

class UButton;
class UCommonButtonBase;

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

	// ESC = '한 단계 뒤로/닫기'(명세 5-1). 기본 동작(DeactivateWidget)은 라우터를 우회하므로,
	// 닫기를 라우터(PopCurrentOverlay)로 위임해 상태/스택 동기화를 유지한다.
	virtual bool NativeOnHandleBackAction() override;

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

	// Btn_ToLobby: 로비 복귀(호스트 전용, InGame 컨텍스트 전용) → O_Confirm 경유 → HostReturnToLobby()
	UFUNCTION()
	void HandleToLobbyClicked();

	// Btn_Reset: 스테이지 초기화(호스트 전용, InGame 컨텍스트 전용) → O_Confirm 경유 → RestartStage()
	UFUNCTION()
	void HandleResetClicked();

	// Btn_LeaveRoom: 방 나가기(Lobby=전원, InGame=클라이언트 전용, 2026-07-15 InGame 확장) → O_Confirm 경유 → LeaveToTitle()
	UFUNCTION()
	void HandleLeaveRoomClicked();

	// O_Confirm 팝업에서 '확인'을 눌렀을 때 실행될 브릿지 함수들.
	UFUNCTION()
	void OnConfirmReturnToLobby();

	UFUNCTION()
	void OnConfirmResetStage();

	UFUNCTION()
	void OnConfirmLeaveRoom();

	// 호출 컨텍스트(Lobby/Tutorial/InGame)에 따라 Btn_Save/Btn_ToLobby/Btn_Reset/Btn_LeaveRoom의
	// 노출을 갱신한다(v3 내부 개정 — UI_Technical_Spec.md 4장-12 노출 표). 새 enum 없이 기존
	// MockUIController::GetCurrentState()를 그대로 재사용한다.
	void RefreshContextVisibility();

public:
	// --- Pause Menu Buttons ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Resume;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Settings;

	// 수동 저장 버튼: 호스트 전용, InGame 컨텍스트 전용(Tutorial 에서는 강제 비활성화, 명세 4장-12).
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Save;

	// 로비 복귀 버튼: 호스트 전용, InGame 컨텍스트 전용(v3 내부 신규).
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_ToLobby;

	// 리셋 버튼: 호스트 전용, InGame 컨텍스트 전용(v3 내부 신규).
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Reset;

	// 나가기 버튼: Lobby/InGame 컨텍스트 모두 전원(호스트 포함) 노출 → LeaveToTitle().
	// 호스트가 누르면 세션이 파기되어 다른 플레이어 전원이 함께 끊기고, 클라이언트가 누르면
	// 본인만 이탈한다 — LeaveToTitle() 내부에서 호출자 권한에 따라 이미 다르게 처리되므로
	// (OnConfirmLeaveRoom 참고), 게이팅 없이 그대로 노출하고 확인 문구로만 차이를 알린다.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_LeaveRoom;

private:
	float LastMenuToggleTime = 0.0f;
	const float MenuToggleCooldown = 0.2f;
};
