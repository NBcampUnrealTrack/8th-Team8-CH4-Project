// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "O_SaveLoad.generated.h"

class UCommonButtonBase;
class UCommonAnimatedSwitcher;
class UHorizontalBox;
class UWidget;
class UW_GameSlotCard_Saved;

/**
 * UO_SaveLoad - 저장/불러오기 오버레이 (명세 3-12).
 *
 * O_PauseMenu 의 [수동 저장] 선택 시 스택에 Push 되는 오버레이.
 * - 4개의 슬롯 카드(Switcher_0..3 하위에 Card_New_X/Card_Saved_X 쌍)를 배치한다.
 * - 각 슬롯은 UTCSessionFlow::GetAllSaveSlotInfos() 로 저장 데이터 존재 여부를 조회해,
 *   있으면 Card_Saved(덮어쓰기+삭제), 없으면 Card_New(신규 기록)를 보여준다.
 * - New/Saved 카드 클릭은 둘 다 ATeamCarryGameMode::SaveGameToSlot() 으로 현재 진행도를 그
 *   슬롯에 저장한다(다른 점은 확인 문구뿐 — 덮어쓰기는 명시적으로 경고한다).
 * - 활성화 시 입력을 Menu(UI Only) 컨텍스트로 제한해 하위(게임) 입력을 차단한다(명세 5-2).
 * - 취소/닫기 또는 ESC 시 라우터(PopCurrentOverlay)로 닫아 상태/스택 동기화를 유지한다.
 */
UCLASS()
class TEAMCARRY_API UO_SaveLoad : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UO_SaveLoad();

protected:
	virtual void NativeConstruct() override;

	// 활성화 시 입력을 메뉴(UI Only) 컨텍스트로 제한하여 하위 위젯 입력을 차단한다(명세 5-2).
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// 기본 포커스 대상을 닫기(취소) 버튼에 둔다.
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// ESC = '한 단계 뒤로/닫기'(명세 5-1). 라우터(PopCurrentOverlay)로 위임한다.
	virtual bool NativeOnHandleBackAction() override;

	// --- 저장 슬롯 컨테이너 (명세 3-12) ---
	// 기존 슬롯(덮어쓰기)과 빈 슬롯(신규 기록) 카드가 가로로 나열되는 컨테이너.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UHorizontalBox> Box_SaveSlots;

	// --- 네비게이션 ---
	// 취소/닫기(오버레이만 Pop).
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Cancel;

	// --- 슬롯 카드 4쌍(WBP_O_SaveLoad 디자이너에서 이름 통일됨: Switcher_0..3 / Card_New_0..3 /
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

private:
	UFUNCTION()
	void HandleCancelClicked();

	// 카드 4쌍을 UTCSessionFlow::GetAllSaveSlotInfos() 기준으로 채운다.
	void RefreshSlotCards();

	// Card_New_X / Card_Saved_X 클릭 — SlotIndex 는 OnClicked().AddUObject 페이로드로 전달.
	// 신규 기록/덮어쓰기 모두 같은 저장 실행으로 이어지되 확인 문구만 다르다.
	UFUNCTION()
	void HandleNewCardClicked(int32 SlotIndex);

	UFUNCTION()
	void HandleSavedCardClicked(int32 SlotIndex);

	// Card_Saved_X 의 Btn_Delete 클릭 시 UW_GameSlotCard_Saved::OnDeleteRequested 가 전달.
	UFUNCTION()
	void HandleDeleteRequested(const FString& SlotName);

	// O_Confirm 팝업 '확인' 브릿지들.
	UFUNCTION()
	void OnConfirmSaveToSlot();

	UFUNCTION()
	void OnConfirmDeleteSlot();

	// O_Confirm 콜백 실행 시 참조할, 마지막으로 연 확인 팝업이 어떤 슬롯에 대한 것인지.
	int32 PendingSlotIndex = INDEX_NONE;
	FString PendingDeleteSlotName;
};
