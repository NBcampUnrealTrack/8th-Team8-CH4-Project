// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "S_Result.generated.h"

class UButton;
class UTextBlock;
class UWidget;

/**
 * US_Result - 최종 결과 화면 (명세 3-7).
 *
 * 가구 전량 운반 완료 시 진입하는 풀스크린 정산 화면.
 * - 남은 내구도 기반 0~5 등급, 점수, 팀 보유 금액(Team_Money) 등 통계를 표시한다.
 * - 확인 시 다음 스테이지 선택(S_StageSelect)으로 복귀한다.
 * - 타이틀로(S_MainMenu) 복귀하는 보조 경로도 제공한다(명세 1: 결과 → StageSelect 또는 MainMenu).
 *
 * 실제 점수/등급 산정값은 게임플레이/세이브 연동 단계에서 텍스트에 채운다.
 */
UCLASS()
class TEAMCARRY_API US_Result : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// --- 정산 통계 텍스트 (명세 3-7) ---
	// 최종 점수(등급 포함) 표시.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Score;

	// 팀 보유 금액/세부 통계 표시.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Stats;

	// --- 네비게이션 ---
	// 확인: 스테이지 선택 화면으로 복귀.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Confirm;

	// 타이틀로 복귀(보조 경로).
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_ToTitle;

private:
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleToTitleClicked();
};
