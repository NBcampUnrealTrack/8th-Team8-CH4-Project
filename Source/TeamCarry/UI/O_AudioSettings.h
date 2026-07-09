// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "O_AudioSettings.generated.h"

class USlider;
class USoundMix;
class USoundClass;

// 마스터 볼륨이 바뀔 때(슬라이더 드래그) 외부 리스너에게 알리는 델리게이트.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSettingMasterVolumeChanged, float, Value);

/**
 * UO_AudioSettings - 오디오 설정 탭 (명세 ⑩ O_Settings 하위 탭).
 *
 * UCommonAnimatedSwitcher 슬롯에 들어가는 컨텐츠 탭이므로 UCommonUserWidget 을 상속한다.
 * 슬라이더 3종(전체/효과음/배경음악)을 SoundMix(SM_Volume) + SoundClass(SC_Master/SC_SFX/SC_BGM)
 * 오버라이드로 실제 믹서에 반영한다. 드래그 중 실시간 미리듣기, [적용] 시 확정.
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

	// 전체 음량 슬라이더(0.0 ~ 1.0).
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<USlider> MasterVolumeSlider;

	// 효과음 슬라이더(0.0 ~ 1.0). ※ 프로퍼티명은 WBP_AudioSettings 의 기존 위젯 이름("Slider")과
	// 일치해야 BindWidget 이 잡힌다 — WBP 를 수정하지 않기 위한 의도적 네이밍.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<USlider> Slider;

	// 배경음악 슬라이더(0.0 ~ 1.0). ※ 위와 같은 이유로 WBP 위젯 이름("Slider_B") 유지.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<USlider> Slider_B;

	// 볼륨 믹서 에셋(에디터에서 교체 가능, 미지정 시 goldb/Audio 기본 세트 로드).
	UPROPERTY(EditDefaultsOnly, Category = "UI|Settings|Audio")
	TObjectPtr<USoundMix> VolumeMix;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Settings|Audio")
	TObjectPtr<USoundClass> MasterClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Settings|Audio")
	TObjectPtr<USoundClass> SFXClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Settings|Audio")
	TObjectPtr<USoundClass> BGMClass;

private:
	UFUNCTION()
	void HandleMasterVolumeChanged(float Value);

	UFUNCTION()
	void HandleSFXVolumeChanged(float Value);

	UFUNCTION()
	void HandleBGMVolumeChanged(float Value);

	// SoundMix 오버라이드 적용 (드래그 미리듣기·[적용] 공용)
	void ApplyMix();

	// 마지막으로 적용/선택된 볼륨 값 — 위젯 재생성 간 유지되도록 static (클라 로컬 설정).
	static float CurrentMasterVolume;
	static float CurrentSFXVolume;
	static float CurrentBGMVolume;
};
