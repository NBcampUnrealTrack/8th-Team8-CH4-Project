// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/TC_DataTypes.h"
#include "TCFeedbackSubsystem.generated.h"

class USoundBase;
class UNiagaraSystem;
class UAudioComponent;

/**
 * 게임 피드백 서브시스템 — 기존 클래스/BP를 수정하지 않고 사운드·이펙트를 얹는다.
 *
 * 1) 운반 가구(ATCCarriableFurniture)에 UTCFeedbackComponent를 '런타임 자동 부착'
 *    (레벨에 있던 것 + 이후 스폰되는 것 모두) → 잡기/놓기 피드백
 * 2) GameState 복제값을 관찰:
 *    - RemainingFurniture 감소 = 적재 성공 → 징글 + 트럭 위 팝 이펙트
 *    - CurrentPhase 전환 → 카운트다운/시작음
 *
 * 컨플릭트 방지 설계: 팀원 소유 파일(가구·GameState·BP)에 손대지 않는다.
 * 액터별 커스텀은 ITCFeedbackOverride 인터페이스 구현으로 개입한다.
 */
UCLASS()
class TEAMCARRY_API UTCFeedbackSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UTCFeedbackSubsystem, STATGROUP_Tickables);
	}

protected:
	UPROPERTY()
	TObjectPtr<USoundBase> TruckInSound;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> TruckInFX;

	UPROPERTY()
	TObjectPtr<USoundBase> CountdownSound;

	UPROPERTY()
	TObjectPtr<USoundBase> GoSound;

	// 인게임 BGM — Playing 진입 시 시작음 뒤에 페이드인, 게임 종료 시 페이드아웃
	UPROPERTY()
	TObjectPtr<USoundBase> GameBGM;

	UPROPERTY()
	TObjectPtr<UAudioComponent> BGMComp;

private:
	void AttachFeedbackIfFurniture(AActor* Actor);
	void StartBGM();
	void StopBGM();

	FTimerHandle BGMStartTimer;
	bool bBGMFadedOut = false;

	FDelegateHandle ActorSpawnedHandle;

	// 감소 판정용 이전값. -1 = 초기화 전(첫 관찰은 재생 안 함)
	int32 LastRemaining = -1;
	int32 LastScore = 0;
	EGamePhase LastPhase = EGamePhase::WaitingToStart;
	bool bPhaseInitialized = false;

	// 가구 감소 직후 적재 판별 대기 상태
	// (적재 = 점수도 함께 변함. 파괴 연출은 TCFeedbackComponent::EndPlay가 가구 위치에서 담당)
	bool bPendingClassify = false;
	float PendingTimeLeft = 0.f;
	int32 PendingBaseScore = 0;

	void PlayDeposit();
};
