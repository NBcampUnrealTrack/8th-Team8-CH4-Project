// Fill out your copyright notice in the Description page of Project Settings.

#include "FurnitureGrabSystem.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "CatchCharacter/Furniture/FurnitureStat.h"
#include "CatchCharacter/Furniture/FurnitureDamage.h"

// =====================================================================
// 생성 / 초기화
// =====================================================================

UFurnitureGrabSystem::UFurnitureGrabSystem()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UFurnitureGrabSystem::BeginPlay()
{
	Super::BeginPlay();
}

void UFurnitureGrabSystem::Setup(UStaticMeshComponent* InMesh, UFurnitureStat* InStat)
{
	FurnitureMesh = InMesh;
	FurnitureStat = InStat;
}

bool UFurnitureGrabSystem::CanAcceptGrab() const
{
	if (!FurnitureStat)
		return false;
	return GrabbedPlayers.Num() < FurnitureStat->GetRequiredPlayer();
}

void UFurnitureGrabSystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UFurnitureGrabSystem, GrabbedPlayers);
	DOREPLIFETIME(UFurnitureGrabSystem, ServerLocation);
	DOREPLIFETIME(UFurnitureGrabSystem, ServerRotation);
}

// =====================================================================
// 틱
// =====================================================================

void UFurnitureGrabSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner)
		return;

	const bool bAuthority = Owner->HasAuthority();
	const bool bGrabbed   = GrabbedPlayers.Num() > 0;

	if (bAuthority && bGrabbed)
	{
		HandleMovement(DeltaTime);
	}
	else if (!bAuthority && bGrabbed)
	{
		UpdateClientInterpolation(DeltaTime);
	}
	else if (!bAuthority && !bGrabbed)
	{
		PreviousClientLoc    = Owner->GetActorLocation();
		PreviousClientRot    = Owner->GetActorRotation();
		bHasClientInterpInit = false;
	}

	// 로컬 플레이어 몸통 Yaw를 가구 회전에 직접 동기화 (서버·클라 공통)
	// 서버가 원격 플레이어에 SetActorRotation을 보내면 CMC 예측과 충돌해 흔들림 →
	// 대신 각 클라이언트가 자기 주도권으로 로컬에서 처리
	if (bGrabbed && GetWorld())
	{
		APlayerController* PC       = GetWorld()->GetFirstPlayerController();
		ACharacter*        LocalChar = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		if (LocalChar && GrabbedPlayers.Contains(LocalChar) && LocalChar->IsLocallyControlled()
		    && Anchors.Contains(LocalChar))
		{
			const float DesiredYaw = GetDesiredYaw(LocalChar, LocalSyncTargetYaw);
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(LocalChar->GetActorRotation().Yaw, DesiredYaw)) > YawCorrectionDeadzone)
			{
				FRotator NewRot = LocalChar->GetActorRotation();
				NewRot.Yaw      = DesiredYaw;
				LocalChar->SetActorRotation(NewRot);
			}
		}
	}
}

// =====================================================================
// Grab / Release  (서버 전용)
// =====================================================================

void UFurnitureGrabSystem::Grab(ACharacter* Grabber, FVector height, UPrimitiveComponent* GrabberComponent)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !Grabber || GrabbedPlayers.Contains(Grabber))
		return;
	if (FurnitureStat && GrabbedPlayers.Num() >= FurnitureStat->GetRequiredPlayer())
		return;

	// 첫 번째 그랩: 물리 끄기 + 들어올리기 + 이동복제 단일화
	if (GrabbedPlayers.Num() == 0 && FurnitureMesh)
	{
		FurnitureMesh->SetSimulatePhysics(false);
		FurnitureMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		Owner->SetActorLocation(Owner->GetActorLocation() + height, false, nullptr, ETeleportType::TeleportPhysics);
		Owner->SetReplicateMovement(false);
		ServerLocation = Owner->GetActorLocation();
		ServerRotation = Owner->GetActorRotation();
	}

	GrabbedPlayers.Add(Grabber);

	// 잡은 순간 기준값 기록
	{
		FGrabAnchor Anchor;
		Anchor.InitialOffset       = Owner->GetActorLocation() - Grabber->GetActorLocation();
		Anchor.InitialFurnitureYaw = Owner->GetActorRotation().Yaw;
		Anchor.InitialPlayerYaw    = Grabber->GetActorRotation().Yaw;       // 몸통 방향: GetDesiredYaw 기준, 그랩 시 스냅 방지
		Anchor.InitialAimYaw       = Grabber->GetBaseAimRotation().Yaw;     // 카메라 방향: 가구 회전 기준
		Anchors.Add(Grabber, Anchor);
		Multicast_SetPlayerAnchor(Grabber, Anchor.InitialFurnitureYaw, Anchor.InitialPlayerYaw, Anchor.InitialAimYaw, Anchor.InitialOffset);

		// 리슨서버 호스트가 그랩한 경우: Multicast 수신부는 서버에서 조기 return되므로
		// 호스트의 로컬 Yaw 동기화 기준값을 여기서 직접 초기화 (클라와 동일한 시점·값)
		if (Grabber->IsLocallyControlled())
		{
			LocalSyncTargetYaw = Anchor.InitialFurnitureYaw;
		}
	}

	SetGrabCollisionState(Grabber, true);

	if (UCharacterMovementComponent* CMC = Grabber->GetCharacterMovement())
	{
		if (!OriginalMaxWalkSpeeds.Contains(Grabber))
			OriginalMaxWalkSpeeds.Add(Grabber, CMC->MaxWalkSpeed);
		CMC->bOrientRotationToMovement = false;
	}

	// 그랩 순간 무적: 잡는 과정의 스윕/물리 접촉으로 즉시 데미지 입는 것 방지
	if (UFurnitureDamage* DamageComp = Owner->FindComponentByClass<UFurnitureDamage>())
		DamageComp->SetInvincible(0.5f);

	// 모든 현재 그랩 플레이어 이동속도 = BaseSpeed * (현재인원 / 필요인원)
	if (FurnitureStat)
	{
		const int32 Required = FurnitureStat->GetRequiredPlayer();
		if (Required > 0)
		{
			const float NewSpeed = FurnitureStat->GetBaseSpeed() * (float)GrabbedPlayers.Num() / (float)Required;
			for (ACharacter* P : GrabbedPlayers)
			{
				if (UCharacterMovementComponent* PCMC = P->GetCharacterMovement())
					PCMC->MaxWalkSpeed = NewSpeed;
			}
		}
		FurnitureStat->UpdateGrabbedPlayers(GrabbedPlayers.Num());
	}
}

