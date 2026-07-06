// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "O_StageSelect.generated.h"

class UListView;
class UCommonButtonBase;
class UObject;

/**
 * UO_StageSelect - 방장 전용 스테이지 선택 오버레이(명세 4장-5, 7장-3, 구 S_StageSelect 대체).
 *
 * S_Lobby 의 Btn_StageSelect(방장 전용) 클릭 시 PushOverlay 된다. 이 오버레이 자체는 트래블을
 * 수행하지 않는다 — 진입은 오직 S_Lobby 의 [게임 시작](HostStartGame())이 수행한다.
 *
 * 목록은 UTCSessionFlow::GetAllStageInfos()(DT_Stages 기반)를 읽어 UListView 에 동적으로 채우므로,
 * 행이 늘어나도 이 위젯은 수정할 필요가 없다.
 */
UCLASS()
class TEAMCARRY_API UO_StageSelect : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 활성화 중 게임 입력을 차단하고 UI 전용 입력으로 전환한다(명세 6장-2).
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// ESC = 취소와 동일 처리(명세 4장-5): 선택 변경 없이 PopCurrentOverlay().
	virtual bool NativeOnHandleBackAction() override;

	// --- 스테이지 목록(DT_Stages 기반 동적 생성, 카드 아이템=UW_StageCard) ---
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UListView> List_Stages;

	// 확인: 하이라이트된 스테이지로 SetStageSelection() 호출 후 닫기.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Confirm;

	// 취소: 선택 변경 없이 닫기.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Cancel;

private:
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	// 리스트 항목 클릭 시 호출(네이티브 이벤트). 확인 전까지는 하이라이트만 갱신한다.
	void HandleStageItemClicked(UObject* Item);

	// UTCSessionFlow::GetAllStageInfos() 를 읽어 List_Stages 를 채운다.
	void PopulateStageList();

	// 확인(Btn_Confirm) 시 SetStageSelection() 에 전달할 하이라이트된 StageId(0 = 미하이라이트).
	int32 PendingSelectedStageId = 0;
};
