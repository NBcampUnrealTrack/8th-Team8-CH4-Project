// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TCFootstepComponent.generated.h"

class USoundBase;
class ACharacter;

/**
 * 달리기 발소리 컴포넌트 — 캐릭터 클래스를 수정하지 않고 소리를 얹는다.
 * TCFeedbackSubsystem이 플레이어 캐릭터에 런타임 자동 부착하며,
 * 지면 이동 속도에 비례한 케이던스로 발소리 베리에이션을 재생한다.
 * (애님 노티파이 방식이 정석이지만, 애님 에셋을 건드리지 않기 위한 절충)
 */
UCLASS(ClassGroup = (TeamCarry), meta = (BlueprintSpawnableComponent))
class TEAMCARRY_API UTCFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTCFootstepComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// 발소리 베리에이션 (랜덤 선택, BP에서 교체 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TArray<TObjectPtr<USoundBase>> StepSounds;

	// 이 속도(cm/s) 이상 지면 이동 시에만 발소리
	UPROPERTY(EditAnywhere, Category = "Footstep")
	float MinSpeed = 130.f;

	// 걸음 간격: 속도 MinSpeed→MaxSpeed에서 SlowInterval→FastInterval로 보간
	UPROPERTY(EditAnywhere, Category = "Footstep")
	float MaxSpeed = 600.f;

	UPROPERTY(EditAnywhere, Category = "Footstep")
	float SlowInterval = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Footstep")
	float FastInterval = 0.28f;

	UPROPERTY(EditAnywhere, Category = "Footstep")
	float Volume = 0.5f;

private:
	UPROPERTY()
	TObjectPtr<ACharacter> OwnerChar;

	float StepClock = 0.f;
	int32 LastIndex = -1;
};
