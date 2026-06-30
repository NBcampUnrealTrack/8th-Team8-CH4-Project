// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "O_JoinRoom.generated.h"

class UCommonButtonBase;
class UEditableTextBox;
class UWidget;

/**
 * UO_JoinRoom - 통합 접속 팝업 오버레이 (명세 3-8).
 *
 * 호스트의 방 생성('방 만들기')과 클라이언트의 방 참가('방 참가')를 분기하는 강제 모달.
 * - 방 접속에 두 버튼 중 하나를 클릭하여 바로 다음 UI 화면으로 바로 전환됨.
 * - 생성 -> 게임 슬롯 UI, 참가 -> 캐릭터 선택창 UI
 * - 취소로 현재 모달 팝업을 화면에서 지우며 타이틀 화면으로 전환할 수 있음.
 */
UCLASS()
class TEAMCARRY_API UO_JoinRoom : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UO_JoinRoom();

protected:
	virtual void NativeConstruct() override;

	// 기본 포커스 대상을 '방 만들기' 버튼에 둔다.
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// ESC = '한 단계 뒤로/닫기'(명세 5-1). 취소와 동일하게 라우터(PopCurrentOverlay)로 위임한다.
	virtual bool NativeOnHandleBackAction() override;

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	// --- 모드 선택 버튼 ---
	// '방 만들기' 선택 버튼.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_CreateRoom;

	// '방 참가' 선택 버튼.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_JoinRoom;

	// --- 코드 입력란 ---
	// 방 참가 시 입력하는 방 코드.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UEditableTextBox> Txt_Code;

	// 취소(팝업 닫고 S_MainMenu 복귀).
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Cancel;

private:
	UFUNCTION()
	void HandleCreateRoomClicked();

	UFUNCTION()
	void HandleJoinRoomClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleCodeTextChanged(const FText& Text);

	// 네트워크 세션 델리게이트 수신 핸들러
	UFUNCTION()
	void OnFindSessionsComplete(bool bWasSuccessful, int32 NumResults);

	UFUNCTION()
	void OnJoinSessionComplete(bool bWasSuccessful);
};