void UFurnitureGrabSystem::Release(ACharacter* Grabber)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !Grabber || !GrabbedPlayers.Contains(Grabber))
		return;

	SetGrabCollisionState(Grabber, false);

	// CMC 원복
	if (UCharacterMovementComponent* CMC = Grabber->GetCharacterMovement())
	{
		if (OriginalMaxWalkSpeeds.Contains(Grabber))
			CMC->MaxWalkSpeed = OriginalMaxWalkSpeeds[Grabber];
		CMC->bOrientRotationToMovement = true;
		CMC->Velocity = FVector::ZeroVector;
	}
	OriginalMaxWalkSpeeds.Remove(Grabber);

	GrabbedPlayers.Remove(Grabber);
	Anchors.Remove(Grabber);
	DraggedLastTick.Remove(Grabber);

	// 남은 그랩 플레이어 이동속도 재계산
	if (FurnitureStat && GrabbedPlayers.Num() > 0)
	{
		const int32 Required = FurnitureStat->GetRequiredPlayer();
		if (Required > 0)
		{
			const float NewSpeed = FurnitureStat->GetBaseSpeed() * (float)GrabbedPlayers.Num() / (float)Required;
			for (ACharacter* P : GrabbedPlayers)
			{
				if (UCharacterMovementComponent* PCMC = P->GetCharacterMovement())
					PCMC->MaxWalkSpeed = NewSpeed;
			}
		}
	}

	if (GrabbedPlayers.Num() == 0 && FurnitureMesh)
	{
		FurnitureMesh->SetSimulatePhysics(true);
		FurnitureMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		Owner->SetReplicateMovement(true);
	}

	// 놓는 순간 무적: 물리 복원 직후 바닥 낙하 접촉(Hit 이벤트)으로
	// 놓자마자 데미지 입는 것 방지
	if (UFurnitureDamage* DamageComp = Owner->FindComponentByClass<UFurnitureDamage>())
		DamageComp->SetInvincible(0.5f);

	if (FurnitureStat)
		FurnitureStat->UpdateGrabbedPlayers(GrabbedPlayers.Num());
}
void UFurnitureGrabSystem::AllRelease()
{
	TArray<ACharacter*> PlayersToRelease = GrabbedPlayers;
	for (ACharacter* Player : PlayersToRelease)
	{
		Release(Player);
	}
}

// =====================================================================
// 가구 단독 이동 (오프셋 적용)
// =====================================================================

void UFurnitureGrabSystem::AddFurnitureOffset(FVector LocationOffset, float YawOffset, float PitchOffset)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
		return;

	// 이동/회전 적용 전의 트랜스폼 기록
	FVector OldLoc = Owner->GetActorLocation();
	float OldYaw = Owner->GetActorRotation().Yaw;

	// 가구 단독 이동 및 회전 적용
	// Pitch(기울이기)는 HandleMovement가 매 틱 현재값을 유지하므로 여기서 바꾸면 그대로 운반됨
	FVector NewLoc = OldLoc + LocationOffset;
	FRotator NewRot = Owner->GetActorRotation();
	NewRot.Yaw += YawOffset;
	NewRot.Pitch += PitchOffset;
	Owner->SetActorLocationAndRotation(NewLoc, NewRot, true);

	FVector ActualLoc = Owner->GetActorLocation();
	float ActualYaw = Owner->GetActorRotation().Yaw;

	FVector ActualLocDelta = ActualLoc - OldLoc;
	float ActualYawDelta = FMath::FindDeltaAngleDegrees(OldYaw, ActualYaw);

	// 서버 내부 변수 갱신
	ServerLocation = ActualLoc;
	ServerRotation = Owner->GetActorRotation();

	// 리슨서버 호스트도 클라(ApplySystemOffset 수신부)와 동일하게 목표 Yaw 갱신.
	// 앵커의 InitialFurnitureYaw도 같은 델타만큼 밀리므로 GetDesiredYaw 결과는 불변
	// → 가구만 회전하고 호스트 캐릭터는 돌지 않음 (의도된 동작 유지).
	LocalSyncTargetYaw = ServerRotation.Yaw;

	for (ACharacter* P : GrabbedPlayers)
	{
		if (P && Anchors.Contains(P))
		{
			Anchors[P].InitialOffset += ActualLocDelta;
			Anchors[P].InitialFurnitureYaw += ActualYawDelta;
		}
	}

	DraggedLastTick.Empty();
	StoppedDraggingLastTick.Empty();

	SystemOffsetSequence++;

	// 트랜스폼 갱신과 앵커 갱신을 패킷으로 묶어 클라이언트에 전송
	Multicast_ApplySystemOffset(ActualLocDelta, ActualYawDelta, ServerLocation, ServerRotation, SystemOffsetSequence);
}

