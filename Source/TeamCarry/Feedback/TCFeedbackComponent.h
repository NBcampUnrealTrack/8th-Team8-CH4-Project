// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TCFeedbackComponent.generated.h"

class USoundBase;
class UNiagaraSystem;
class UFurnitureGrabSystem;
class UFurnitureStat;
class AActor;

/**
 * 가구 잡기/놓기 피드백 컴포넌트.
 * 소유 액터의 잡힘 상태를 관찰해(소유 클래스 무수정) 전이 시 사운드·이펙트를 재생한다.
 *  - TCFurnitureActor 계열: FurnitureGrabSystem.GrabbedPlayers 관찰 (타입 접근)
 *  - TCCarriableFurniture 계열: bIsGrabbed 복제 프로퍼티 관찰 (리플렉션)
 * 호스트·클라 공통, 데디서버 스킵.
 *
 * 부착 방법: TCFeedbackSubsystem이 운반 가구에 런타임 자동 부착한다.
 * BP에서 직접 추가해도 되며, 에셋은 프로퍼티로 교체 가능하다.
 * 커스텀이 필요하면 소유 액터가 ITCFeedbackOverride를 구현한다.
 */
UCLASS(ClassGroup = (TeamCarry), meta = (BlueprintSpawnableComponent))
class TEAMCARRY_API UTCFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTCFeedbackComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
	TObjectPtr<USoundBase> PickupSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
	TObjectPtr<USoundBase> DropSound;

	// 들어올릴 때 바닥 먼지 (액터 바닥 기준으로 스폰)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
	TObjectPtr<UNiagaraSystem> PickupFX;

	// 가구 파괴 시 그 자리에서 재생 — 내구도(FurnitureStat.CurrentHealth)가
	// 0이 되는 프레임을 관찰해 감지한다 (액터 Destroy 시점은 적재·정리와 겹쳐 부정확)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
	TObjectPtr<USoundBase> BreakSound;

	// 내구도 감소(타격) 시 랜덤 재생 목록. 비워두면 기본 우드히트 2종 자동 로드.
	// (가구별 타격음 커스텀: BP에 이 컴포넌트를 수동 추가하고 여기에 지정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
	TArray<TObjectPtr<USoundBase>> HitSounds;

	// 타격음 아래에 겹치는 저역 '쿵' 레이어. 비워두면 기본 자동 로드, 없애려면 무음 에셋 지정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
	TObjectPtr<USoundBase> ThudSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
	TObjectPtr<UNiagaraSystem> BreakFX;

private:
	// 잡힘 상태 소스 1: 플러그인 GrabSystem 컴포넌트 (TCFurnitureActor 계열)
	UPROPERTY()
	TObjectPtr<UFurnitureGrabSystem> GrabSystem;

	// 잡힘 상태 소스 2: 소유자 클래스의 bIsGrabbed(bool) — 리플렉션 캐시
	const FBoolProperty* GrabbedProp = nullptr;

	// 내구도 관찰용 (복제값 — 호스트·클라 공통)
	UPROPERTY()
	TObjectPtr<UFurnitureStat> Stat;

	float LastHealth = -1.f;

	bool bLastGrabbed = false;

	// 적재존(BP_TruckTrigger) 캐시 — 핫타임 빨간 링에서 존 안 가구를 제외할 때 사용
	TWeakObjectPtr<AActor> CachedTruckZone;
	bool bTruckZoneSearched = false;

	bool ReadGrabbed() const;
	FVector GetFXLocation() const;
	bool IsOwnerInTruckZone();
};
