// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FurnitureGrabSystem.generated.h"


class UStaticMeshComponent;
class UFurnitureStat;
class ACharacter;

/**
 * 가구 운반(그랩) 시스템.
 *
 * 설계 핵심
 *  - 서버 권위: Grab/Release/이동계산은 모두 서버에서만 수행한다.
 *  - 강체 추종(Rigid Follow): 가구의 목표 트랜스폼은 "잡고 있는 플레이어들의 현재 위치"로부터
 *    매 틱 절대좌표로 새로 계산한다. (누적 AddOffset 금지 - 보간 깨짐 방지)
 *      · 1인 캐리: 플레이어 트랜스폼에 잡은 순간의 상대 트랜스폼을 곱해 그대로 따라간다.
 *      · 2인 캐리: 두 사람의 위치에 2D 강체정합(Kabsch)을 적용해 위치+Yaw를 동시에 푼다.
 *  - 가구는 항상 SetActorLocationAndRotation(sweep=true) 절대좌표 세팅으로 이동한다.
 *  - 클라이언트는 복제된 ServerLocation/ServerRotation 단 하나의 소스만 보간한다.
 *    (잡는 동안 SetReplicateMovement(false) 로 엔진 이동복제를 꺼서 writer 를 1개로 단일화)
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CATCHCHARACTER_API UFurnitureGrabSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	UFurnitureGrabSystem();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버에서 실행될 상호작용 함수 (GrabActorComponent 의 ServerRPC 를 통해 호출됨)
	// height: 첫 번째로 잡을 때 가구를 살짝 들어올리는 오프셋
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Grab(ACharacter* Grabber, FVector height, UPrimitiveComponent* GrabberComponent = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Release(ACharacter* Grabber);

	// 초기화 시 호출 (가구 본체에서 넘겨줌)
	void Setup(UStaticMeshComponent* InMesh, UFurnitureStat* InStat);

	// === 외부 상호작용(인터페이스 어댑터)용 조회 헬퍼 ===
	// 이 플레이어가 지금 이 가구를 잡고 있는가 (놓기/잡기 토글 판정용)
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsGrabbedBy(ACharacter* Player) const { return Player != nullptr && GrabbedPlayers.Contains(Player); }

	// 정원이 아직 차지 않아 더 잡을 수 있는가
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool CanAcceptGrab() const;

protected:
	virtual void BeginPlay() override;

	// --- 참조 캐싱 ---
	UPROPERTY()
	UStaticMeshComponent* FurnitureMesh;

	UPROPERTY()
	UFurnitureStat* FurnitureStat;

	// --- 상태 변수 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_GrabbedPlayers, Category = "Furniture|State")
	TArray<ACharacter*> GrabbedPlayers;

	// 클라에서도 충돌/물리/속도 셋업을 반영하기 위한 콜백
	UFUNCTION()
	void OnRep_GrabbedPlayers();

	// 클라 보간의 단일 진실원본(서버가 계산한 가구 최종 트랜스폼)
	UPROPERTY(Replicated)
	FVector ServerLocation;

	UPROPERTY(Replicated)
	FRotator ServerRotation;

public:
	// 클라 보간 속도 (VInterpTo/RInterpTo)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float ClientInterpSpeed = 18.0f;

	// 안전장치: 이상위치에서 이만큼 벌어지면 자동으로 놓는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float MaxGrabSeparationDistance = 300.0f;

	// 플레이어 위치 보정량 상한 (cm/s). 평소엔 거의 발동 안 함(안전장치).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float MaxCorrectionSpeed = 2000.0f;

	// 이보다 작은 위치 보정은 잡음으로 보고 무시(떨림 방지)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float CorrectionDeadzone = 0.5f; // cm

	// 이보다 작은 시선(Yaw) 보정은 잡음으로 보고 무시(떨림 방지)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float YawCorrectionDeadzone = 0.25f; // deg

	// true 면, 끌려가는 사람이 벽/장애물에 막혔을 때 그만큼 가구를 후퇴시켜 정지시킨다(조건3).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	bool bBlockedCarrierStopsFurniture = true;

	// 이보다 크게 막혔을 때만 가구를 후퇴시킨다(미세 충돌 무시).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float BlockStopThreshold = 1.0f; // cm

private:
	// 잡은 순간 기록하는 추종 기준값 (서버 전용)
	struct FGrabAnchor
	{
		// 잡은 순간의 (가구위치 - 플레이어위치) 월드 벡터. 가구가 플레이어에 대해 가질 상대 위치.
		FVector InitialOffset = FVector::ZeroVector;
		// 잡은 순간의 가구 Yaw / 플레이어 Yaw. 회전량 누적 없이 절대 기준으로 계산하기 위함.
		float   InitialFurnitureYaw = 0.0f;
		float   InitialPlayerYaw = 0.0f;
	};
	TMap<ACharacter*, FGrabAnchor> Anchors;

	// 서버: 잡기 전 원래 이동속도 저장
	TMap<ACharacter*, float> OriginalMaxWalkSpeeds;

	// --- 서버 이동 로직 ---
	void HandleMovement(float DeltaTime);

	// 서버가 계산한 플레이어 보정(절대 목표 위치 + 시선 Yaw)을 소유 클라이언트에 적용
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ApplyPlayerCorrection(ACharacter* Player, FVector TargetLocation, float TargetYaw);

	// --- 클라 보간 ---
	void UpdateClientInterpolation(float DeltaTime);
	FVector PreviousClientLoc = FVector::ZeroVector;
	FRotator PreviousClientRot = FRotator::ZeroRotator;
	bool bHasClientInterpInit = false;

	// 클라에서 충돌무시/틱순서/속도 셋업을 추적/정리하기 위한 캐시
	UPROPERTY()
	TArray<ACharacter*> ClientTrackedPlayers;
	float LocalOriginalMaxWalkSpeed = 0.0f;
	bool bLocalSpeedReduced = false;
	void UpdateLocalWalkSpeed();

	// 한 플레이어에 대한 충돌무시/틱순서 셋업(true) 또는 해제(false). 서버/클라 공통.
	void SetGrabCollisionState(ACharacter* Player, bool bEnable);
};
