// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "O_Result.generated.h"

class UCommonButtonBase;
class UTextBlock;

/**
 * UO_Result - 최종 결과 오버레이(명세 4장-8, 구 S_Result 대체).
 *
 * 게임 종료 시 인게임 레벨 위에 뜨는 전체화면 오버레이. 정산 로직은 v1의 S_Result 와 동일하며,
 * UMockUIController::TriggerGameResult() 가 값을 캐시한 뒤 PushOverlay("O_Result") 로 띄운다.
 * 위젯 생성이 델리게이트 브로드캐스트보다 먼저 동기적으로 일어나므로, NativeConstruct 시점에
 * GetLastFinalScore()/GetLastStarCount()/GetLastElapsedTime() 로 캐시된 값을 바로 읽어 반영한다.
 *
 * 강제 모달: ESC 로 닫을 수 없다 — 반드시 Btn_ToLobby/Btn_ToTitle 중 하나로만 이탈한다.
 */
UCLASS()
class TEAMCARRY_API UO_Result : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// 활성화 중 게임 입력을 완전히 차단한다(명세 4장-8). 인게임 코어 루프 정지 자체는
	// 서버 권위의 bIsGameFinished 게이팅(GameMode/GrabComponent)이 담당하며, 이 위젯은 UI 입력만 막는다.
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// ESC 무시: 강제 모달이라 뒤로가기로 닫을 수 없다. Back 전파만 차단하고 아무 동작도 하지 않는다.
	virtual bool NativeOnHandleBackAction() override;

	// --- 정산 통계 텍스트 ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Score;

	// 세부 통계(미구현 — 정산 세부 항목 데이터 소스가 아직 없어 플레이스홀더로 남긴다).
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Stats;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_StarCount;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_ElapsedTime;

	// --- 네비게이션 ---
	// 로비로 가기: 세션 유지 복귀(호스트 전용 트래블 — 프로토타입은 호스트 전용 활성화 기본).
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_ToLobby;

	// 메인 화면으로: 세션 파기 후 복귀(호스트/클라 공통).
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_ToTitle;

private:
	UFUNCTION()
	void HandleToLobbyClicked();

	UFUNCTION()
	void HandleToTitleClicked();
};
