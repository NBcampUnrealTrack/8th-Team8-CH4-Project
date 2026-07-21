// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/O_AudioSettings.h"

#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"

// 클라 로컬 볼륨 설정 — 위젯 인스턴스가 파괴/재생성돼도 세션 동안 유지된다.
float UO_AudioSettings::CurrentMasterVolume = 1.0f;
float UO_AudioSettings::CurrentSFXVolume = 1.0f;
float UO_AudioSettings::CurrentBGMVolume = 1.0f;

namespace
{
	// 기본 믹서 에셋 (WBP 에서 오버라이드 가능)
	const TCHAR* DefaultVolumeMix = TEXT("/Game/Developers/goldb/Audio/SM_Volume.SM_Volume");
	const TCHAR* DefaultMasterClass = TEXT("/Game/Developers/goldb/Audio/SC_Master.SC_Master");
	const TCHAR* DefaultSFXClass = TEXT("/Game/Developers/goldb/Audio/SC_SFX.SC_SFX");
	const TCHAR* DefaultBGMClass = TEXT("/Game/Developers/goldb/Audio/SC_BGM.SC_BGM");

	// 슬라이더 값(0.0~1.0)을 "75%" 형태의 정수 퍼센트 텍스트로 변환.
	FText FormatVolumePercent(float Value)
	{
		return FText::Format(NSLOCTEXT("O_AudioSettings", "VolumePercentFormat", "{0}%"),
			FText::AsNumber(FMath::RoundToInt(Value * 100.0f)));
	}
}

void UO_AudioSettings::NativeConstruct()
{
	Super::NativeConstruct();

	// 믹서 에셋 준비 (미지정 시 기본 세트)
	if (!VolumeMix) { VolumeMix = LoadObject<USoundMix>(nullptr, DefaultVolumeMix); }
	if (!MasterClass) { MasterClass = LoadObject<USoundClass>(nullptr, DefaultMasterClass); }
	if (!SFXClass) { SFXClass = LoadObject<USoundClass>(nullptr, DefaultSFXClass); }
	if (!BGMClass) { BGMClass = LoadObject<USoundClass>(nullptr, DefaultBGMClass); }

	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UO_AudioSettings::HandleMasterVolumeChanged);
		MasterVolumeSlider->SetValue(CurrentMasterVolume);
	}
	if (Slider)
	{
		Slider->OnValueChanged.AddUniqueDynamic(this, &UO_AudioSettings::HandleSFXVolumeChanged);
		Slider->SetValue(CurrentSFXVolume);
	}
	if (Slider_B)
	{
		Slider_B->OnValueChanged.AddUniqueDynamic(this, &UO_AudioSettings::HandleBGMVolumeChanged);
		Slider_B->SetValue(CurrentBGMVolume);
	}

	// SetValue()는 값이 실제로 바뀔 때만 OnValueChanged를 발생시키므로(위젯 최초 생성 시
	// 슬라이더 기본값이 이미 CurrentXVolume과 같으면 콜백이 안 불릴 수 있다), 퍼센트 텍스트는
	// 여기서 한 번 더 직접 채워 둔다.
	if (Txt_MasterVolumePercent) { Txt_MasterVolumePercent->SetText(FormatVolumePercent(CurrentMasterVolume)); }
	if (Txt_SFXVolumePercent) { Txt_SFXVolumePercent->SetText(FormatVolumePercent(CurrentSFXVolume)); }
	if (Txt_BGMVolumePercent) { Txt_BGMVolumePercent->SetText(FormatVolumePercent(CurrentBGMVolume)); }

	// 열릴 때 현재 설정값으로 믹스를 활성화해 둔다 (첫 사용 시 기본 1.0 = 변화 없음)
	ApplyMix();
}

void UO_AudioSettings::HandleMasterVolumeChanged(float Value)
{
	CurrentMasterVolume = Value;
	ApplyMix(); // 드래그 중 실시간 미리듣기
	if (Txt_MasterVolumePercent) { Txt_MasterVolumePercent->SetText(FormatVolumePercent(Value)); }
	OnMasterVolumeChanged.Broadcast(Value);
}

void UO_AudioSettings::HandleSFXVolumeChanged(float Value)
{
	CurrentSFXVolume = Value;
	ApplyMix();
	if (Txt_SFXVolumePercent) { Txt_SFXVolumePercent->SetText(FormatVolumePercent(Value)); }
}

void UO_AudioSettings::HandleBGMVolumeChanged(float Value)
{
	CurrentBGMVolume = Value;
	ApplyMix();
	if (Txt_BGMVolumePercent) { Txt_BGMVolumePercent->SetText(FormatVolumePercent(Value)); }
}

void UO_AudioSettings::ApplyMix()
{
	if (!VolumeMix)
	{
		return;
	}
	// 클래스별 볼륨 오버라이드 — SC_SFX/SC_BGM 은 SC_Master 의 자식이라
	// 마스터 값이 두 채널에 곱으로 함께 걸린다.
	if (MasterClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(this, VolumeMix, MasterClass, CurrentMasterVolume, 1.0f, 0.1f);
	}
	if (SFXClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(this, VolumeMix, SFXClass, CurrentSFXVolume, 1.0f, 0.1f);
	}
	if (BGMClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(this, VolumeMix, BGMClass, CurrentBGMVolume, 1.0f, 0.1f);
	}
	UGameplayStatics::PushSoundMixModifier(this, VolumeMix);
}

void UO_AudioSettings::ApplyAudioSettings()
{
	if (MasterVolumeSlider) { CurrentMasterVolume = MasterVolumeSlider->GetValue(); }
	if (Slider) { CurrentSFXVolume = Slider->GetValue(); }
	if (Slider_B) { CurrentBGMVolume = Slider_B->GetValue(); }

	ApplyMix();

	UE_LOG(LogTemp, Log, TEXT("[UI Settings|Audio] 볼륨 적용 — 전체 %.2f / 효과음 %.2f / 배경음악 %.2f"),
		CurrentMasterVolume, CurrentSFXVolume, CurrentBGMVolume);
}
