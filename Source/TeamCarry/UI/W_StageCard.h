// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "W_StageCard.generated.h"

class UTextBlock;

/**
 * UW_StageCard - O_StageSelect 의 스테이지 목록 카드 아이템(명세 4장-5, 6장-1 하위 위젯 규칙).
 *
 * UListView 의 엔트리 위젯. IUserObjectListEntry 를 통해 UStageListItemData 를 전달받아
 * 표시만 담당한다(선택/클릭 처리는 UListView::OnItemClicked() 를 구독하는 O_StageSelect 가 수행).
 */
UCLASS()
class TEAMCARRY_API UW_StageCard : public UCommonUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	// UListView 가 이 엔트리에 데이터 아이템을 배정할 때 호출.
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_DisplayName;
};
