// Fill out your copyright notice in the Description page of Project Settings.

#include "FurnitureGrabSystem.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/OverlapResult.h"
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
				ApplyBodyYaw(LocalChar, DesiredYaw, DeltaTime);   // 즉시 스냅 대신 보간
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
		// [시야] 운반 중 가구가 스프링암 카메라 프로브를 밀어내 큰 가구를 들면 화면이
		// 가구 안으로 처박히는 문제 — 잡혀 있는 동안만 카메라 채널을 무시한다.
		// 놓을 때 PhysicsActor 프로파일 복원이 응답을 원상복구하므로 별도 처리 불필요.
		FurnitureMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

		CurrentHeightOffset = 0.0f;

		// 들어올릴 시 높이를 잡은 플레이어 기준 [FurnitureHeightMin, FurnitureHeightMax] 범위로 제한.
		// [피벗 오프셋 보정] 메쉬 중심축(피벗)이 실제 메쉬와 멀리 떨어진 가구는 피벗 Z로 제약하면
		// 시각 메쉬가 엉뚱한 높이(천장/바닥)에 감. Bounds에서 피벗→메쉬중심 Z오프셋을 구해
		// '메쉬 중심'이 범위에 오도록 클램프한 뒤 피벗으로 역산 (메쉬 무수정 해결).
		const float MeshCenterOffZ = FurnitureMesh->Bounds.Origin.Z - Owner->GetActorLocation().Z;
		FVector sumLocation = Owner->GetActorLocation() + height;
		const float ClampedCenterZ = FMath::Clamp(sumLocation.Z + MeshCenterOffZ,
			Grabber->GetActorLocation().Z + FurnitureHeightMin,
			Grabber->GetActorLocation().Z + FurnitureHeightMax);
		sumLocation.Z = ClampedCenterZ - MeshCenterOffZ;

		Owner->SetActorLocation(sumLocation, true, nullptr, ETeleportType::ResetPhysics);
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
		Anchor.PrevAimYaw          = Anchor.InitialAimYaw;                  // 견인 중 자기 회전 입력 감지 기준
		Anchor.InitialAimPitch     = Grabber->GetBaseAimRotation().Pitch;   // 카메라 상하: 가구 높이 조절 기준
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

	// 모든 현재 그랩 플레이어 이동속도 갱신 (필요 인원 미달이면 대폭 감속)
	if (FurnitureStat)
	{
		const float NewSpeed = ComputeCarrySpeed();
		for (ACharacter* P : GrabbedPlayers)
		{
			if (UCharacterMovementComponent* PCMC = P->GetCharacterMovement())
				PCMC->MaxWalkSpeed = NewSpeed;
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

	// CMC 원복 (Z 속도는 보존: 낙하 중 자동 해제 시 공중 정지 방지)
	if (UCharacterMovementComponent* CMC = Grabber->GetCharacterMovement())
	{
		if (OriginalMaxWalkSpeeds.Contains(Grabber))
			CMC->MaxWalkSpeed = OriginalMaxWalkSpeeds[Grabber];
		CMC->bOrientRotationToMovement = true;
		CMC->Velocity = FVector(0.0f, 0.0f, CMC->Velocity.Z);
	}
	OriginalMaxWalkSpeeds.Remove(Grabber);

	GrabbedPlayers.Remove(Grabber);
	Anchors.Remove(Grabber);
	DraggedLastTick.Remove(Grabber);

	// 남은 그랩 플레이어 이동속도 재계산 (필요 인원 미달이면 대폭 감속)
	if (FurnitureStat && GrabbedPlayers.Num() > 0)
	{
		const float NewSpeed = ComputeCarrySpeed();
		for (ACharacter* P : GrabbedPlayers)
		{
			if (UCharacterMovementComponent* PCMC = P->GetCharacterMovement())
				PCMC->MaxWalkSpeed = NewSpeed;
		}
	}

	if (GrabbedPlayers.Num() == 0 && FurnitureMesh)
	{
		// [관통 복구] 인원미달 끌림 자세는 바닥에 닿아 있고, 회전은 스윕 보정이 없어
		// 모서리가 지면에 박혀 있을 수 있음 → 물리 켜기 전에 겹침이 풀릴 때까지 들어올린다.
		// (박힌 채 물리를 켜면 그대로 잠기거나 튕겨나감)
		if (UWorld* World = GetWorld())
		{
			FComponentQueryParams DepenParams(SCENE_QUERY_STAT(ReleaseDepenetrate), Owner);
			FCollisionObjectQueryParams StaticOnly(ECC_WorldStatic);
			TArray<FOverlapResult> Overlaps;
			for (int32 Step = 0; Step < 10; ++Step)
			{
				Overlaps.Reset();
				// +2: 바닥 정상 접촉을 겹침으로 오탐하지 않도록 살짝 띄운 위치에서 검사
				const bool bPenetrating = World->ComponentOverlapMulti(
					Overlaps, FurnitureMesh,
					FurnitureMesh->GetComponentLocation() + FVector(0.f, 0.f, 2.f),
					FurnitureMesh->GetComponentQuat(), DepenParams, StaticOnly);
				if (!bPenetrating)
				{
					break;
				}
				Owner->AddActorWorldOffset(FVector(0.f, 0.f, 3.f), false);
			}
		}

		FurnitureMesh->SetSimulatePhysics(true);
		FurnitureMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		Owner->SetReplicateMovement(true);

		bYawStalemate = false;            // 다음 그랩에 교착 상태 누출 방지
		bCarrierBlockedLastTick = false;  // 다음 그랩에 막힘 상태 누출 방지
	}

	// 놓는 순간 무적: 물리 복원 직후 바닥 낙하 접촉(Hit 이벤트)으로
	// 놓자마자 데미지 입는 것 방지
	if (UFurnitureDamage* DamageComp = Owner->FindComponentByClass<UFurnitureDamage>())
		DamageComp->SetInvincible(0.5f);

	if (FurnitureStat)
		FurnitureStat->UpdateGrabbedPlayers(GrabbedPlayers.Num());
}
float UFurnitureGrabSystem::ComputeCarrySpeed() const
{
	if (!FurnitureStat)
		return 0.0f;

	const float Base     = FurnitureStat->GetBaseSpeed();
	const int32 Required = FurnitureStat->GetRequiredPlayer();
	const int32 Num      = GrabbedPlayers.Num();

	// 인원 충족(초과 포함) → 기본속도 그대로
	if (Required <= 0 || Num >= Required)
		return Base;

	// 인원 미달 → 1인당 UnderMannedSpeedFactor(기본 1/5)씩만 반영해 극단적으로 감속. Base 상한.
	//   예) 필요3인: 1명 → 1×0.2 = 1/5, 2명 → 2×0.2 = 2/5 (요구사항과 일치)
	return Base * FMath::Min(1.0f, (float)Num * UnderMannedSpeedFactor);
}

void UFurnitureGrabSystem::ApplyBodyYaw(ACharacter* P, float DesiredYaw, float DeltaTime) const
{
	if (!P) return;
	const float CurYaw = P->GetActorRotation().Yaw;

	float NewYaw;
	if (BodyYawInterpSpeed > 0.0f)
	{
		// 최단각 보간(래핑 안전): 현재 → 목표를 BodyYawInterpSpeed로 부드럽게 접근
		const float Delta = FMath::FindDeltaAngleDegrees(CurYaw, DesiredYaw);
		NewYaw = CurYaw + Delta * FMath::Clamp(DeltaTime * BodyYawInterpSpeed, 0.0f, 1.0f);
	}
	else
	{
		NewYaw = DesiredYaw;   // 보간 끔 → 즉시 스냅(기존 동작)
	}

	FRotator NewRot = P->GetActorRotation();
	NewRot.Yaw = NewYaw;
	P->SetActorRotation(NewRot);
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
	const FTransform PreOffsetTransform = Owner->GetActorTransform();

	// 가구 단독 이동 및 회전 적용
	// Pitch(기울이기)는 HandleMovement가 매 틱 현재값을 유지하므로 여기서 바꾸면 그대로 운반됨
	FVector NewLoc = OldLoc + LocationOffset;
	//FRotator NewRot = Owner->GetActorRotation();
	//NewRot.Yaw += YawOffset;
	//NewRot.Pitch += PitchOffset;
	//Owner->SetActorLocationAndRotation(NewLoc, NewRot, true);
	Owner->AddActorLocalRotation(FRotator(PitchOffset, YawOffset, 0.0f), true);
	Owner->AddActorLocalOffset(LocationOffset, true);

	// [벽 관통 방지] UE 의 스윕은 '이동'만 검사하고 회전에는 적용되지 않아, 긴 가구를
	// 벽 옆에서 돌리면 벽에 파묻힌다. 한번 파묻히면 이후 스윕이 관통 방향으로 풀리면서
	// 벽을 통과해 버리므로, 적용 결과가 정적 지오메트리(벽·바닥)와 겹치면 이번 오프셋을
	// 통째로 되돌린다. (플레이어·다른 가구와의 겹침은 허용 — 정적만 검사)
	if (FurnitureMesh && GetWorld())
	{
		FComponentQueryParams OverlapParams(SCENE_QUERY_STAT(FurnitureOffsetOverlap), Owner);
		for (ACharacter* P : GrabbedPlayers)
		{
			OverlapParams.AddIgnoredActor(P);
		}
		FCollisionObjectQueryParams StaticOnly(ECC_WorldStatic);
		TArray<FOverlapResult> Overlaps;
		// 테스트 포즈를 3uu 올려서 검사 — 바닥에 닿아 있는(접촉 수준) 가구가
		// 항상 '겹침'으로 오탐되어 회전이 전부 거부되는 것을 막는다.
		// 벽 관통은 측면 겹침이라 3uu 상승과 무관하게 그대로 검출된다.
		const FVector OverlapTestLoc = FurnitureMesh->GetComponentLocation() + FVector(0.f, 0.f, 3.f);
		const bool bPenetratesStatic = GetWorld()->ComponentOverlapMulti(
			Overlaps, FurnitureMesh,
			OverlapTestLoc, FurnitureMesh->GetComponentQuat(),
			OverlapParams, StaticOnly);
		if (bPenetratesStatic)
		{
			Owner->SetActorTransform(PreOffsetTransform, false, nullptr, ETeleportType::TeleportPhysics);
			return; // 변화 없음 — 앵커 갱신·클라 통지 생략
		}
	}

	// 위치 이동
	//Owner->SetActorLocation(NewLoc, true);
	//
	//// 짐벌락 방지를 위해 AddActorRotation 사용
	//Owner->AddActorWorldRotation(FRotator(PitchOffset, YawOffset, 0.0f), true);
	////Owner->AddActorLocalRotation(FRotator(PitchOffset, YawOffset, 0.0f), true);

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

	// [들것 회전] 수동 회전(키 입력)으로 가구 Yaw가 밀린 만큼 의도 누적치도 함께 이동
	// → 다음 틱 선 회전 계산이 수동 회전을 되돌리지 않는다
	if (bPairLineValid)
		PairLineTargetYaw = FRotator::NormalizeAxis(PairLineTargetYaw + ActualYawDelta);

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

	// ---- 0.5. [들것 회전] 2인 이상: 가구 목표 Yaw = '두 운반자를 잇는 선'의 회전 ----
	// 카메라를 돌려도 가구는 돌지 않고, 한 사람이 상대를 축으로 걸어 돌면 가구가 따라 돈다.
	// 매 틱 선 Yaw 변화량을 'A만 움직였을 때 / B만 움직였을 때'로 분해해, 능동(직접 걷는)
	// 플레이어의 기여만 회전 의도로 누적한다. 피동(견인) 이동까지 포함하면 회전→견인→
	// 선 회전→재회전의 폭주 피드백이 생기므로 제외. 3인 이상은 앞의 두 명이 기준선.
	const bool bPairLine = bPairLineRotation && N >= 2;
	if (bPairLine)
	{
		ACharacter*  A    = Players[0];
		ACharacter*  B    = Players[1];
		const FVector PosA = A->GetActorLocation();
		const FVector PosB = B->GetActorLocation();

		if (!bPairLineValid || PairLineA != A || PairLineB != B)
		{
			// 첫 진입 또는 페어 구성 변경(그랩/해제) → 현재 가구 Yaw 기준 재설정 (스냅 없음)
			bPairLineValid    = true;
			PairLineA         = A;
			PairLineB         = B;
			PairLineTargetYaw = CurFurnYaw;
		}
		else if (FVector::DistSquared2D(PosA, PosB) >= FMath::Square(PairLineMinDistance))
		{
			auto LineYaw = [](const FVector& From, const FVector& To)
			{
				return FMath::RadiansToDegrees(FMath::Atan2(To.Y - From.Y, To.X - From.X));
			};
			const float PrevYaw    = LineYaw(PairLinePrevPosA, PairLinePrevPosB);
			const float DeltaFromA = FMath::FindDeltaAngleDegrees(PrevYaw, LineYaw(PosA, PairLinePrevPosB));
			const float DeltaFromB = FMath::FindDeltaAngleDegrees(PrevYaw, LineYaw(PairLinePrevPosA, PosB));

			float IntentDelta = 0.0f;
			if (!DraggedLastTick.Contains(A) && !StoppedDraggingLastTick.Contains(A))
				IntentDelta += DeltaFromA;
			if (!DraggedLastTick.Contains(B) && !StoppedDraggingLastTick.Contains(B))
				IntentDelta += DeltaFromB;

			PairLineTargetYaw = FRotator::NormalizeAxis(PairLineTargetYaw + IntentDelta);
			// 윈드업 방지: 실제 가구 Yaw보다 45° 이상 앞서 누적하지 않는다
			// (빠르게 빙글 돈 뒤 가구 혼자 한참 도는 현상 차단)
			const float Lead = FMath::FindDeltaAngleDegrees(CurFurnYaw, PairLineTargetYaw);
			PairLineTargetYaw = FRotator::NormalizeAxis(CurFurnYaw + FMath::Clamp(Lead, -45.0f, 45.0f));
		}
		// 두 사람이 너무 가까우면 선 방향이 불안정 → 이번 틱은 의도 누적 없이 유지

		PairLinePrevPosA = PosA;
		PairLinePrevPosB = PosB;
	}
	else
	{
		bPairLineValid = false;
	}

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
		// [들것 회전] 2인 이상이면 카메라 델타 대신 선 회전 누적치 기준 → 전원의 ProposalYaw가
		// PairLineTargetYaw로 일치(일치도 1)해 아래 원형 평균·교착 파이프라인을 그대로 통과한다.
		const float        PlayerYawDelta = bPairLine
			? FMath::FindDeltaAngleDegrees(Anc.InitialFurnitureYaw, PairLineTargetYaw)
			: FMath::FindDeltaAngleDegrees(Anc.InitialAimYaw, PlayerAimYaw);
		const float        ProposalYaw   = Anc.InitialFurnitureYaw + PlayerYawDelta;
		const FVector      ProposalLoc   = P->GetActorLocation() + Anc.InitialOffset.RotateAngleAxis(PlayerYawDelta, FVector::UpVector);
		float              Demand        = FVector(ProposalLoc.X - CurFurnLoc.X, ProposalLoc.Y - CurFurnLoc.Y, 0.0f).Size();
		// [견인 데드존 보완] 데드존 안에서 '서 있는' 플레이어는 낡은 앵커 제안이 가구를
		// 뒤로 당기는 브레이크가 됨 — 가구가 멈췄다가 상대가 견인으로 전환되는 순간 점프.
		// 자기 이동량 이상의 발언권을 주지 않는다: 서 있으면 최소 가중치, 걸으면 즉시 회복.
		Demand = FMath::Min(Demand, P->GetVelocity().Size2D() * DeltaTime * 4.0f + 1.0f);
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
	// [회전 교착 판정] 제안 방향들의 합 벡터 크기 R로 "의견 일치도"를 측정.
	//  - 같은 방향이면 R ≈ WTotal(1.0), 정반대면 R ≈ 0.
	//  - R이 거의 0인데 Atan2를 쓰면 방향이 정의되지 않아(Atan2(0,0)=0) 가구 Yaw가 엉뚱한 값으로
	//    붕괴 → 몸통이 가구 Yaw에 종속이라 캐릭터 시선이 수직/반대로 틀어지는 버그의 원인이었음.
	//  - 의견이 크게 갈리면(줄다리기) 회전하지 않고 현재 Yaw 유지. 경계 팔락임 방지용 히스테리시스.
	const float AgreementRatio = (WTotal > 0.0)
		? (float)(FMath::Sqrt(WSumSin * WSumSin + WSumCos * WSumCos) / WTotal)
		: 1.0f;
	if (bYawStalemate)
	{
		if (AgreementRatio > YawStalemateExitRatio)   { bYawStalemate = false; }
	}
	else
	{
		if (AgreementRatio < YawStalemateEnterRatio)  { bYawStalemate = true; }
	}

	// [운반자 막힘 → 회전 보류] 지난 틱 벽에 낀 운반자가 있었으면(Step 4 감지) 회전 정지.
	// 낀 사람을 두고 가구만 돌면: 낀 사람만 견인/도달 앵커 재기록이 반복 → 두 사람의 기준 시점이
	// 어긋남 → 제안 방향 불일치 → 원형 평균이 엉뚱한 곳을 가리킴 → 제어불능. 회전을 멈추면 차단됨.
	// FixedTurn: 한 틱에 FurnYawRotationSpeed*DT 이상 회전 불가 → 빠른 카메라 회전 시 가구 튐 방지
	const float TargetYawRaw = (bYawStalemate || bCarrierBlockedLastTick)
		? CurFurnYaw
		: FMath::RadiansToDegrees(FMath::Atan2(WSumSin, WSumCos));
	const float TargetYaw    = FMath::FixedTurn(CurFurnYaw, TargetYawRaw, FurnYawRotationSpeed * DeltaTime);

	// [들것 회전] 회전 보류(교착/벽 막힘) 동안 의도 누적 금지 — 해제 순간 홱 도는 것 방지
	if (bPairLineValid && (bYawStalemate || bCarrierBlockedLastTick))
		PairLineTargetYaw = CurFurnYaw;

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

	// ---- 2.5. 카메라 상하(Pitch) → 가구 높이 오프셋 ----
	// 그랩 시점 카메라 대비 위/아래로 본 각도만큼 가구를 올리고 내림. 전원 평균(둘 다 위 봐야 최대).
	// 범위 제약은 여기서 하지 않음 → 아래 '플레이어 기준' 제약에서 처리.
	float TargetHeightOffset = 0.0f;
	float PairHandHeight0 = 0.0f;   // 들것 기울기용: 페어 각자의 '손 높이' (평균은 높이, 차이는 기울기)
	float PairHandHeight1 = 0.0f;
	if (FurnitureHeightPerPitch != 0.0f)
	{
		float HSum = 0.0f;
		for (int32 i = 0; i < N; ++i)
		{
			const FGrabAnchor& Anc = Anchors[Players[i]];
			const float Hi = FMath::FindDeltaAngleDegrees(Anc.InitialAimPitch, Players[i]->GetBaseAimRotation().Pitch) * FurnitureHeightPerPitch;
			HSum += Hi;
			if (i == 0)      { PairHandHeight0 = Hi; }
			else if (i == 1) { PairHandHeight1 = Hi; }
		}
		TargetHeightOffset = HSum / (float)N;
	}

	// [인원 미달 드래그 연출] 2인 가구를 혼자 들면 잡은 쪽만 들리고 반대쪽 끝이 바닥에
	// 끌리는 자세가 되도록 — 기울기(Step 3)와 함께 중심을 낮춰 먼 쪽 끝을 바닥에 붙인다.
	// '이건 혼자 못 드는 가구'라는 걸 시각적으로 즉시 전달. 인원 충족 시 보간으로 수평 복귀.
	const int32 RequiredPlayers = FurnitureStat->GetRequiredPlayer();
	const bool  bUnderManned    = (N == 1) && (RequiredPlayers >= 2);
	float   UnderMannedTilt = 0.0f;
	FVector UnderMannedDir  = FVector::ZeroVector;
	if (bUnderManned)
	{
		UnderMannedDir = FVector(CurFurnLoc.X - Players[0]->GetActorLocation().X,
		                         CurFurnLoc.Y - Players[0]->GetActorLocation().Y, 0.0f);
		if (UnderMannedDir.Normalize())
		{
			UnderMannedTilt = 14.0f;
			// '기울어진 가구의 바닥(월드 AABB 하단)이 발밑 바닥에 닿는' 중심 높이를 역산.
			// Bounds.BoxExtent는 회전이 반영된 월드 AABB라 기울기 성분이 이미 포함 —
			// 여기에 sin(T)·길이를 또 더하면 이중 가산되어 그만큼 공중에 뜬다(버그 이력).
			// 이 동안은 카메라 피치 높이 조절도 무시됨: 혼자서는 못 들어올린다는 표현.
			const FVector Ext = FurnitureMesh ? FurnitureMesh->Bounds.BoxExtent : FVector(50.0f);
			float FloorZ = Players[0]->GetActorLocation().Z - 90.0f;
			if (const UCapsuleComponent* Cap = Players[0]->GetCapsuleComponent())
			{
				FloorZ = Players[0]->GetActorLocation().Z - Cap->GetScaledCapsuleHalfHeight();
			}
			const float CenterOffZ = FurnitureMesh ? (FurnitureMesh->Bounds.Origin.Z - Owner->GetActorLocation().Z) : 0.0f;
			// +1: 관통 없이 '닿아 보이는' 높이. 음수 겹침을 주면 회전(기울기)은 스윕이
			// 안 되어 모서리가 바닥을 파고들고, 그 자세로 놓으면 박힌 채 시작함.
			const float DesiredCenterZ = FloorZ + Ext.Z + 1.0f;
			TargetHeightOffset = DesiredCenterZ - (TargetLoc.Z + CenterOffZ);
		}
	}

	// 현재 높이 오프셋에서 목표 높이 오프셋으로 부드럽게 보간
	CurrentHeightOffset = FMath::FInterpTo(CurrentHeightOffset, TargetHeightOffset, DeltaTime, FurnitureHeightInterpSpeed);

	// [범위 제약: 플레이어 기준] 가구 높이를 '들고 있는 플레이어들의 평균 위치' 대비 [Min, Max]로 제한.
	// 경사에서 두 사람 높이가 다르면 그 평균에 맞춰 허용 범위(고저)가 함께 오르내림.
	{
		float AvgPlayerZ = 0.0f;
		for (int32 i = 0; i < N; ++i)
			AvgPlayerZ += Players[i]->GetActorLocation().Z;
		AvgPlayerZ /= (float)N;

		// [피벗 오프셋 보정] 제약을 피벗이 아니라 '메쉬 중심' 기준으로 → 피벗이 메쉬와 떨어진 가구도
		// 시각 위치가 범위 안에 맞음(천장/바닥 관통 방지, 위치 일관).
		const float MeshCenterOffZ = FurnitureMesh ? (FurnitureMesh->Bounds.Origin.Z - Owner->GetActorLocation().Z) : 0.0f;
		const float DesiredCenterZ = TargetLoc.Z + CurrentHeightOffset + MeshCenterOffZ;
		// 인원 미달 드래그 자세는 바닥 접지까지 내려가야 하므로 하한을 크게 완화
		const float MinZ = AvgPlayerZ + FurnitureHeightMin - (bUnderManned ? 250.0f : 0.0f);
		const float ClampedCenterZ = FMath::Clamp(DesiredCenterZ, MinZ, AvgPlayerZ + FurnitureHeightMax);
		// 윈드업 방지: 범위 밖 입력이 계속 쌓이지 않도록, 실제 적용 가능한 오프셋으로 되돌려 저장
		CurrentHeightOffset = (ClampedCenterZ - MeshCenterOffZ) - TargetLoc.Z;
	}

	// ---- 3. 가구 이동 (sweep=true, 가구 자체 충돌) ----
	// Pitch/Roll은 현재 값 유지: 물리로 쓰러진 가구는 그 자세 그대로 운반 (억지로 세우면 바닥 파고듦)
	FRotator TargetRot = Owner->GetActorRotation();
	TargetRot.Yaw = TargetYaw;

	// [들것 기울기] 공동운반 시 두 운반자의 손 높이(카메라 피치) '차이'를 기울기로 변환 —
	// 평균은 위 2.5의 높이 조절 그대로, 차이는 두 사람을 잇는 선을 따라 낮은 쪽으로 기움.
	// 1인 운반·놓기는 이 분기를 안 타므로 기존(자세 보존) 동작 유지.
	if (bPairLine && N >= 2 && FurnitureHeightPerPitch != 0.0f)
	{
		const FVector PosA = Players[0]->GetActorLocation();
		const FVector PosB = Players[1]->GetActorLocation();
		FVector LineDir(PosB.X - PosA.X, PosB.Y - PosA.Y, 0.0f);
		const float PairDist = LineDir.Size();
		if (PairDist > PairLineMinDistance)
		{
			LineDir /= PairDist;
			// B쪽 손이 높으면 B쪽 끝이 올라가는 회전. 과도한 기울기는 ±20°로 제한.
			const float TiltDeg = FMath::Clamp(FMath::RadiansToDegrees(
				FMath::Atan2(PairHandHeight1 - PairHandHeight0, PairDist)), -20.0f, 20.0f);
			const FVector TiltAxis = FVector::CrossProduct(LineDir, FVector::UpVector);
			const FQuat GoalQuat = FQuat(TiltAxis, FMath::DegreesToRadians(TiltDeg))
			                     * FQuat(FRotator(0.0f, TargetYaw, 0.0f));
			// 피치/롤은 보간으로 부드럽게 접근(합류 순간 스냅 방지), Yaw는 기존 FixedTurn 결과 유지
			TargetRot = FMath::RInterpTo(Owner->GetActorRotation(), GoalQuat.Rotator(),
			                             DeltaTime, FurnitureHeightInterpSpeed);
			TargetRot.Yaw = TargetYaw;
		}
	}
	// [인원 미달 드래그 연출] 잡은 쪽만 들리고 먼 쪽 끝이 바닥으로 기우는 자세 (위 2.5의 중심 낮춤과 세트)
	else if (bUnderManned && UnderMannedTilt > 0.0f)
	{
		const FVector TiltAxis = FVector::CrossProduct(UnderMannedDir, FVector::UpVector);
		// Cross(D,Up) 축의 +θ는 D쪽(먼 쪽) 끝을 올리므로, 내리려면 -θ
		const FQuat GoalQuat = FQuat(TiltAxis, FMath::DegreesToRadians(-UnderMannedTilt))
		                     * FQuat(FRotator(0.0f, TargetYaw, 0.0f));
		TargetRot = FMath::RInterpTo(Owner->GetActorRotation(), GoalQuat.Rotator(),
		                             DeltaTime, FurnitureHeightInterpSpeed);
		TargetRot.Yaw = TargetYaw;
	}

	// HeightOffset 대신 보간된 CurrentHeightOffset 적용
	Owner->SetActorLocationAndRotation(TargetLoc + FVector(0.0f, 0.0f, CurrentHeightOffset), TargetRot, true);

	// sweep 이동 도중 발생한 물리/데미지 이벤트 처리 로직 (기존 코드 유지)
	for (int32 i = Players.Num() - 1; i >= 0; --i)
	{
		if (!Anchors.Contains(Players[i]))
		{
			Players.RemoveAt(i);
		}
	}
	if (Players.Num() == 0)
		return;

	// 앵커 계산용 자연 좌표 추출 시에도 보간된 CurrentHeightOffset 제거
	FVector ActualLoc = Owner->GetActorLocation() - FVector(0.0f, 0.0f, CurrentHeightOffset);
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
			// [들것 회전] 막힌 만큼 의도도 실제 Yaw로 재기준 (앵커 리셋과 같은 이유의 윈드업 방지)
			if (bPairLineValid)
				PairLineTargetYaw = ActualYaw;

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
	// TODO(높이 이탈 자동해제): 낭떠러지 낙하 시 높이차 기준 해제가 필요하지만,
	// 강제 해제 시 UGrabComponent::GrabbedActor가 정리되지 않아 원거리 재그랩 버그 유발.
	// GrabComponent(캐릭터 담당) 수정 후 재도입 예정.
	TArray<ACharacter*> ToRelease;
	const float MaxSepSq = FMath::Square(MaxGrabSeparationDistance);
	for (ACharacter* P : Players)
	{
		const FVector D = FVector(P->GetActorLocation() - GetAttachedLocation(P, ActualLoc, ActualYaw));
		if (FVector(D.X, D.Y, 0.0f).SizeSquared() > MaxSepSq)
			ToRelease.Add(P);
	}

	// ---- 4. 벽 막힘 감지 → 가구 후퇴 (플레이어 직접 이동 없음, CMC 충돌 없음) ----
	bCarrierBlockedLastTick = false;   // 이번 틱 감지 결과로 갱신 (아래에서 막히면 true)
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

			// [턱 오탐 방지] CMC가 걸어서 오를 수 있는 턱(MaxStepHeight 이하)은 벽으로 치지 않는다.
			// 발바닥 높이 그대로 수평 스윕하면 문턱·낮은 단차에 막힘 판정 → 가구 후퇴 ↔ 견인이
			// 반복되며 서버·클라 위치가 어긋나 러버밴딩(디싱크 체감)이 생김. 캡슐 밑단을 스텝
			// 높이만큼 들어올려(반높이 축소 + 중심 상향, 머리 높이는 유지) 스윕하고,
			// 실제 등반은 CMC 스텝업이 알아서 처리하게 둔다.
			const UCharacterMovementComponent* PCMC = P->GetCharacterMovement();
			const float CapRadius     = Cap->GetScaledCapsuleRadius();
			const float CapHalfHeight = Cap->GetScaledCapsuleHalfHeight();
			const float StepH         = PCMC ? PCMC->MaxStepHeight : 45.0f;
			const float NewHalfHeight = FMath::Max(CapHalfHeight - StepH * 0.5f, CapRadius);
			const FVector LiftZ(0.0f, 0.0f, CapHalfHeight - NewHalfHeight);   // 밑단만 올라가도록 중심 상향

			FCollisionShape Shape = FCollisionShape::MakeCapsule(CapRadius, NewHalfHeight);
			FCollisionQueryParams QP;
			QP.AddIgnoredActor(Owner);
			for (ACharacter* Other : Players)  // 그랩 플레이어끼리 오탐 WorstBlock 방지
				QP.AddIgnoredActor(Other);

			FHitResult Hit;
			if (GetWorld()->SweepSingleByProfile(Hit, StartPos + LiftZ, EndPos + LiftZ, FQuat::Identity,
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
			// 실제 배치는 카메라 높이 오프셋 포함, 앵커용 ActualLoc은 자연 좌표 유지
			// (자연 좌표를 그대로 SetActorLocation하면 후퇴할 때마다 높이가 소실되는 버그)
			Owner->SetActorLocation(ActualLoc + FVector(0.0f, 0.0f, CurrentHeightOffset), false);
			ActualLoc = Owner->GetActorLocation() - FVector(0.0f, 0.0f, CurrentHeightOffset);

			// [상대좌표 보존] 운반자가 막힌 동안은 회전을 보류(다음 틱)한다.
			// 회전을 멈추면 가구가 낀 플레이어를 두고 돌지 않으므로 그랩 시점의 상대 위치·방향이
			// 그대로 유지됨. (앵커를 재기록하지 않는 것이 핵심 — 재기록하면 낀 사람의 틀어진
			//  위치가 새 기준으로 구워져 상대좌표가 소실되고 캐릭터·가구가 벌어진다.)
			bCarrierBlockedLastTick = true;
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
		const bool bWasDragged = DraggedLastTick.Contains(P);
		// [견인 데드존] 정착 상태에서는 PullStartRadius까지 자유 이동을 허용(견인 시작을 늦춤),
		// 일단 견인이 시작되면 CorrectionDeadzone까지 완전히 끌어 대형을 정확히 복원한다.
		// 해제 반경까지 넓히면 도달 앵커 재기록(아래)에 잔여 오차가 구워져, 견인이 반복될수록
		// 피동 플레이어가 대형에서 점점 뒤로 밀리는 누적 드리프트가 생긴다 — 진입만 넓게.
		const float AtTargetRadius = bWasDragged
			? CorrectionDeadzone
			: FMath::Max(PullStartRadius, CorrectionDeadzone);
		// [겹침 방지] 데드존 여유는 옆·뒤 방향까지만 — 앵커 자리에서 '가구 중심 방향'으로
		// 일정 이상 파고들면(운반자-가구 충돌은 그랩 중 꺼져 있어 몸이 가구를 관통해 보임)
		// 도달 판정을 깨고 견인을 발동시켜 대형을 복원한다. 견인 램프(0.12s) 덕에 부드럽게 밀려남.
		bool bIntrudesFurniture = false;
		{
			FVector DirToFurn(ActualLoc.X - Att.X, ActualLoc.Y - Att.Y, 0.0f);
			if (DirToFurn.Normalize())
			{
				const float TowardFurn = FVector::DotProduct(FVector(-Delta.X, -Delta.Y, 0.0f), DirToFurn);
				bIntrudesFurniture = TowardFurn > 12.0f;
			}
		}
		const bool bAtTarget   = !bIntrudesFurniture
			&& Delta.SizeSquared() <= FMath::Square(AtTargetRadius);

		if (bAtTarget && !bWasDragged)
		{
			// 능동 주도자: CMC '속도'는 간섭하지 않음 (자기 입력으로 걸음).
			// [발 미끄러짐 방지] 예전엔 여기서 Multicast_ApplyPlayerCorrection(ZeroVector)을 호출했는데,
			// 그 구현이 오너 CMC 속도를 매 틱 0으로 덮어써(→ 걷기 속도 0↔걷기 왕복) 발이 미끄러졌음(발발).
			// → 그 Multicast 제거. 속도를 안 건드리니 오너는 매끈하게 걸음.
			// [회전 복제] 몸통 Yaw는 서버가 ApplyBodyYaw로 세팅 → 서버 권위 회전이 다른 뷰어(호스트·타클라)에게
			// 복제되어 회전이 보임. 오너 자신은 로컬 동기화 블록(TickComponent)이 매끈하게 돌리므로,
			// 서버는 원격 허용오차(RemoteBodyYawTolerance) 안에선 덮어쓰지 않아 이중 기록 왕복을 최소화.
			const float DesiredYaw = GetDesiredYaw(P, ActualYaw);
			const float YawTol = P->IsLocallyControlled() ? YawCorrectionDeadzone : RemoteBodyYawTolerance;
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(P->GetActorRotation().Yaw, DesiredYaw)) > YawTol)
			{
				ApplyBodyYaw(P, DesiredYaw, DeltaTime);
			}
			continue;
		}

		// 여기 이후 = 피동·정지 플레이어 (능동 주도자는 위 블록에서 continue됨)
		// Yaw 관용치는 능동 블록과 동일한 이유로 원격/호스트 분리
		const float DesiredYaw = GetDesiredYaw(P, ActualYaw);
		const float YawTol = P->IsLocallyControlled() ? YawCorrectionDeadzone : RemoteBodyYawTolerance;
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(P->GetActorRotation().Yaw, DesiredYaw)) > YawTol)
		{
			ApplyBodyYaw(P, DesiredYaw, DeltaTime);   // 즉시 스냅 대신 보간
		}

		if (bAtTarget && bWasDragged)
		{
			// 피동 플레이어가 방금 목표에 도달 → XY 정지 (관성 슬라이딩 방지)
			// Z는 보존: 낙하 중이면 중력 속도를 지워선 안 됨 (공중 정지/슬로모 방지)
			// [예측 보호] 서버 직접 기록은 호스트(로컬 제어) 폰에만. 원격 프록시(클라 조종)의
			// CMC 속도를 예측 파이프라인 밖에서 덮어쓰면, 클라 move 재생 시작속도와 어긋나
			// ClientAdjustPosition(=텔레포트 체감)이 발사됨. 원격은 아래 Multicast가 소유
			// 클라에서 같은 값을 적용하고, 서버는 그 move를 재생하며 자연히 일치한다.
			if (P->IsLocallyControlled())
			{
				CMC->Velocity = FVector(0.0f, 0.0f, CMC->Velocity.Z);
			}
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

		// [견인 중 회전 기준점 추종] 상대(P1)의 회전으로 이 플레이어가 견인되는 동안,
		// 자기 카메라를 '안 움직이면' 회전 기준점을 현재 가구로 계속 재정렬해 회전 의도를 0으로 유지.
		// → 나중에 이 플레이어가 카메라를 돌리면 '그랩 시점'이 아니라 '현재 가구' 기준으로 제안됨
		//   (기준점이 그랩 시점에 머물러 제안이 크게 튀던 문제 해소).
		// 자기 카메라를 '움직이면' 재정렬을 건너뛰어 그 입력이 회전 의도로 살아남(주도권 인수).
		// 위치 상대좌표는 회전을 오프셋에 구워 보존(그랩 관계 유지). 몸통 기준은 현재값으로 연속.
		{
			FGrabAnchor& Anc   = Anchors[P];
			const float  CurAim = P->GetBaseAimRotation().Yaw;
			const float  AimMoved = FMath::Abs(FMath::FindDeltaAngleDegrees(Anc.PrevAimYaw, CurAim));
			if (AimMoved < 0.1f)   // 카메라 정지 = 순수 견인 → 기준점 현재로 추종(의도 0)
			{
				const float OldYC = FMath::FindDeltaAngleDegrees(Anc.InitialFurnitureYaw, ActualYaw);
				Anc.InitialOffset       = Anc.InitialOffset.RotateAngleAxis(OldYC, FVector::UpVector);
				Anc.InitialFurnitureYaw = ActualYaw;
				Anc.InitialAimYaw       = CurAim;
				// 몸통 Yaw 기준은 '현재 몸통 대입'이 아니라 회전량(OldYC)만큼 함께 이동시켜
				// GetDesiredYaw = InitPlayerYaw + (ActualYaw - InitFurnYaw) 결과를 재정렬 전후 '불변'으로 유지.
				// (현재 몸통을 대입하면 서버 계산값과 클라 로컬 동기화 값의 기준이 어긋나
				//  원격 클라에서 서버 회전 + 로컬 회전이 이중 적용 → 몸통이 2배로 돌아 뒤를 보게 됨)
				Anc.InitialPlayerYaw    = FRotator::NormalizeAxis(Anc.InitialPlayerYaw + OldYC);
			}
			Anc.PrevAimYaw = CurAim;
		}

		// !bAtTarget: 피동 → 목표를 향해 끌어당김
		// BrakingDecel 보상: CMC가 다음 틱 시작 시 BrakingDecel*DT 만큼 속도를 감쇠시키므로
		// 그만큼 더 주입해 실질 이동거리가 Delta와 일치하도록 함.
		// 서버·클라 모두 동일하게 BrakingDecel 감쇠 적용 → 동일 이동 → ClientAdjustPosition 없음.
		// DeltaTime=0(첫 틱/히치) 나눗셈 가드 — inf 속도 주입 방지
		const float SafeDeltaTime = FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);
		// [견인 램프] 벌어진 거리(데드존 50uu)를 한 틱에 닫으면 견인 시작 순간 수천 cm/s가
		// 주입돼 '멈췄다가 순간이동' 체감이 됨 → 0.12초에 걸쳐 지수적으로 닫는다.
		const float PullCloseTime = FMath::Max(SafeDeltaTime, 0.12f);
		const FVector NeededVelocity = (Delta / PullCloseTime).GetClampedToMaxSize(MaxCorrectionSpeed);
		const FVector CarryVelocity  = (NeededVelocity + NeededVelocity.GetSafeNormal() * CMC->BrakingDecelerationWalking * DeltaTime)
		                               .GetClampedToMaxSize(MaxCorrectionSpeed);
		// XY만 견인, Z는 보존: CarryVelocity.Z=0이라 통째로 대입하면 낙하 속도가 매 틱 0으로
		// 리셋되어 공중에서 슬로모션으로 떨어지는 현상 발생
		// [예측 보호] 위 도달 블록과 동일 — 원격 프록시는 Multicast 경로(소유 클라 적용)만 사용
		if (P->IsLocallyControlled())
		{
			CMC->Velocity = FVector(CarryVelocity.X, CarryVelocity.Y, CMC->Velocity.Z);
		}
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

	// 낡은(이전 시퀀스) 패킷만 무시하고 최신은 항상 수용한다. 기존의 '불일치=전부 버림'은
	// 수동 회전으로 시퀀스가 오른 뒤 Reliable(ApplySystemOffset)이 도착할 때까지 새 시퀀스가
	// 달린 트랜스폼 패킷을 통째로 버려, 보간 타깃이 정체됐다가 한 번에 튀는 히치를 만들었음.
	// (uint8 순환 대응: int8 차이의 부호로 앞뒤 판정)
	const int8 SeqDelta = (int8)(SeqID - LocalSystemOffsetSequence);
	if (SeqDelta < 0)
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
#if !UE_BUILD_SHIPPING
	if (!GEngine) return;
	GEngine->AddOnScreenDebugMessage(9000, 0.1f, FColor::Yellow,
		FString::Printf(TEXT("[가구] 실속도: %.0f  /  설정최대속도: %.0f"), FurnActualSpeed, FurnMaxSpeed));
	for (int32 i = 0; i < MaxWalkSpeeds.Num(); ++i)
	{
		GEngine->AddOnScreenDebugMessage(9001 + i, 0.1f, FColor::Cyan,
			FString::Printf(TEXT("  [P%d] MaxWalkSpeed: %.0f  /  현재속도: %.0f"),
				i + 1, MaxWalkSpeeds[i], ActualSpeeds[i]));
	}
