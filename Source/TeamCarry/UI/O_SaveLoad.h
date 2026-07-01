// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "O_SaveLoad.generated.h"

class UCommonButtonBase;
class UHorizontalBox;
class UWidget;

/**
 * UO_SaveLoad - 저장/불러오기 오버레이 (명세 3-12).
 *
 * O_PauseMenu 의 [수동 저장] 선택 시 스택에 Push 되는 오버레이.
 * - 현재 진행도를 기존 슬롯에 덮어쓰거나 빈 슬롯에 기록하는 역할만 수행한다.
 * - 활성화 시 입력을 Menu(UI Only) 컨텍스트로 제한해 하위(게임) 입력을 차단한다(명세 5-2).
 * - 취소/닫기 또는 ESC 시 라우터(PopCurrentOverlay)로 닫아 상태/스택 동기화를 유지한다.
 *
 * 실제 디스크 저장(USaveGame) 로직은 백엔드 연동 단계에서 채운다.
 * 본 프로토타입은 슬롯 컨테이너 바인딩과 닫기 라우팅만 담당한다.
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

private:
	UFUNCTION()
	void HandleCancelClicked();
};