// =====================================================================
// 헬퍼
// =====================================================================

FVector UFurnitureGrabSystem::GetAttachedLocation(ACharacter* Player, const FVector& FurnitureLoc, float FurnitureYaw) const
{
	const FGrabAnchor& A      = Anchors[Player];
	const float        Delta  = FMath::FindDeltaAngleDegrees(A.InitialFurnitureYaw, FurnitureYaw);
	const FVector      Offset = A.InitialOffset.RotateAngleAxis(Delta, FVector::UpVector);
	FVector Target = FurnitureLoc - Offset;
	Target.Z = Player->GetActorLocation().Z;
	return Target;
}

float UFurnitureGrabSystem::GetDesiredYaw(ACharacter* Player, float FurnitureYaw) const
{
	const FGrabAnchor& A = Anchors[Player];
	return A.InitialPlayerYaw + FMath::FindDeltaAngleDegrees(A.InitialFurnitureYaw, FurnitureYaw);
}

// =====================================================================
// HandleMovement  (서버 틱)
// =====================================================================

void UFurnitureGrabSystem::HandleMovement(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !FurnitureStat)
		return;

	// ---- 0. 유효 플레이어 수집 ----
	TArray<ACharacter*> Players;
	Players.Reserve(GrabbedPlayers.Num());
	for (ACharacter* P : GrabbedPlayers)
	{
		if (P && Anchors.Contains(P))
			Players.Add(P);
	}
	if (Players.Num() == 0)
		return;

	const int32  N             = Players.Num();
	const FVector CurFurnLoc   = Owner->GetActorLocation();
	const float   CurFurnYaw   = Owner->GetActorRotation().Yaw;

	// ---- 1. 각 플레이어의 "내가 주도한다면 가구는 여기" 제안 + 활동량 가중치 계산 ----
	//   활동량 = 현재 가구 위치에서 제안 위치까지의 거리. 더 많이 움직인 사람이 더 큰 가중치.
	//   피동(끌려가는) 플레이어는 가중치 0: 뒤처진 피동 플레이어의 제안이 가구를 역방향으로 당기는 것을 방지.
	const float Eps = 0.01f;
	TArray<double> Weights;
	Weights.Init(0.0, N);
	double WTotal = 0.0, WSumSin = 0.0, WSumCos = 0.0;

	for (int32 i = 0; i < N; ++i)
	{
		ACharacter* P = Players[i];
		if (DraggedLastTick.Contains(P) || StoppedDraggingLastTick.Contains(P))
			continue;  // Weights[i] = 0 (이미 초기화됨)

		const FGrabAnchor& Anc           = Anchors[P];
		const float        PlayerAimYaw   = P->GetBaseAimRotation().Yaw;
		const float        PlayerYawDelta = FMath::FindDeltaAngleDegrees(Anc.InitialAimYaw, PlayerAimYaw);
		const float        ProposalYaw   = Anc.InitialFurnitureYaw + PlayerYawDelta;
		const FVector      ProposalLoc   = P->GetActorLocation() + Anc.InitialOffset.RotateAngleAxis(PlayerYawDelta, FVector::UpVector);
		const float        Demand        = FVector(ProposalLoc.X - CurFurnLoc.X, ProposalLoc.Y - CurFurnLoc.Y, 0.0f).Size();
		const double       W             = Demand + Eps;

		Weights[i] = W;
		WTotal   += W;
		WSumSin  += W * FMath::Sin(FMath::DegreesToRadians(ProposalYaw));
		WSumCos  += W * FMath::Cos(FMath::DegreesToRadians(ProposalYaw));
	}

	// 교착 방지: 벽 충돌 등으로 전원 피동 판정 → WTotal=0 → 가구 영구 동결
	// 피동 추적을 초기화해 모든 플레이어를 능동으로 복귀, 재계산
	if (WTotal <= 0.0)
	{
		DraggedLastTick.Empty();
		StoppedDraggingLastTick.Empty();
		WTotal = 0.0; WSumSin = 0.0; WSumCos = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			ACharacter* P = Players[i];
			const FGrabAnchor& Anc           = Anchors[P];
			const float        PlayerAimYaw   = P->GetBaseAimRotation().Yaw;
			const float        PlayerYawDelta = FMath::FindDeltaAngleDegrees(Anc.InitialAimYaw, PlayerAimYaw);
			const float        ProposalYaw   = Anc.InitialFurnitureYaw + PlayerYawDelta;
			const FVector      ProposalLoc   = P->GetActorLocation() + Anc.InitialOffset.RotateAngleAxis(PlayerYawDelta, FVector::UpVector);
			const float        Demand        = FVector(ProposalLoc.X - CurFurnLoc.X, ProposalLoc.Y - CurFurnLoc.Y, 0.0f).Size();
			const double       W             = Demand + Eps;
			Weights[i] = W;
			WTotal   += W;
			WSumSin  += W * FMath::Sin(FMath::DegreesToRadians(ProposalYaw));
			WSumCos  += W * FMath::Cos(FMath::DegreesToRadians(ProposalYaw));
		}
	}

	// ---- 2. 가구 목표 Yaw + 위치 결정 ----
	// FixedTurn: 한 틱에 FurnYawRotationSpeed*DT 이상 회전 불가 → 빠른 카메라 회전 시 가구 튐 방지
	const float TargetYawRaw = FMath::RadiansToDegrees(FMath::Atan2(WSumSin, WSumCos));
	const float TargetYaw    = FMath::FixedTurn(CurFurnYaw, TargetYawRaw, FurnYawRotationSpeed * DeltaTime);

	FVector WLocSum = FVector::ZeroVector;
	for (int32 i = 0; i < N; ++i)
	{
		ACharacter*        P   = Players[i];
		const FGrabAnchor& Anc = Anchors[P];
		const float        YC  = FMath::FindDeltaAngleDegrees(Anc.InitialFurnitureYaw, TargetYaw);
		// Z도 그랩 시점 오프셋(InitialOffset.Z)을 유지 → 플레이어가 낙하하면 가구도 따라 내려감
		// (UpVector 회전은 Z를 보존하므로 Prop.Z = 플레이어Z + 오프셋Z)
		FVector Prop = P->GetActorLocation() + Anc.InitialOffset.RotateAngleAxis(YC, FVector::UpVector);
		WLocSum += Weights[i] * Prop;
	}
	FVector TargetLoc = (WTotal > 0.0) ? (WLocSum / WTotal) : CurFurnLoc;

	// ---- 3. 가구 이동 (sweep=true, 가구 자체 충돌) ----
	// Pitch/Roll은 현재 값 유지: 물리로 쓰러진 가구는 그 자세 그대로 운반 (억지로 세우면 바닥 파고듦)
	// AddFurnitureOffset으로 기울인 자세도 그대로 존중됨 (좁은 곳 통과용)
	FRotator TargetRot = Owner->GetActorRotation();
	TargetRot.Yaw = TargetYaw;
	Owner->SetActorLocationAndRotation(TargetLoc, TargetRot, true);
	
	// sweep 이동 도중 발생한 물리/데미지 이벤트(가구 파괴 등)로 인해 
	// 플레이어가 동기적으로 Release 되었을 수 있으므로 로컬 배열(Players)의 유효성을 다시 갱신합니다.
	for (int32 i = Players.Num() - 1; i >= 0; --i)
	{
		if (!Anchors.Contains(Players[i]))
		{
			Players.RemoveAt(i);
		}
	}
	// 갱신 후 남은 플레이어가 없다면 아래 로직(위치 보정 등)을 수행할 필요 없이 즉시 중단합니다.
	if (Players.Num() == 0)
		return;

	FVector ActualLoc = Owner->GetActorLocation();
	const float ActualYaw = Owner->GetActorRotation().Yaw;

	// ---- 3.5. 회전 막힘 감지 → 위치 기준점 리셋 (호(弧) 미끄러짐 방지) ----
	// 회전이 막히면 TargetYaw는 매 틱 증가하지만 ActualYaw는 고정 → Step 2의 YC가 누적
	// → TargetLoc이 호(弧)를 순회 → 가구가 벽을 따라 이리저리 미끄러짐.
	// InitOffset+InitFurnYaw 리셋 시 다음 틱 YC ≈ 8.9°(1틱분) → TargetLoc ≈ ActualLoc(안정).
	// InitAimYaw 유지 → ProposalYaw = ActualYaw + 원래카메라각도 → 막힘 해제 후 즉시 정상 회전.
	{
		const bool bRotationBlocked =
			FMath::Abs(FMath::FindDeltaAngleDegrees(TargetYaw, ActualYaw)) > CorrectionDeadzone;
		if (bRotationBlocked)
		{
			for (int32 i = 0; i < N; ++i)
			{
				ACharacter* P = Players[i];
				if (!Anchors.Contains(P)) continue;
				if (DraggedLastTick.Contains(P) || StoppedDraggingLastTick.Contains(P)) continue;

				FGrabAnchor& Anc        = Anchors[P];
				Anc.InitialOffset       = ActualLoc - P->GetActorLocation();
				Anc.InitialFurnitureYaw = ActualYaw;
				Anc.InitialAimYaw       = P->GetBaseAimRotation().Yaw;
				// InitialPlayerYaw 유지 → GetDesiredYaw 정합성 유지
				Multicast_SetPlayerAnchor(P, ActualYaw, Anc.InitialPlayerYaw,
				                          Anc.InitialAimYaw, Anc.InitialOffset);
			}
		}
	}

	// ---- 3.6. Z 이동 막힘(바닥 착지 등) → Z 오프셋 리셋 ----
	// 가구가 바닥에 먼저 닿았는데 플레이어가 더 낮게 있으면 목표 Z가 바닥 아래로 남아
	// 매 틱 바닥에 밀어붙이게 됨 → 실제 도달한 Z 기준으로 오프셋 재기록 (3.5와 동일 패턴)
	if (FMath::Abs(TargetLoc.Z - ActualLoc.Z) > CorrectionDeadzone)
	{
		for (ACharacter* P : Players)
		{
			if (FGrabAnchor* Anc = Anchors.Find(P))
				Anc->InitialOffset.Z = ActualLoc.Z - P->GetActorLocation().Z;
		}
	}

	// 안전장치: 너무 멀어진 플레이어 자동 해제
	TArray<ACharacter*> ToRelease;
	const float MaxSepSq = FMath::Square(MaxGrabSeparationDistance);
	for (ACharacter* P : Players)
	{
		const FVector D = FVector(P->GetActorLocation() - GetAttachedLocation(P, ActualLoc, ActualYaw));
		if (FVector(D.X, D.Y, 0.0f).SizeSquared() > MaxSepSq)
			ToRelease.Add(P);
	}

	// ---- 4. 벽 막힘 감지 → 가구 후퇴 (플레이어 직접 이동 없음, CMC 충돌 없음) ----
	if (bBlockedCarrierStopsFurniture)
	{
		FVector WorstBlock = FVector::ZeroVector;
		for (ACharacter* P : Players)
		{
			UCapsuleComponent* Cap = P->GetCapsuleComponent();
			if (!Cap)
				continue;

			const FVector Att      = GetAttachedLocation(P, ActualLoc, ActualYaw);
			const FVector StartPos = P->GetActorLocation();
			const FVector EndPos(Att.X, Att.Y, StartPos.Z);

			if ((EndPos - StartPos).SizeSquared2D() < KINDA_SMALL_NUMBER)
				continue;

			FCollisionShape Shape = FCollisionShape::MakeCapsule(
				Cap->GetScaledCapsuleRadius(), Cap->GetScaledCapsuleHalfHeight());
			FCollisionQueryParams QP;
			QP.AddIgnoredActor(Owner);
			for (ACharacter* Other : Players)  // 그랩 플레이어끼리 오탐 WorstBlock 방지
				QP.AddIgnoredActor(Other);

			FHitResult Hit;
			if (GetWorld()->SweepSingleByProfile(Hit, StartPos, EndPos, FQuat::Identity,
				Cap->GetCollisionProfileName(), Shape, QP))
			{
				const FVector Shortfall(Att.X - Hit.Location.X, Att.Y - Hit.Location.Y, 0.0f);
				if (Shortfall.SizeSquared() > WorstBlock.SizeSquared())
					WorstBlock = Shortfall;
			}
		}
		if (WorstBlock.SizeSquared() > FMath::Square(BlockStopThreshold))
		{
			// WorstBlock은 XY 성분만 있음(Shortfall Z=0) → Z는 Step 3 결과를 유지
			// (CurFurnZ로 되돌리면 Z 추종(낙하 따라가기)을 매번 무효화하게 됨)
			ActualLoc -= WorstBlock;
			Owner->SetActorLocation(ActualLoc, false);
			ActualLoc = Owner->GetActorLocation();
		}
	}

	// ---- 5. CMC 속도 주입으로 플레이어 이동 제어 ----
	//
	//   DraggedLastTick 으로 각 플레이어의 이전 틱 피동 여부를 추적하여 능동/피동 판별:
	//   - bAtTarget && !bWasDragged : 능동 주도자 (드래그된 적 없이 목표 위치에 있음 = 직접 걷는 중)
	//                                 → CMC·Yaw 모두 간섭 안 함
	//   - bAtTarget && bWasDragged  : 피동 플레이어가 방금 목표 도달 → ZeroVector 주입 (슬라이딩 방지)
	//   - !bAtTarget                : 피동 → CarryVelocity 주입
	//
	//   GetCurrentAcceleration()(리모트 클라에서 서버가 읽으면 0) 및
	//   bFurnitureMoving(프레임레이트 의존, 첫 틱 소이동 시 오판) 두 가지 방식 모두 폐기.

	TSet<ACharacter*> CurrentTickDragged;
	TSet<ACharacter*> StoppedDraggingThisTick;

	for (ACharacter* P : Players)
	{
		UCharacterMovementComponent* CMC = P->GetCharacterMovement();
		if (!CMC)
			continue;

		const FVector Att      = GetAttachedLocation(P, ActualLoc, ActualYaw);
		const FVector Delta    = FVector(Att.X - P->GetActorLocation().X, Att.Y - P->GetActorLocation().Y, 0.0f);
		const bool bAtTarget   = Delta.SizeSquared() <= FMath::Square(CorrectionDeadzone);
		const bool bWasDragged = DraggedLastTick.Contains(P);

		if (bAtTarget && !bWasDragged)
		{
			// 능동 주도자: CMC 속도 간섭 없음.
			// Yaw는 서버에서 SetActorRotation → 자동 복제 → 다른 클라가 캐릭터 회전을 볼 수 있음.
			// Multicast_ApplyPlayerCorrection → 해당 클라이언트 즉시 적용.
			const float DesiredYaw = GetDesiredYaw(P, ActualYaw);
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(P->GetActorRotation().Yaw, DesiredYaw)) > YawCorrectionDeadzone)
			{
				FRotator NewRot = P->GetActorRotation();
				NewRot.Yaw = DesiredYaw;
				P->SetActorRotation(NewRot);
				Multicast_ApplyPlayerCorrection(P, FVector::ZeroVector, DesiredYaw);
			}
			continue;
		}

		// 여기 이후 = 피동·정지 플레이어 (능동 주도자는 위 블록에서 continue됨)
		const float DesiredYaw = GetDesiredYaw(P, ActualYaw);
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(P->GetActorRotation().Yaw, DesiredYaw)) > YawCorrectionDeadzone)
		{
			FRotator NewRot = P->GetActorRotation();
			NewRot.Yaw = DesiredYaw;
			P->SetActorRotation(NewRot);
		}

		if (bAtTarget && bWasDragged)
		{
			// 피동 플레이어가 방금 목표에 도달 → 정지 (관성 슬라이딩 방지)
			CMC->Velocity = FVector::ZeroVector;
			Multicast_ApplyPlayerCorrection(P, FVector::ZeroVector, DesiredYaw);
			StoppedDraggingThisTick.Add(P);

			// 앵커 갱신: 도달 시점의 가구 상태를 새 기준점으로 설정
			// 갱신하지 않으면 다음 틱 ProposalYaw = grab당시InitFurnYaw + 카메라Delta
			// = 이전 가구 Yaw 기준 → 능동 플레이어의 ProposalYaw와 충돌 → 역회전 → 상호 피동 진동.
			// 갱신하면 ProposalYaw = ActualYaw + 0 = 현재 가구 Yaw → 두 플레이어 Yaw 제안 일치 → 안정.
			{
				FGrabAnchor& Anc        = Anchors[P];
				Anc.InitialOffset       = ActualLoc - P->GetActorLocation();
				Anc.InitialFurnitureYaw = ActualYaw;
				Anc.InitialAimYaw       = P->GetBaseAimRotation().Yaw;
				Anc.InitialPlayerYaw    = P->GetActorRotation().Yaw;
				Multicast_SetPlayerAnchor(P, ActualYaw, Anc.InitialPlayerYaw, Anc.InitialAimYaw, Anc.InitialOffset);
			}
			continue;
		}

		// !bAtTarget: 피동 → 목표를 향해 끌어당김
		// BrakingDecel 보상: CMC가 다음 틱 시작 시 BrakingDecel*DT 만큼 속도를 감쇠시키므로
		// 그만큼 더 주입해 실질 이동거리가 Delta와 일치하도록 함.
		// 서버·클라 모두 동일하게 BrakingDecel 감쇠 적용 → 동일 이동 → ClientAdjustPosition 없음.
		const FVector NeededVelocity = (Delta / DeltaTime).GetClampedToMaxSize(MaxCorrectionSpeed);
		const FVector CarryVelocity  = (NeededVelocity + NeededVelocity.GetSafeNormal() * CMC->BrakingDecelerationWalking * DeltaTime)
		                               .GetClampedToMaxSize(MaxCorrectionSpeed);
		CMC->Velocity = CarryVelocity;
		Multicast_ApplyPlayerCorrection(P, CarryVelocity, DesiredYaw);
		CurrentTickDragged.Add(P);
	}

	DraggedLastTick          = MoveTemp(CurrentTickDragged);
	StoppedDraggingLastTick  = MoveTemp(StoppedDraggingThisTick);

	// ---- 6. 안전장치 처리 ----
	for (ACharacter* P : ToRelease)
		Release(P);

	// ---- 7. 클라 보간용 트랜스폼 갱신 ----
	ServerLocation = Owner->GetActorLocation();
	ServerRotation = Owner->GetActorRotation();
	Multicast_UpdateFurnitureTransform(ServerLocation, ServerRotation, SystemOffsetSequence);

	// 리슨서버 호스트 보정: LocalSyncTargetYaw 갱신은 Multicast 수신부에만 있는데
	// 서버에서는 전부 조기 return되어 호스트의 로컬 Yaw 동기화 블록(TickComponent)이
	// 갱신 안 된 값으로 매 틱 스냅 → 호스트 캐릭터가 회전을 따라가지 못함.
	// 서버에서도 클라와 동일하게 최신 가구 Yaw로 갱신.
	LocalSyncTargetYaw = ServerRotation.Yaw;

