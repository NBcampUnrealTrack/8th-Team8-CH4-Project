// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/O_AudioSettings.h"

#include "Components/Slider.h"

void UO_AudioSettings::NativeConstruct()
{
	Super::NativeConstruct();

	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UO_AudioSettings::HandleMasterVolumeChanged);

		// 슬라이더 초기값을 현재 보존된 볼륨으로 동기화한다.
		MasterVolumeSlider->SetValue(CurrentMasterVolume);
	}
}

void UO_AudioSettings::HandleMasterVolumeChanged(float Value)
{
	// 드래그 중에는 값만 갱신하고 외부 리스너에게 미리듣기용으로 브로드캐스트한다.
	// (영속화는 [적용] 시점의 ApplyAudioSettings() 에서만 수행)
	CurrentMasterVolume = Value;
	OnMasterVolumeChanged.Broadcast(Value);
}

void UO_AudioSettings::ApplyAudioSettings()
{
	if (MasterVolumeSlider)
	{
		CurrentMasterVolume = MasterVolumeSlider->GetValue();
	}

	// 프로토타입 단계: 마스터 볼륨은 클라이언트별 전역 설정(명세 4. 전역 설정 - MasterVolume)으로 보존된다.
	// 실제 믹서 반영은 SoundMix/SoundClass 연동 단계에서 이 위치에 구현한다.
	//   UGameplayStatics::SetSoundMixClassOverride(this, MasterMix, MasterClass, CurrentMasterVolume, 1.0f, 0.0f);
	//   UGameplayStatics::PushSoundMixModifier(this, MasterMix);
	UE_LOG(LogTemp, Log, TEXT("[UI Settings|Audio] Master volume applied: %.2f"), CurrentMasterVolume);
}
