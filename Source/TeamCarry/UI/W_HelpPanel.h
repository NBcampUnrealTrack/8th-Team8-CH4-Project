// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "W_HelpPanel.generated.h"

class UTextBlock;

/**
 * UW_HelpPanel - 인게임 도움말/조작 팁 패널(명세 3장·4장-7·4장-15). 구 O_KeyGuide 팝업 대체.
 *
 * S_InGame 화면 우측 중단부터 하단까지 상시 노출되는 단순 위젯(버튼과 같은 급, UCommonUserWidget).
 * 팝업이 아니므로 게임 입력을 차단하지 않으며 열고 닫는 개념이 없다. 콘텐츠가 정적이므로
 * 델리게이트 구독 없이 EditDefaultsOnly 로 노출된 줄 목록을 NativeConstruct 시점에 합쳐 표시한다.
 */
UCLASS()
class TEAMCARRY_API UW_HelpPanel : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_HelpContent;

	// 조작 키 안내 + 게임 팁 문구(줄 단위). 블루프린트 디폴트에서 스테이지에 맞게 채운다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|HelpPanel")
	TArray<FText> HelpLines;
};