#if !UE_BUILD_SHIPPING
	{
		const float FurnActualSpeed = FVector(ServerLocation - CurFurnLoc).Size2D() / DeltaTime;
		const int32 Required        = FurnitureStat->GetRequiredPlayer();
		const float BaseSpeed       = FurnitureStat->GetBaseSpeed();
		const float FurnMaxSpeed    = (Required > 0) ? (BaseSpeed * Players.Num() / (float)Required) : 0.0f;

		TArray<float> MaxSpeeds, ActualSpeeds;
		for (ACharacter* P : Players)
		{
			if (UCharacterMovementComponent* CMC = P->GetCharacterMovement())
			{
				MaxSpeeds.Add(CMC->MaxWalkSpeed);
				ActualSpeeds.Add(FVector(CMC->Velocity.X, CMC->Velocity.Y, 0.0f).Size());
			}
		}
		Multicast_ShowDebugSpeeds(FurnActualSpeed, FurnMaxSpeed, MaxSpeeds, ActualSpeeds);
	}
#endif
}

// =====================================================================
// Multicast: 클라이언트 CMC 속도 동기화
// =====================================================================

void UFurnitureGrabSystem::Multicast_SetPlayerAnchor_Implementation(
	ACharacter* Player, float InitFurnYaw, float InitPlayerYaw, float InitAimYaw, FVector InitOffset)
{
	if (!Player || (GetOwner() && GetOwner()->HasAuthority()))
		return;

	// 서버 Grab() 시점의 정확한 기준값으로 클라 앵커를 설정/갱신.
	// OnRep에서 이미 임시 앵커가 만들어졌어도 Reliable이므로 반드시 덮어씀.
	FGrabAnchor& A        = Anchors.FindOrAdd(Player);
	A.InitialFurnitureYaw = InitFurnYaw;
	A.InitialPlayerYaw    = InitPlayerYaw;
	A.InitialAimYaw       = InitAimYaw;
	A.InitialOffset       = InitOffset;

	if (Player->IsLocallyControlled())
	{
		LocalSyncTargetYaw = InitFurnYaw;
	}
}