#endif
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
	// Z는 로컬 값 보존 (낙하 속도 리셋 방지)
	if (UCharacterMovementComponent* CMC = Player->GetCharacterMovement())
		CMC->Velocity = FVector(CarryVelocity.X, CarryVelocity.Y, CMC->Velocity.Z);

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
			// [시야] 카메라 프로브(스프링암)는 각 클라이언트에서 돌므로,
			// 클라 동기화 경로에서도 운반 중 카메라 채널 무시를 적용한다.
			FurnitureMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
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
						// Z 속도 보존: 낙하 중 해제 시 공중 정지 방지
						CMC->Velocity                  = FVector(0.0f, 0.0f, CMC->Velocity.Z);
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
			// Multicast_SetPlayerAnchor(Reliable)가 먼저 도착해 '그랩 시점' 정확 앵커를
			// 만들어둔 경우, 여기서 '현재 트랜스폼' 기반 재구성 앵커로 덮어쓰면 가구가 움직이는 중
			// 합류한 두 번째 캐리어가 스냅한다 — 이미 있으면 유지.
			if (P->IsLocallyControlled() && GetOwner() && !Anchors.Contains(P))
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
		// OnRep는 인원이 바뀔 때마다 호출되므로 매번 이속을 재계산한다 (서버와 동일 규칙)
		if (FurnitureStat)
			CMC->MaxWalkSpeed = ComputeCarrySpeed();
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
