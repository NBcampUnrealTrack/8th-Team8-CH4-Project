// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "S_Tutorial.generated.h"

class UButton;
class UTextBlock;
class UWidget;

/**
 * US_Tutorial - 튜토리얼 화면 (명세 3-4).
 *
 * NewGame 방에서 최초 1회 진입하는 학습 맵 HUD.
 * - 잡기/이동/놓기/적재 단계를 안내하는 텍스트를 표시한다.
 * - 마지막 Step 완료 또는 '건너뛰기' 시 S_StageSelect 로 직행한다(명세 1 흐름).
 *
 * 단계 진행 판정(1명만 성공해도 다음 단계)은 게임플레이 연동 단계에서 채운다.
 * 본 프로토타입은 단계 안내 텍스트 바인딩과 화면 라우팅만 담당한다.
 */
UCLASS()
class TEAMCARRY_API US_Tutorial : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// --- 단계 안내 텍스트 (명세 3-4) ---
	// 현재 튜토리얼 단계를 표시하는 텍스트(예: "가구를 들어보세요").
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_StepDescription;

	// --- 네비게이션 ---
	// 튜토리얼 건너뛰기(마지막 Step 완료와 동일하게 S_StageSelect 직행).
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Skip;

private:
	UFUNCTION()
	void HandleSkipClicked();
};