void UFurnitureGrabSystem::Multicast_UpdateFurnitureTransform_Implementation(FVector NewLocation, FRotator NewRotation, uint8 SeqID)
{
	if (GetOwner() && GetOwner()->HasAuthority())
		return;

	if (SeqID != LocalSystemOffsetSequence)
	{
		return; 
	}

	ServerLocation = NewLocation;
	ServerRotation = NewRotation;

	LocalSyncTargetYaw = NewRotation.Yaw;

	LocalSystemOffsetSequence = SeqID;
}

void UFurnitureGrabSystem::Multicast_ShowDebugSpeeds_Implementation(
	float FurnActualSpeed, float FurnMaxSpeed,
	const TArray<float>& MaxWalkSpeeds, const TArray<float>& ActualSpeeds)
{
//#if !UE_BUILD_SHIPPING
//	if (!GEngine) return;
//	GEngine->AddOnScreenDebugMessage(9000, 0.1f, FColor::Yellow,
//		FString::Printf(TEXT("[가구] 실속도: %.0f  /  설정최대속도: %.0f"), FurnActualSpeed, FurnMaxSpeed));
//	for (int32 i = 0; i < MaxWalkSpeeds.Num(); ++i)
//	{
//		GEngine->AddOnScreenDebugMessage(9001 + i, 0.1f, FColor::Cyan,
//			FString::Printf(TEXT("  [P%d] MaxWalkSpeed: %.0f  /  현재속도: %.0f"),
//				i + 1, MaxWalkSpeeds[i], ActualSpeeds[i]));
//	}
//#endif
}

