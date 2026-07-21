// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_MovieLoadingScreen.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;

/**
 * UW_MovieLoadingScreen - GetMoviePlayer()의 FLoadingScreenAttributes::WidgetLoadingScreen에
 * 붙는 로딩 화면 위젯(UI_Technical_Spec.md 2장·4장-9, "로딩 화면 동기화 문제" 참고).
 *
 * hard travel(예: 방 생성 직후 ?listen ServerTravel, TCGameInstance.cpp:36-46/176-189)은 게임
 * 스레드를 LoadMap 동안 완전히 블로킹하므로, 일반 UMG(S_Loading, UCommonActivatableWidget)는
 * 이 구간에 그려질 수 없다. MoviePlayer는 별도 로딩 스레드 + 전용 Slate 루프를 갖고 있어
 * 게임 스레드가 블로킹된 동안에도 계속 그려진다 — 이 위젯은 그 루프에 올라가는 대상이다.
 *
 * UTCSessionFlow가 CreateWidget() 직후 TakeWidget()으로 SWidget만 뽑아 GetMoviePlayer()에
 * 넘기므로, 이 위젯은 CommonUI 화면 스택(UCommonActivatableWidget)이나 UMockUIController
 * 라우팅을 전혀 거치지 않는 독립된 UUserWidget이다. 뷰포트에 직접 AddToViewport 되지 않는다.
 */
UCLASS()
class TEAMCARRY_API UW_MovieLoadingScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	// UTCSessionFlow::HandlePostLoadMap()이 스테이지 맵(S_InGame) 도착 후 "전원 대기" 상태로
	// 전환할 때 호출한다. S_Loading에는 없던 요소 — 전원 대기 게이트 전용.
	UFUNCTION(BlueprintCallable, Category = "UI|LoadingScreen")
	void SetStatusText(const FText& NewText);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// S_Loading(UI_Technical_Spec.md 4장-9)과 동일한 이름으로 맞춰 두었다 — 같은 디자인을
	// 재사용하기 쉽도록 하기 위함일 뿐, 실제로는 서로 다른 독립된 위젯 클래스다.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UImage> Img_Background;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UProgressBar> PB_Loading;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_LoadingGauge;

	// 스테이지 맵 진입 "전원 대기" 상태 문구("다른 플레이어를 기다리는 중..." 등) 표시용.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Status;

	// S_Loading과 동일한 시간 기반 유사 진행(0→90% 보간) — UE의 LoadMap은 정밀 진행률 콜백을
	// 제공하지 않는다는 동일한 한계를 갖는다(UI_Technical_Spec.md 4장-9 구현 노트 참고).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading")
	float PseudoProgressDuration = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading")
	float PseudoProgressCap = 0.9f;

private:
	float ComputeTimeBasedProgress(float InElapsedSeconds) const;
	void ApplyLoadingProgress(float Alpha01);

	float ElapsedSeconds = 0.0f;
};
