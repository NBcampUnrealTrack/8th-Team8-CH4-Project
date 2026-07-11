// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FurnitureGrabSystem.generated.h"


class UStaticMeshComponent;
class UFurnitureStat;
class ACharacter;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CATCHCHARACTER_API UFurnitureGrabSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	UFurnitureGrabSystem();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Grab(ACharacter* Grabber, FVector height, UPrimitiveComponent* GrabberComponent = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Release(ACharacter* Grabber);
	void AllRelease();

	// 가구만 이동/회전 (플레이어는 회전·이동하지 않음)
	// YawOffset = Z축 회전, PitchOffset = Y축 회전(기울이기, 좁은 곳 통과용)
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void AddFurnitureOffset(FVector LocationOffset, float YawOffset, float PitchOffset = 0.0f);

	void Setup(UStaticMeshComponent* InMesh, UFurnitureStat* InStat);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsGrabbedBy(ACharacter* Player) const { return Player != nullptr && GrabbedPlayers.Contains(Player); }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool CanAcceptGrab() const;

	// 외부에서 잡고 있는 플레이어 배열을 가져갈 수 있도록 Getter 추가
	const TArray<ACharacter*>& GetGrabbedPlayers() const { return GrabbedPlayers; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	UStaticMeshComponent* FurnitureMesh;

	UPROPERTY()
	UFurnitureStat* FurnitureStat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_GrabbedPlayers, Category = "Furniture|State")
	TArray<ACharacter*> GrabbedPlayers;

	UFUNCTION()
	void OnRep_GrabbedPlayers();

	UPROPERTY(Replicated)
	FVector ServerLocation;

	UPROPERTY(Replicated)
	FRotator ServerRotation;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float ClientInterpSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float MaxGrabSeparationDistance = 300.0f;

	// 피동 플레이어를 끌어당기는 최대 속도 (cm/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float MaxCorrectionSpeed = 5000.0f;

	// 이보다 작은 위치 오차는 무시 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float CorrectionDeadzone = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float YawCorrectionDeadzone = 0.25f;

	// [서버] 원격 운반자 몸통 Yaw에 대한 서버 개입 허용 오차(도).
	// 원격 몸통 Yaw는 그 클라가 로컬에서(복제된 가구 Yaw 기준 = 한두 틱 낡음) 돌려서 ServerMove로 올라오는데,
	// 서버 Step 5가 최신 가구 Yaw로 매 틱 덮어쓰면 '낡은 값 ↔ 최신 값'이 서버 사본에서 매 틱 왕복
	// → 회전 중에만 뚝뚝 끊김(직진은 두 값이 같아 무증상). 정상 신선도 차(회전속도×지연 ≈ 2~5°)는
	// 클라 값을 존중하고, 이 값 이상 어긋날 때만 서버가 교정. 호스트는 지연이 없어 기존 정밀 데드존 사용.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float RemoteBodyYawTolerance = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	bool bBlockedCarrierStopsFurniture = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float BlockStopThreshold = 1.0f;

	// 가구 최대 회전 속도 (도/초). 빠른 카메라 회전 시 가구 위치 튐 방지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float FurnYawRotationSpeed = 90.0f;

	// 필요 인원 미달 시 이동속도 배율 (기본속도 대비). 0.1 = 1/10로 대폭 감속.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float UnderMannedSpeedFactor = 0.1f;

	// [회전 교착] 제안 방향 일치도(0~1)가 이 값 미만이면 줄다리기로 보고 회전 정지.
	// 등가중치 2인 기준 일치도 = cos(의견차/2) → 0.3 ≈ 의견차 145° 이상일 때 교착.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float YawStalemateEnterRatio = 0.3f;

	// [회전 교착] 일치도가 이 값을 넘어야 교착 해제 (진입값보다 크게 → 경계 팔락임 방지)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float YawStalemateExitRatio = 0.4f;

	// 카메라 상하(Pitch) → 가구 높이. 그랩 시점 대비 카메라가 1도 위/아래 볼 때마다 이 cm만큼 가구 높이 변경.
	// 방향이 반대면(위 보는데 내려감) 부호를 뒤집을 것. 0이면 기능 끔.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float FurnitureHeightPerPitch = 3.f;

	// 가구 높이 조절 범위 (그랩 시점 기준 cm). 최소=아래로 얼마까지, 최대=위로 얼마까지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float FurnitureHeightMin = -10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float FurnitureHeightMax = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Grab")
	float FurnitureHeightInterpSpeed = 10.0f;

private:
	struct FGrabAnchor
	{
		FVector InitialOffset        = FVector::ZeroVector;
		float   InitialFurnitureYaw  = 0.0f;
		float   InitialPlayerYaw     = 0.0f;  // 그랩 시점 캐릭터 몸통 Yaw (GetDesiredYaw 기준, 스냅 방지)
		float   InitialAimYaw        = 0.0f;  // 그랩 시점 카메라 Yaw (가구 회전 기준)
		float   PrevAimYaw           = 0.0f;  // 직전 틱 카메라 Yaw (견인 중 자기 회전 입력 감지용)
		float   InitialAimPitch      = 0.0f;  // 그랩 시점 카메라 Pitch (가구 높이 조절 기준)
	};
	TMap<ACharacter*, FGrabAnchor> Anchors;

	// 서버: 그랩 전 MaxWalkSpeed 원본값 (Release 시 복원)
	TMap<ACharacter*, float> OriginalMaxWalkSpeeds;

	// 이전 틱에 피동(끌어당김) 상태였던 플레이어 집합
	TSet<ACharacter*> DraggedLastTick;

	// [서버] 회전 교착(줄다리기) 상태. 히스테리시스로 관리 (Enter/ExitRatio 참고)
	bool bYawStalemate = false;

	// [서버] 지난 틱에 운반자가 벽에 막혔는가 (Step 4 감지 → 다음 틱 Step 2에서 회전 보류)
	bool bCarrierBlockedLastTick = false;

	// 이전 틱에 "피동→도달" 전환(bAtTarget && bWasDragged)이었던 플레이어 집합.
	// 이 틱의 Step 1에서 가중치=0으로 처리해 역방향 견인력을 방지하되,
	// DraggedLastTick에는 포함하지 않아 bWasDragged=false 유지 → Active 복귀 가능.
	TSet<ACharacter*> StoppedDraggingLastTick;

	void HandleMovement(float DeltaTime);
	FVector GetAttachedLocation(ACharacter* Player, const FVector& FurnitureLoc, float FurnitureYaw) const;
	float   GetDesiredYaw(ACharacter* Player, float FurnitureYaw) const;

	// CarryVelocity: 피동=끌어당기는 속도, 정지=FVector::ZeroVector
	// 능동(직접 걷는 중)일 때는 Multicast를 보내지 않는다.
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ApplyPlayerCorrection(ACharacter* Player, FVector CarryVelocity, float TargetYaw);

	// Grab 시점 앵커(초기 가구 Yaw·플레이어 카메라 Yaw·오프셋)를 클라이언트에 정확히 전달.
	// OnRep_GrabbedPlayers는 타이밍이 달라 초기값이 틀릴 수 있으므로 Reliable로 보정.
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetPlayerAnchor(ACharacter* Player, float InitFurnYaw, float InitPlayerYaw, float InitAimYaw, FVector InitOffset);

	// 패킷 순서 보장용 시퀀스 ID
	uint8 SystemOffsetSequence = 0;
	uint8 LocalSystemOffsetSequence = 0;
	// 가구 위치·회전을 매 서버 틱 클라이언트에 직접 전달 (DOREPLIFETIME 보완).
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_UpdateFurnitureTransform(FVector NewLocation, FRotator NewRotation, uint8 SeqID);

	// 가구 단독이동 시 가구와 플레이어 동시에 처리를 위해 추가.
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ApplySystemOffset(FVector ActualLocDelta, float ActualYawDelta, FVector NewServerLoc, FRotator NewServerRot, uint8 SeqID);

	// 디버그: 가구 실속도 + 플레이어 속도 전체 표시 (서버→모든 클라)
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ShowDebugSpeeds(float FurnActualSpeed, float FurnMaxSpeed,
		const TArray<float>& MaxWalkSpeeds, const TArray<float>& ActualSpeeds);

	// --- 클라 보간 (가구) ---
	void UpdateClientInterpolation(float DeltaTime);
	FVector  PreviousClientLoc   = FVector::ZeroVector;
	FRotator PreviousClientRot   = FRotator::ZeroRotator;
	bool     bHasClientInterpInit = false;

	// 클라: 잡힌 플레이어 추적
	UPROPERTY()
	TArray<ACharacter*> ClientTrackedPlayers;

	// 클라: 로컬 플레이어 MaxWalkSpeed 원본값 (OnRep Release 시 복원)
	float LocalOriginalMaxWalkSpeed = 0.0f;
	bool  bLocalCMCModified         = false;
	bool  bLocalSpeedReduced        = false;

	// RPC과정에서 타이밍이 어긋나 회전이 제대로 안되는걸 방지를 위한 로컬 회전값
	float LocalSyncTargetYaw = 0.0f;

	// 현재 보간 적용 중인 가구 높이 오프셋
	float CurrentHeightOffset = 0.0f;

	void UpdateLocalWalkSpeed();
	void SetGrabCollisionState(ACharacter* Player, bool bEnable);

	// 운반 이동속도 계산: 필요 인원 미달이면 UnderMannedSpeedFactor배로 대폭 감속, 충족이면 기본속도
	float ComputeCarrySpeed() const;
};
