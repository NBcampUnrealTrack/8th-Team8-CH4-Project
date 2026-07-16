// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "W_GameSlotCard_Saved.generated.h"

// 삭제 버튼 클릭 시 브로드캐스트(부모 화면 S_SlotSelect/O_SaveLoad 가 구독해 확인 팝업 →
// UTCSessionFlow::DeleteSaveSlot() 흐름을 잇는다).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameSlotDeleteRequested, const FString&, SlotName);

/**
 * UW_GameSlotCard_Saved - WBP_GameSlot_SavedGame_Button 의 C++ 베이스.
 *
 * 카드 전체(이 클래스 자신, CommonButtonBase)는 "이어하기/이 슬롯에 저장" 클릭을 담당하고,
 * 내부에 중첩된 Btn_Delete 는 별도로 삭제 요청만 브로드캐스트한다 — 실제 삭제 실행(확인 팝업 포함)은
 * 이 위젯을 갖고 있는 화면(S_SlotSelect/O_SaveLoad)이 OnDeleteRequested 를 구독해 처리한다.
 */
UCLASS()
class TEAMCARRY_API UW_GameSlotCard_Saved : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "TeamCarry|Save")
	FOnGameSlotDeleteRequested OnDeleteRequested;

	// 이 카드가 대응하는 세이브 슬롯 이름(부모 화면이 카드 배치 시 채워준다).
	UPROPERTY(BlueprintReadWrite, Category = "TeamCarry|Save")
	FString SlotName;

protected:
	virtual void NativeConstruct() override;

	// 삭제 버튼(WBP_GameSlot_SavedGame_Button 에 이 이름으로 배치해야 바인딩된다).
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Delete;

private:
	UFUNCTION()
	void HandleDeleteClicked();
};
