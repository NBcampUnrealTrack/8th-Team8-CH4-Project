// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "W_StageListItem.generated.h"

class UTextBlock;
class UImage;

/**
 * UW_StageListItem - W_StageBoardScreen(구 O_StageSelect)의 List_Stages(UListView) 엔트리 위젯
 * (명세 4장-5, 게시판 UI 개정).
 *
 * IUserObjectListEntry 를 네이티브로 구현해, UListView 가 각 행(row)을 생성할 때 전달하는
 * UStageListItemData 를 받아 자동으로 텍스트/이미지를 채운다 — 블루프린트 이벤트 그래프 배선이
 * 필요 없다.
 */
UCLASS()
class TEAMCARRY_API UW_StageListItem : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	// IUserObjectListEntry
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> TextBlock_StageName;

	// 확장 여지(미구현): FStageInfo 에 썸네일 필드가 아직 없어 현재는 사용하지 않는다(7장-3).
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UImage> Image_Stage;
};
