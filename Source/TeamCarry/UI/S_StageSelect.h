// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "S_StageSelect.generated.h"

class UButton;
class UListView;
class UWidget;
class UObject;

/**
 * US_StageSelect - 스테이지 선택 화면 (명세 3-5).
 *
 * 튜토리얼 종료 또는 이어하기로 진입하는 풀스크린 화면.
 * - 플레이 가능한 스테이지 목록을 리스트로 표시한다.
 * - 맵 선택·진입 시 (메모리 세이브 데이터 갱신 후) S_InGame 으로 전환한다.
 * - 뒤로(ESC) 시 O_Confirm 을 거쳐, 진입 경로에 따라 S_Tutorial 또는 S_CharacterSelect(로비)로
 *   복귀한다(튜토리얼 경유 시 → S_Tutorial, 그 외/게임 종료 후 재진입 시 → S_CharacterSelect).
 *
 * 세이브 데이터 갱신(저장)과 스테이지 메타데이터 채움은 백엔드 연동 단계에서 처리한다.
 */
UCLASS()
class TEAMCARRY_API US_StageSelect : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// ESC = '한 단계 뒤로'(명세 5-1). O_Confirm 모달을 거쳐 메인 메뉴로 복귀한다.
	virtual bool NativeOnHandleBackAction() override;

	// --- 스테이지 목록 (명세 3-5) ---
	// 선택 가능한 스테이지 항목을 표시하는 리스트.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UListView> List_Stages;

	// --- 네비게이션 ---
	// 선택한 스테이지로 진입(S_InGame 전환).
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Enter;

	// 뒤로 가기(O_Confirm 경유 메인 메뉴 복귀).
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Back;

private:
	UFUNCTION()
	void HandleEnterClicked();

	UFUNCTION()
	void HandleBackClicked();

	// O_Confirm 팝업에서 '확인'을 눌렀을 때 실행될 브릿지 함수.
	// 진입 경로(MockController->GetPreviousState())에 따라 목적지가 갈린다:
	//  - 튜토리얼(새 게임)에서 진입했다면 → S_Tutorial 로 복귀.
	//  - 그 외(이어하기 방 진입, S_Result 이후 재진입 등)라면 → S_CharacterSelect(로비)로 복귀.
	UFUNCTION()
	void OnConfirmCancelStageSelect();

	// 리스트 항목 클릭 시 호출(네이티브 이벤트). 선택한 스테이지로 곧바로 진입한다.
	void HandleStageItemClicked(UObject* Item);

	// 스테이지 진입 공통 처리: 메모리 세이브 갱신(추후) 후 S_InGame 으로 교체.
	void EnterSelectedStage();
};