void UFurnitureGrabSystem::Multicast_ApplyPlayerCorrection_Implementation(
	ACharacter* Player, FVector CarryVelocity, float TargetYaw)
{
	if (!Player)
		return;
	if (GetOwner() && GetOwner()->HasAuthority())
		return;
	if (!Player->IsLocallyControlled())
		return;

	// 서버와 동일한 velocity를 클라 CMC에 설정.
	// 서버/클라 CMC가 같은 속도로 같은 거리를 이동 → 예측 일치 → ClientAdjustPosition 없음.
	if (UCharacterMovementComponent* CMC = Player->GetCharacterMovement())
		CMC->Velocity = CarryVelocity;

	// Yaw 보정 (bOrientRotationToMovement=false 상태이므로 안전)
	FRotator NewRot = Player->GetActorRotation();
	NewRot.Yaw = TargetYaw;
	Player->SetActorRotation(NewRot);
}

// =====================================================================
// 클라 가구 보간
// =====================================================================

void UFurnitureGrabSystem::Multicast_ApplySystemOffset_Implementation(FVector ActualLocDelta, float ActualYawDelta, FVector NewServerLoc, FRotator NewServerRot, uint8 SeqID)
{
	// 서버 기준으로는 이미처리됨
	if (GetOwner() && GetOwner()->HasAuthority())
		return;

	// 클라이언트 보간용 목표 트랜스폼 갱신
	ServerLocation = NewServerLoc;
	ServerRotation = NewServerRot;

	// 트랜스폼과 동일한 프레임에 앵커를 갱신하여 GetDesiredYaw 연산 시 오차가 발생하는 것을 원천 차단
	for (ACharacter* P : GrabbedPlayers)
	{
		if (P && Anchors.Contains(P))
		{
			Anchors[P].InitialOffset += ActualLocDelta;
			Anchors[P].InitialFurnitureYaw += ActualYawDelta;
		}
	}

	DraggedLastTick.Empty();
	StoppedDraggingLastTick.Empty();

	LocalSyncTargetYaw = NewServerRot.Yaw;

	LocalSystemOffsetSequence = SeqID;
}

