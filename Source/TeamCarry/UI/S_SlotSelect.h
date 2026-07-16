// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "S_SlotSelect.generated.h"

class UCommonButtonBase;
class UCommonAnimatedSwitcher;
class UHorizontalBox;
class UWidget;
class UW_GameSlotCard_Saved;

/**
 * US_SlotSelect - 게임 선택 슬롯 화면 (명세 3-2).
 *
 * O_JoinRoom 에서 '방 만들기'를 선택했을 때 진입하는 풀스크린 화면.
 * - 4개의 슬롯 카드(Switcher_0..3 하위에 Card_New_X/Card_Saved_X 쌍)를 배치한다.
 * - 각 슬롯은 UTCSessionFlow::GetAllSaveSlotInfos() 로 저장 데이터 존재 여부를 조회해,
 *   있으면 Card_Saved(이어하기+삭제), 없으면 Card_New(새 게임)를 보여준다.
 * - 뒤로(ESC) 시 S_MainMenu 로 복귀한다(명세 1 화면 흐름).
 */
UCLASS()
class TEAMCARRY_API US_SlotSelect : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// ESC = '한 단계 뒤로'(명세 5-1). 라우터를 통해 S_MainMenu 로 교체한다.
	virtual bool NativeOnHandleBackAction() override;

private:
	UFUNCTION()
	void HandleBackClicked();

	// 카드 4쌍을 UTCSessionFlow::GetAllSaveSlotInfos() 기준으로 채운다: 저장 데이터 존재 여부로
	// 스위처를 New/Saved 중 하나로 전환하고, 각 카드의 클릭/삭제 이벤트를 바인딩한다.
	void RefreshSlotCards();

	// Card_New_X 클릭(빈 슬롯 → 새 게임) — SlotIndex 는 OnClicked().AddUObject 페이로드로 전달.
	UFUNCTION()
	void HandleNewCardClicked(int32 SlotIndex);

	// Card_Saved_X 클릭(이어하기) — SlotIndex 는 OnClicked().AddUObject 페이로드로 전달.
	UFUNCTION()
	void HandleSavedCardClicked(int32 SlotIndex);

	// Card_Saved_X 의 Btn_Delete 클릭 시 UW_GameSlotCard_Saved::OnDeleteRequested 가 전달.
	UFUNCTION()
	void HandleDeleteRequested(const FString& SlotName);

	// O_Confirm 팝업 '확인' 브릿지들.
	UFUNCTION()
	void OnConfirmNewGame();

	UFUNCTION()
	void OnConfirmContinueGame();

	UFUNCTION()
	void OnConfirmDeleteSlot();

	// O_Confirm 콜백 실행 시 참조할, 마지막으로 연 확인 팝업이 어떤 슬롯에 대한 것인지(팝업은
	// 한 번에 하나만 뜨므로 단일 필드로 충분하다).
	int32 PendingSlotIndex = INDEX_NONE;
	FString PendingDeleteSlotName;

protected:
	// --- 가로형 카드 배치용 컨테이너 (명세 3-2) ---
	// 세이브 슬롯 카드들이 가로로 나열되는 컨테이너.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UHorizontalBox> Box_SlotCards;

	// --- 네비게이션 ---
	// 뒤로 가기(메인 메뉴 복귀).
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Back;

	// --- 슬롯 카드 4쌍(WBP_S_SlotSelect 디자이너에서 이름 통일됨: Switcher_0..3 / Card_New_0..3 /
	// Card_Saved_0..3, 각 Switcher 는 자식 0=Card_New, 1=Card_Saved 순서) ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonAnimatedSwitcher> Switcher_0;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonAnimatedSwitcher> Switcher_1;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonAnimatedSwitcher> Switcher_2;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonAnimatedSwitcher> Switcher_3;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Card_New_0;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Card_New_1;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Card_New_2;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Card_New_3;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UW_GameSlotCard_Saved> Card_Saved_0;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UW_GameSlotCard_Saved> Card_Saved_1;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UW_GameSlotCard_Saved> Card_Saved_2;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UW_GameSlotCard_Saved> Card_Saved_3;

	// 슬롯 카드(BP)에서 슬롯 확정 시 호출하는 seam.
	// SlotName = 세이브 슬롯 식별자, bContinue = 이어하기 여부(저장 데이터 존재).
	// 내부에서 UTCSessionFlow 에 세이브 선택을 저장하고 방 생성(호스트)을 시작한다.
	UFUNCTION(BlueprintCallable, Category = "UI|Slot")
	void ConfirmSlotAndCreateRoom(const FString& SlotName, bool bContinue);

};
