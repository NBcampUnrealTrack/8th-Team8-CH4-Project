// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "O_JoinRoom.generated.h"

class UButton;
class UEditableTextBox;
class UWidget;

/**
 * EJoinRoomMode - 접속 모드 열거형 (명세 6-2).
 *
 * O_JoinRoom 팝업에서 유저의 현재 선택 상태를 추적한다.
 * - None       : 초기 상태. 아무것도 선택되지 않아 Btn_Confirm 비활성화.
 * - CreateRoom : '방 만들기' 선택 상태.
 * - JoinRoom   : '방 참가' 선택 상태(코드 입력 필요).
 */
UENUM(BlueprintType)
enum class EJoinRoomMode : uint8
{
	None,
	CreateRoom,
	JoinRoom
};

/**
 * UO_JoinRoom - 통합 접속 팝업 오버레이 (명세 3-8).
 *
 * 호스트의 방 생성('방 만들기')과 클라이언트의 방 참가('방 참가')를 분기하는 강제 모달.
 * - 활성화 시 입력을 Menu(UI Only) 컨텍스트로 제한해 하위 위젯 입력을 차단한다.
 * - 초기에는 모드 미선택(None) 상태이므로 Btn_Confirm 을 비활성화한다.
 * - '방 참가'의 경우 코드 입력란에 문자열이 존재할 때만 Btn_Confirm 을 활성화한다.
 * - 확인 클릭 시 모드에 따라 라우터(UMockUIController)의 ReplaceState 로 화면을 교체한다.
 */
UCLASS()
class TEAMCARRY_API UO_JoinRoom : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UO_JoinRoom();

protected:
	virtual void NativeConstruct() override;

	// 활성화 시 입력을 메뉴(UI Only) 컨텍스트로 제한하여 하위 위젯 입력을 차단한다(명세 5-2).
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// 기본 포커스 대상을 '방 만들기' 버튼에 둔다.
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// ESC = '한 단계 뒤로/닫기'(명세 5-1). 취소와 동일하게 라우터(PopCurrentOverlay)로 위임한다.
	virtual bool NativeOnHandleBackAction() override;

	// --- 모드 선택 버튼 ---
	// '방 만들기' 선택 버튼.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_CreateRoom;

	// '방 참가' 선택 버튼.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_JoinRoom;

	// --- 코드 입력란 ---
	// 방 참가 시 입력하는 방 코드.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UEditableTextBox> Txt_Code;

	// --- 확정/취소 버튼 ---
	// 확인(선택된 모드로 진행). 조건 충족 전까지 비활성화.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Confirm;

	// 취소(팝업 닫고 S_MainMenu 복귀).
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Cancel;

private:
	UFUNCTION()
	void HandleCreateRoomClicked();

	UFUNCTION()
	void HandleJoinRoomClicked();

	UFUNCTION()
	void HandleCodeTextChanged(const FText& Text);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	// 현재 선택 모드에 따라 모드 버튼 하이라이트와 Btn_Confirm 활성화 상태를 갱신한다.
	void UpdateSelectionVisuals();

	// 확인 버튼 활성화 조건 평가:
	//  - None      : 비활성화
	//  - CreateRoom : 활성화
	//  - JoinRoom   : 코드 입력란에 문자열이 존재할 때만 활성화
	bool ShouldEnableConfirm() const;

	// 현재 선택된 접속 모드(초기 None).
	EJoinRoomMode CurrentMode = EJoinRoomMode::None;
};
