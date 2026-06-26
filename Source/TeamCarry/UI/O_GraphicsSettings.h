// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "O_GraphicsSettings.generated.h"

class UComboBoxString;
class UCheckBox;

/**
 * UO_GraphicsSettings - 그래픽 설정 탭 (명세 ⑩ O_Settings 하위 탭).
 *
 * UCommonAnimatedSwitcher 슬롯에 들어가는 컨텐츠 탭이므로 UCommonUserWidget 을 상속한다.
 * (탭 자체는 활성화/입력 스택의 단위가 아니므로 UCommonActivatableWidget 이 아니다.)
 *
 * 그래픽 품질(GraphicsQualityComboBox)과 전체화면(FullscreenCheckBox) 위젯 바인딩만 담당하며,
 * 실제 저장은 메인 UI(UO_Settings)의 [적용] 버튼이 ApplyGraphicsSettings() 를 호출해 수행한다.
 */
UCLASS()
class TEAMCARRY_API UO_GraphicsSettings : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 메인 UI(UO_Settings)의 [적용] 버튼이 호출하는 진입점.
	// 현재 위젯 값을 읽어 UGameUserSettings 에 반영하고 디스크에 저장한다.
	UFUNCTION(BlueprintCallable, Category = "UI|Settings|Graphics")
	void ApplyGraphicsSettings();

protected:
	virtual void NativeConstruct() override;

	// 그래픽 품질 단계(0:Low ~ 4:Epic) 선택 콤보 박스.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UComboBoxString> GraphicsQualityComboBox;

	// 전체화면 여부 체크 박스.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCheckBox> FullscreenCheckBox;

private:
	// 현재 UGameUserSettings 값을 위젯에 반영하여 초기 상태를 동기화한다.
	void SyncWidgetsFromCurrentSettings();
};