void UFurnitureGrabSystem::UpdateClientInterpolation(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner)
		return;

	if (!bHasClientInterpInit)
	{
		PreviousClientLoc    = Owner->GetActorLocation();
		PreviousClientRot    = Owner->GetActorRotation();
		bHasClientInterpInit = true;
	}

	const FVector  NewLoc = FMath::VInterpTo(PreviousClientLoc, ServerLocation, DeltaTime, ClientInterpSpeed);
	const FRotator NewRot = FMath::RInterpTo(PreviousClientRot, ServerRotation,  DeltaTime, ClientInterpSpeed);
	Owner->SetActorLocationAndRotation(NewLoc, NewRot, false);
	PreviousClientLoc = NewLoc;
	PreviousClientRot = NewRot;
}

// =====================================================================
// OnRep_GrabbedPlayers: 클라에서 CMC 설정 동기화
// =====================================================================

void UFurnitureGrabSystem::OnRep_GrabbedPlayers()
{
	// 가구 물리/충돌 동기화
	if (FurnitureMesh)
	{
		if (GrabbedPlayers.Num() > 0)
		{
			FurnitureMesh->SetSimulatePhysics(false);
			FurnitureMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		}
		else
		{
			FurnitureMesh->SetSimulatePhysics(true);
			FurnitureMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		}
	}

	// 더 이상 잡고 있지 않은 플레이어 정리
	for (int32 i = ClientTrackedPlayers.Num() - 1; i >= 0; --i)
	{
		ACharacter* P = ClientTrackedPlayers[i];
		if (!P || !GrabbedPlayers.Contains(P))
		{
			if (P)
			{
				SetGrabCollisionState(P, false);
				Anchors.Remove(P);

				if (P->IsLocallyControlled() && bLocalCMCModified)
				{
					if (UCharacterMovementComponent* CMC = P->GetCharacterMovement())
					{
						CMC->bOrientRotationToMovement = true;
						CMC->Velocity                  = FVector::ZeroVector;
					}
					bLocalCMCModified = false;
				}
			}
			ClientTrackedPlayers.RemoveAt(i);
		}
	}

	// 새로 잡은 플레이어 셋업
	for (ACharacter* P : GrabbedPlayers)
	{
		if (P && !ClientTrackedPlayers.Contains(P))
		{
			SetGrabCollisionState(P, true);
			ClientTrackedPlayers.Add(P);

			// 클라이언트에서 로컬 플레이어의 Anchor 복구 (Grab()은 서버 전용이므로 클라에는 없음)
			// GetDesiredYaw 및 TickComponent 로컬 Yaw 보정에 필요
			if (P->IsLocallyControlled() && GetOwner())
			{
				FGrabAnchor LocalAnchor;
				LocalAnchor.InitialOffset       = GetOwner()->GetActorLocation() - P->GetActorLocation();
				LocalAnchor.InitialFurnitureYaw = GetOwner()->GetActorRotation().Yaw;
				LocalAnchor.InitialPlayerYaw    = P->GetActorRotation().Yaw;       // 몸통 방향 (스냅 방지)
				LocalAnchor.InitialAimYaw       = P->GetBaseAimRotation().Yaw;     // 카메라 방향 (Multicast로 서버값으로 덮어씌워짐)
				Anchors.Add(P, LocalAnchor);

				LocalSyncTargetYaw = LocalAnchor.InitialFurnitureYaw;
			}

			// 로컬 플레이어의 CMC를 서버 Grab()과 동일한 상태로 전환
			if (P->IsLocallyControlled() && !bLocalCMCModified)
			{
				if (UCharacterMovementComponent* CMC = P->GetCharacterMovement())
				{
					CMC->bOrientRotationToMovement = false;
				}
				bLocalCMCModified = true;
			}
		}
	}

	UpdateLocalWalkSpeed();
}

