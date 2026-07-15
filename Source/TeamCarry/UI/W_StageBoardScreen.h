// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "W_StageBoardScreen.generated.h"

class UTextBlock;
class UListView;
class UCommonButtonBase;
class ATCLobbyGameState;

/**
 * UW_StageBoardScreen - BP_StageSelectBoard(게시판 액터)에 부착되는 월드 스페이스 위젯
 * (명세 3장·4장-5·6장-9, v3 내부 개정 — 게시판 UI 디자인 확정).
 *
 * 구 O_StageSelect(화면 오버레이)의 목록/확인/취소 기능을 그대로 흡수했다 — 스테이지 선택 UI 전체가
 * 화면 오버레이가 아니라 이 게시판의 월드 스크린에 실시간으로 렌더링된다(컴퓨터 디스플레이처럼).
 * CommonUI 화면 스택(State/Overlay)에 속하지 않는 예외 위젯이므로 UMockUIController의
 * ReplaceState/PushOverlay 경로를 타지 않고, ATCLobbyGameState::OnSelectedStageChanged 를 직접
 * 구독한다. 방장이 아닌 플레이어에게도 항상 보이며(현재 선택된 스테이지가 실시간으로 하이라이트됨),
 * 실제로 클릭(확인/취소/목록 항목)이 가능한 것은 ATCPlayerController::ClientEnterBoardInteractionMode()
 * 로 진입한 방장의 WidgetInteractionComponent 뿐이다.
 */
UCLASS()
class TEAMCARRY_API UW_StageBoardScreen : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 현재 선택된 스테이지 이름(경량 텍스트 표시, FStageInfo 에 썸네일 필드가 아직 없으므로 텍스트만).
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_StageName;

	// 스테이지 목록(DT_Stages 기반 동적 생성, 카드 아이템=UW_StageCard). 구 O_StageSelect 에서 이관.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UListView> List_Stages;

	// 확인: 하이라이트된 스테이지로 SetStageSelection() 호출 후 게시판 클릭 모드를 종료한다.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Confirm;

	// 취소: 선택 변경 없이 게시판 클릭 모드를 종료한다.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Cancel;

private:
	// ATCLobbyGameState 구독을 시도한다. 성공 시 true(구독 + 최초 1회 갱신까지 수행).
	bool TryBindLobbyState();

	// ATCLobbyGameState::OnSelectedStageChanged 구독 핸들러.
	UFUNCTION()
	void HandleSelectedStageChanged(int32 NewStageId);

	// RawStageId(GameState 복제값, 미선택 시 0)를 실제 표시/하이라이트에 반영한다.
	void RefreshDisplay(int32 RawStageId);

	// UTCSessionFlow::GetAllStageInfos() 를 읽어 List_Stages 를 채운다(구 O_StageSelect 에서 이관).
	void PopulateStageList();

	// 목록 항목 클릭(호스트의 WidgetInteractionComponent 로컬 클릭). 확인 전까지는 하이라이트만 갱신.
	void HandleStageItemClicked(UObject* Item);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	// 확인(Btn_Confirm) 시 SetStageSelection() 에 전달할 하이라이트된 StageId(0 = 미하이라이트).
	int32 PendingSelectedStageId = 0;

	// 구독 중인 로비 GameState(해제용).
	TWeakObjectPtr<ATCLobbyGameState> BoundLobbyState;
};
