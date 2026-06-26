// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "O_AudioSettings.generated.h"

class USlider;

// 마스터 볼륨이 바뀔 때(슬라이더 드래그) 외부 리스너에게 알리는 델리게이트.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSettingMasterVolumeChanged, float, Value);

/**
 * UO_AudioSettings - 오디오 설정 탭 (명세 ⑩ O_Settings 하위 탭).
 *
 * UCommonAnimatedSwitcher 슬롯에 들어가는 컨텐츠 탭이므로 UCommonUserWidget 을 상속한다.
 * 마스터 볼륨(MasterVolumeSlider) 위젯 바인딩만 담당하며, 실제 저장은 메인 UI(UO_Settings)의
 * [적용] 버튼이 ApplyAudioSettings() 를 호출해 수행한다.
 */
UCLASS()
class TEAMCARRY_API UO_AudioSettings : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 슬라이더 드래그 시 실시간 볼륨 미리듣기를 위해 외부에서 구독할 수 있는 델리게이트.
	UPROPERTY(BlueprintAssignable, Category = "UI|Settings|Audio")
	FSettingMasterVolumeChanged OnMasterVolumeChanged;

	// 메인 UI(UO_Settings)의 [적용] 버튼이 호출하는 진입점.
	// 현재 슬라이더 값을 전역 설정에 반영(영속화)한다.
	UFUNCTION(BlueprintCallable, Category = "UI|Settings|Audio")
	void ApplyAudioSettings();

	// 현재 슬라이더가 가리키는 마스터 볼륨(0.0 ~ 1.0).
	UFUNCTION(BlueprintPure, Category = "UI|Settings|Audio")
	float GetMasterVolume() const { return CurrentMasterVolume; }

protected:
	virtual void NativeConstruct() override;

	// 마스터 볼륨 슬라이더(0.0 ~ 1.0).
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<USlider> MasterVolumeSlider;

private:
	UFUNCTION()
	void HandleMasterVolumeChanged(float Value);

	// 마지막으로 적용/선택된 마스터 볼륨 값.
	float CurrentMasterVolume = 1.0f;
};
