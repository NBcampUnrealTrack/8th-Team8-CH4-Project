// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CommonInputModeTypes.h"
#include "S_InGame.generated.h"

class UTextBlock;
class UProgressBar;
class UTexture2D;
class UImage;
class UButton;

/**
 * US_InGame - In-Game HUD Screen implementing CommonActivatableWidget
 */
UCLASS()
class TEAMCARRY_API US_InGame : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// CommonUI가 이 위젯을 화면에 띄울 때 요구할 입력 설정을 C++ 단에서 오버라이드합니다.
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// --- HUD Bound Widgets ---
	// 팀 값어치 게이지(진행도). 기존 텍스트 전용 점수판을 대체한다(UI_Technical_Spec.md 4장-7).
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UProgressBar> PB_TeamMoney;

	// PB_TeamMoney 내부에 겹쳐 표시되는 "현재 값어치 / 전체 목표 값어치" 텍스트.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> TextBlock_Score;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> TextBlock_Timer;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> TextBlock_InteractPrompt;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UImage> Image_Map;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Map")
	TObjectPtr<UTexture2D> MapTexture;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Menu;

	// 가구 개수 표시: "이동 가능한 개수 / 전체 상자 개수(파괴된 것 포함)" 분수 표기(v3 내부 개정,
	// 구 Txt_RemainingFurniture 대체 — UI_Technical_Spec.md 4장-7).
	// WBP에 아직 위젯이 추가되지 않은 상태에서도 크래시가 나지 않도록 Optional로 선언한다.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_FurnitureCount;

	// 부서진 정도 경고용(기능 미구현) — WBP에 위젯은 존재하므로 바인딩해서
	// 기본값 "Text Block" 노출만 막는다. 경고 기능 구현 시 이 위젯을 사용할 것.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> TextBlock_WarnPlayers;

private:
	// --- Delegate Listeners ---
	UFUNCTION()
	void HandleTeamMoneyUpdated(int32 NewTotalMoney);

	//UFUNCTION()
	//void HandleFurnitureSettled(int32 AddedMoney, int32 Grade);

	UFUNCTION()
	void HandleInteractTargetChanged(AActor* Target, FString Key);

	//UFUNCTION()
	//void HandleDurabilityChanged(float Current, float Max);

	UFUNCTION()
	void HandleRemainingFurnitureUpdated(int32 NewCount);

	UFUNCTION()
	void HandleMenuClicked();

	// GameState의 ElapsedTime(복제됨)을 읽어 TextBlock_Timer 표시를 갱신한다.
	// ElapsedTime은 매 프레임 변하는 값이라 델리게이트/RepNotify 대신 Tick에서 직접 폴링한다.
	void UpdateTimerDisplay(float ElapsedTime);

	// 불필요한 문자열 재생성을 막기 위해 마지막으로 표시한 '초' 단위 값을 캐시한다.
	int32 LastDisplayedSeconds = -1;

	// 팀 값어치 게이지의 Max 값(전체 목표 값어치). 스테이지 중 불변이므로 NativeConstruct에서 1회만 조회한다.
	int32 CachedTotalLevelValue = 0;

	// Txt_FurnitureCount 분모(전체 상자 개수, 파괴된 것 포함). 스테이지 중 불변이므로 NativeConstruct에서 1회만 조회한다.
	int32 CachedTotalFurnitureCount = 0;
};