// =====================================================================
// 이동속도 클라 동기화 (OnRep 에서 호출)
// =====================================================================

void UFurnitureGrabSystem::UpdateLocalWalkSpeed()
{
	if (!GetWorld())
		return;

	APlayerController* PC       = GetWorld()->GetFirstPlayerController();
	ACharacter*        LocalChar = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!LocalChar)
		return;

	UCharacterMovementComponent* CMC = LocalChar->GetCharacterMovement();
	if (!CMC)
		return;

	const bool bGrabbingNow = GrabbedPlayers.Contains(LocalChar);
	if (bGrabbingNow)
	{
		if (!bLocalSpeedReduced)
		{
			LocalOriginalMaxWalkSpeed = CMC->MaxWalkSpeed;
			bLocalSpeedReduced = true;
		}
		// OnRep는 인원이 바뀔 때마다 호출되므로 매번 이속을 재계산한다
		if (FurnitureStat && FurnitureStat->GetRequiredPlayer() > 0)
			CMC->MaxWalkSpeed = FurnitureStat->GetBaseSpeed() * (float)GrabbedPlayers.Num() / (float)FurnitureStat->GetRequiredPlayer();
	}
	else if (bLocalSpeedReduced)
	{
		CMC->MaxWalkSpeed  = LocalOriginalMaxWalkSpeed;
		bLocalSpeedReduced = false;
	}
}

// =====================================================================
// 충돌 무시 / 틱 순서 셋업
// =====================================================================

void UFurnitureGrabSystem::SetGrabCollisionState(ACharacter* Player, bool bEnable)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Player)
		return;

	UCapsuleComponent* Cap = Player->GetCapsuleComponent();

	// 플레이어 캡슐이 가구 액터를 sweep 시 무시
	if (Cap)
		Cap->IgnoreActorWhenMoving(Owner, bEnable);

	// 가구 루트 컴포넌트가 플레이어를 sweep 시 무시
	// (SetActorLocationAndRotation bSweep=true 는 루트 컴포넌트로 sweep → AActor::MoveIgnoreActorAdd 대신 직접 접근)
	if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
		RootPrim->IgnoreActorWhenMoving(Player, bEnable);

	// FurnitureMesh가 루트가 아닐 경우를 대비해 컴포넌트 레벨에서도 무시
	if (FurnitureMesh && Cap)
	{
		FurnitureMesh->IgnoreComponentWhenMoving(Cap, bEnable);
		Cap->IgnoreComponentWhenMoving(FurnitureMesh, bEnable);
	}

	// GrabSystem이 CMC 이후에 틱하도록 → 최신 플레이어 위치를 읽음
	if (UCharacterMovementComponent* CMC = Player->GetCharacterMovement())
	{
		if (bEnable)
			AddTickPrerequisiteComponent(CMC);
		else
			RemoveTickPrerequisiteComponent(CMC);
	}
}
