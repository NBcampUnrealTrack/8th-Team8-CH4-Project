// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "S_SlotSelect.generated.h"

class UCommonButtonBase;
class UHorizontalBox;
class UWidget;

/**
 * US_SlotSelect - 게임 선택 슬롯 화면 (명세 3-2).
 *
 * O_JoinRoom 에서 '방 만들기'를 선택했을 때 진입하는 풀스크린 화면.
 * - 맵 썸네일과 진행도가 포함된 가로형 카드 슬롯을 배치한다(이어/새 게임 통합).
 * - 뒤로(ESC) 시 S_MainMenu 로 복귀한다(명세 1 화면 흐름).
 *
 * 슬롯 카드 자체의 생성/선택/삭제 로직은 백엔드(세이브 시스템) 연동 단계에서 채운다.
 * 본 프로토타입은 가로형 카드 컨테이너 바인딩과 화면 라우팅만 담당한다.
 */
UCLASS()
class TEAMCARRY_API US_SlotSelect : public UCommonActivatableWidget
{
	GENERATED_BODY()

	// ==========================================
	// 함수 (Functions) 영역
	// ==========================================
public:
	// (현재 public 함수는 없습니다)

protected:
	virtual void NativeConstruct() override;

	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// ESC = '한 단계 뒤로'(명세 5-1). 라우터를 통해 S_MainMenu 로 교체한다.
	virtual bool NativeOnHandleBackAction() override;

private:
	// 근본 규칙에 따라 매개변수를 완전히 제거했습니다.
	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleTempEmptySlotClicked();

	// ==========================================
	// 변수 (Variables) 영역
	// ==========================================
public:
	// (현재 public 변수는 없습니다)

protected:
	// --- 가로형 카드 배치용 컨테이너 (명세 3-2) ---
	// 세이브 슬롯 카드들이 가로로 나열되는 컨테이너.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UHorizontalBox> Box_SlotCards;

	// --- 네비게이션 ---
	// 뒤로 가기(메인 메뉴 복귀).
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Back;

	// (임시 테스트용) 빈 슬롯 선택 버튼 바인딩. 동적 생성 구현 전 팝업 테스트 용도입니다.
	// 블루프린트에서 해당 버튼을 배치해야 동작하며, 필수가 아니도록 OptionalWidget 처리했습니다.
	UPROPERTY(meta = (BindWidget, OptionalWidget = "true"))
	TObjectPtr<UCommonButtonBase> Btn_TempEmptySlot;

private:
	// (현재 private 변수는 없습니다)
};