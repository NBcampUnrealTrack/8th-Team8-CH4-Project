// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "S_Loading.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;

/**
 * US_Loading - 레벨 트래블 구간 로딩 화면(명세 4장-9, EE_UIState::Loading).
 *
 * UTCSessionFlow::OnTravelStarted 를 구독한 UMockUIController 가 ReplaceState(Loading) 으로
 * 띄운다. 트래블이 끝나면 도착 맵의 GameMode/PlayerController 가 목적지 State 로 다시
 * ReplaceState 하므로, 이 화면은 별도 종료 처리 없이 자연히 교체되어 사라진다.
 *
 * UE 의 맵 로드는 정밀 진행률 콜백을 제공하지 않으므로, 시간 기반 유사 진행(0→90% 보간, 이후
 * 유지)을 사용한다. "진행률 계산"(ComputeTimeBasedProgress)과 "표시 반영"(ApplyLoadingProgress)을
 * 분리해 두어, 실제 스트리밍/AsyncLoad 진행률이 생기면 계산 함수 호출부만 교체하면 된다.
 */
UCLASS()
class TEAMCARRY_API US_Loading : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 로딩 중 모든 입력을 무시한다(명세 4장-9). 이 화면엔 포커스 가능한 위젯이 없어 게임 입력만
	// 차단하면 사실상 전체 입력이 무시된다.
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// --- 구성 요소(정확히 3개, 명세 4장-9) ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UImage> Img_Background;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UProgressBar> PB_Loading;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_LoadingGauge;

	// 0%→PseudoProgressCap 에 도달하는 데 걸리는 가상의 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading")
	float PseudoProgressDuration = 4.0f;

	// 시간 기반 유사 진행의 상한(명세: 0→90% 보간). 실제 100%/완료 시점은 이 위젯이 알 필요가
	// 없다 — 트래블이 끝나면 화면 자체가 교체되어 사라지기 때문이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading")
	float PseudoProgressCap = 0.9f;

private:
	// 진행률 "계산" — 추후 실제 로딩 진행률(스트리밍/AsyncLoad)로 교체될 지점.
	float ComputeTimeBasedProgress(float InElapsedSeconds) const;

	// 진행률 "표시 반영" — 계산 방식이 바뀌어도 이 함수는 그대로 재사용된다.
	void ApplyLoadingProgress(float Alpha01);

	float ElapsedSeconds = 0.0f;
};
