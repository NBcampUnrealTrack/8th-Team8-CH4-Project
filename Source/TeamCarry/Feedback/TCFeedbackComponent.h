// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TCFeedbackComponent.generated.h"

class USoundBase;
class UNiagaraSystem;
class UFurnitureGrabSystem;

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	// 가구 파괴 시 그 자리에서 재생 (EndPlay(Destroyed)에서 감지 — 트럭 근처면 적재로 보고 억제)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
	TObjectPtr<USoundBase> BreakSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
	TObjectPtr<UNiagaraSystem> BreakFX;

	// 이 거리 안에서 파괴(=액터 제거)되면 트럭 적재로 간주해 파괴 연출을 내지 않는다
	UPROPERTY(EditAnywhere, Category = "Feedback")
	float TruckSuppressRadius = 800.f;

private:
	// 잡힘 상태 소스 1: 플러그인 GrabSystem 컴포넌트 (TCFurnitureActor 계열)
	UPROPERTY()
	TObjectPtr<UFurnitureGrabSystem> GrabSystem;

	// 잡힘 상태 소스 2: 소유자 클래스의 bIsGrabbed(bool) — 리플렉션 캐시
	const FBoolProperty* GrabbedProp = nullptr;

	bool bLastGrabbed = false;

	bool ReadGrabbed() const;
	FVector GetFXLocation() const;
};
