// Fill out your copyright notice in the Description page of Project Settings.

#include "FurnitureGrabSystem.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "CatchCharacter/Furniture/FurnitureStat.h"
#include "CatchCharacter/Furniture/FurnitureDamage.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "CatchCharacter/Furniture/FurnitureCarryShared.h"

DEFINE_LOG_CATEGORY(LogCarry);

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
	DOREPLIFETIME(UFurnitureGrabSystem, bMoveConstrained);
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
	if (!Owner || !Owner->HasAuthority() || !Grabber)
		return;
	// [잡기 진단] 조용한 거부 2종을 F9 동안 기록 — '눌러도 안 잡힘'의 서버측 원인 판별
	if (GrabbedPlayers.Contains(Grabber))
	{
		if (IsCarryDebugEnabled())
			UE_LOG(LogCarry, Warning, TEXT("[잡기] %s 거부: %s 이미 잡는 중(스테일 상태?)"),
				*Owner->GetName(), *Grabber->GetName());
		return;
	}
	if (FurnitureStat && GrabbedPlayers.Num() >= FurnitureStat->GetRequiredPlayer())
	{
		if (IsCarryDebugEnabled())
			UE_LOG(LogCarry, Warning, TEXT("[잡기] %s 거부: 정원 초과 (%d/%d)"),
				*Owner->GetName(), GrabbedPlayers.Num(), FurnitureStat->GetRequiredPlayer());
		return;
	}

	SetGrabCollisionState(Grabber, true);

	// 첫 번째 그랩: 물리 끄기 + 들어올리기 + 이동복제 단일화
	if (GrabbedPlayers.Num() == 0 && FurnitureMesh)
	{
		FurnitureMesh->SetSimulatePhysics(false);
		FurnitureMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		// [시야] 운반 중 가구가 스프링암 카메라 프로브를 밀어내 큰 가구를 들면 화면이
		// 가구 안으로 처박히는 문제 — 잡혀 있는 동안만 카메라 채널을 무시한다.
		// 놓을 때 PhysicsActor 프로파일 복원이 응답을 원상복구하므로 별도 처리 불필요.
		FurnitureMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

		// [스택 웨이크] 이 가구와 겹치거나 얹힌 시작-정적 가구에 물리를 켜서 길을 비키게 한다
		{
			FComponentQueryParams WakeParams(SCENE_QUERY_STAT(GrabWakeStacked), Owner);
			FCollisionObjectQueryParams DynObj(ECC_WorldDynamic);
			DynObj.AddObjectTypesToQuery(ECC_PhysicsBody);
			TArray<FOverlapResult> Stacked;
			const FBoxSphereBounds GrabBounds = FurnitureMesh->Bounds;
			GetWorld()->OverlapMultiByObjectType(Stacked, GrabBounds.Origin, FQuat::Identity,
				DynObj, FCollisionShape::MakeBox(GrabBounds.BoxExtent + FVector(4.0f)), WakeParams);
			for (const FOverlapResult& O : Stacked)
			{
				AActor* OtherA = O.GetActor();
				if (!OtherA || OtherA == Owner)
					continue;
				const UFurnitureGrabSystem* OtherGS = OtherA->FindComponentByClass<UFurnitureGrabSystem>();
				if (!OtherGS || OtherGS->GetGrabbedPlayers().Num() > 0)
					continue;   // 가구가 아니거나, 누가 들고 있는 것(물리 의도적 OFF)은 제외
				// 그랩 순간 밀려나며 서로 부딪히는 접촉은 내구도 대상이 아님 — 주변 가구 잠깐 무적
				if (UFurnitureDamage* OtherDmg = OtherA->FindComponentByClass<UFurnitureDamage>())
					OtherDmg->SetInvincible(2.0f);
				UStaticMeshComponent* OtherMesh = Cast<UStaticMeshComponent>(OtherA->GetRootComponent());
				if (OtherMesh && !OtherMesh->IsSimulatingPhysics())
				{
					OtherMesh->SetSimulatePhysics(true);
					// 배치-겹침 해소가 고속 발사가 되지 않게 — 정지 상태에서 시작 + 밀어내기 속도 제한
					OtherMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
					OtherMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
					if (FBodyInstance* BI = OtherMesh->GetBodyInstance())
						BI->SetMaxDepenetrationVelocity(120.0f);
					//UE_LOG(LogCarry, Log, TEXT("[스택 웨이크] %s (%s 그랩 시 겹침)"),
					//	*OtherA->GetName(), *Owner->GetName());
				}
			}
		}

		CurrentHeightOffset = 0.0f;

		// 초기 리프트 스냅 없음 — 들어올림은 높이 파이프라인(최소 운반 높이 + 보간)이 처리한다
		// (큰 가구에서 그랩 순간 텔레포트 체감의 원인이었음)
		Owner->SetReplicateMovement(false);
		ServerLocation = Owner->GetActorLocation();
		ServerRotation = Owner->GetActorRotation();
	}

	GrabbedPlayers.Add(Grabber);

	// [피동 플래그 초기화] 인원 구성이 바뀌면 이전 대형 기준의 피동/도달 추적을 리셋한다
	DraggedLastTick.Empty();
	StoppedDraggingLastTick.Empty();

	// 잡은 순간 기준값 기록
	{
		FGrabAnchor Anchor;
		Anchor.InitialFurnitureYaw = Owner->GetActorRotation().Yaw;
		// 몸통·정렬 기준은 '카메라 정면' — 잡으면 몸이 카메라 방향을 보고(보간),
		// 가구는 아래 정면 정렬로 화면 정면 중앙에 온다. 이후 카메라 회전 = 가구·몸 회전.
		Anchor.InitialPlayerYaw = FRotator::NormalizeAxis(Grabber->GetBaseAimRotation().Yaw);
		// [자연 좌표 기준] 앵커는 잡은 순간의 실제 상대 위치로 기록한다 — 정면 정렬은
		// 0.8 정면 복원이 매 틱 보간으로 데려가므로 여기서 스냅하지 않는다 (큰 가구 텔레포트 방지)
		Anchor.InitialOffset = (Owner->GetActorLocation() - FVector(0.f, 0.f, CurrentHeightOffset))
		                     - Grabber->GetActorLocation();
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

	if (UCharacterMovementComponent* CMC = Grabber->GetCharacterMovement())
	{
		if (!OriginalMaxWalkSpeeds.Contains(Grabber))
			OriginalMaxWalkSpeeds.Add(Grabber, CMC->MaxWalkSpeed);
		CMC->bOrientRotationToMovement = false;
	}

	// 그랩 순간 무적: 잡는 과정의 스윕/물리 접촉과 주변 가구 웨이크의 밀어내기 접촉이
	// 정착할 때까지 데미지 면제 (웨이크된 주변 가구의 무적 2초와 맞춤)
	if (UFurnitureDamage* DamageComp = Owner->FindComponentByClass<UFurnitureDamage>())
		DamageComp->SetInvincible(2.0f);

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

	//UE_LOG(LogCarry, Log, TEXT("[그랩] %s ← %s (N=%d, 필요=%d)"),
	//	*Owner->GetName(), *Grabber->GetName(), GrabbedPlayers.Num(),
	//	FurnitureStat ? FurnitureStat->GetRequiredPlayer() : -1);
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

	// [남은 운반자 재앵커] 한 명이 놓는 순간 현재 가구 상태를 남은 운반자들의 새 기준으로 재기록한다
	if (GrabbedPlayers.Num() > 0)
	{
		const FVector NaturalLoc = Owner->GetActorLocation() - FVector(0.0f, 0.0f, CurrentHeightOffset);
		const float   CurYaw     = Owner->GetActorRotation().Yaw;
		for (ACharacter* P : GrabbedPlayers)
		{
			if (FGrabAnchor* Anc = Anchors.Find(P))
			{
				Anc->InitialOffset       = NaturalLoc - P->GetActorLocation();
				// 몸통 기준은 델타 시프트로 연속 보존 (재기록 전후 몸통 목표 불변)
				Anc->InitialPlayerYaw    = FRotator::NormalizeAxis(Anc->InitialPlayerYaw
					+ FMath::FindDeltaAngleDegrees(Anc->InitialFurnitureYaw, CurYaw));
				Anc->InitialFurnitureYaw = CurYaw;
				Anc->InitialAimYaw       = P->GetBaseAimRotation().Yaw;
				Multicast_SetPlayerAnchor(P, CurYaw, Anc->InitialPlayerYaw,
					Anc->InitialAimYaw, Anc->InitialOffset);
			}
		}
	}

	if (GrabbedPlayers.Num() == 0 && FurnitureMesh)
	{
		// [관통 복구] 인원미달 끌림 자세는 바닥에 닿아 있고, 회전은 스윕 보정이 없어
		// 모서리가 지면에 박혀 있을 수 있음 → 물리 켜기 전에 겹침이 풀릴 때까지 들어올린다.
		// (박힌 채 물리를 켜면 그대로 잠기거나 튕겨나감)
		// 우선 무조건 +3: 겹침 검사에 안 걸리는 얕은 관통까지 소량 들어올린 뒤 중력으로 안착시킨다
		Owner->AddActorWorldOffset(FVector(0.0f, 0.0f, 3.0f), false);
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

		// CCD 활성화: 낙하 시 가는 다리가 얇은 바닥을 터널링해 박히는 것을 막는다
		FurnitureMesh->BodyInstance.bUseCCD = true;
		FurnitureMesh->SetSimulatePhysics(true);
		FurnitureMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		Owner->SetReplicateMovement(true);

		bYawStalemate = false;            // 다음 그랩에 교착 상태 누출 방지
		bCarrierBlockedLastTick = false;  // 다음 그랩에 막힘 상태 누출 방지
		bMoveConstrained = false;         // 다음 그랩에 리쉬 동결 상태 누출 방지
	}

	// 놓는 순간 무적: 물리 복원 직후 바닥 낙하 접촉(Hit 이벤트)으로
	// 놓자마자 데미지 입는 것 방지
	if (UFurnitureDamage* DamageComp = Owner->FindComponentByClass<UFurnitureDamage>())
		DamageComp->SetInvincible(0.5f);

	if (FurnitureStat)
		FurnitureStat->UpdateGrabbedPlayers(GrabbedPlayers.Num());

	//UE_LOG(LogCarry, Log, TEXT("[해제] %s ← %s (남은 N=%d)"),
	//	*Owner->GetName(), *Grabber->GetName(), GrabbedPlayers.Num());
}
float UFurnitureGrabSystem::ComputeCarrySpeed() const
{
	if (!FurnitureStat)
		return 0.0f;

	const float Base     = FurnitureStat->GetBaseSpeed();
	const int32 Required = FurnitureStat->GetRequiredPlayer();
	const int32 Num      = GrabbedPlayers.Num();

	// 1인 가구를 들면 달리기 허용
	if (Required == 1 && Num == 1)
	{
		if (ACharacter* Player = GrabbedPlayers[0])
		{
			if (UCharacterMovementComponent* CMC = Player->GetCharacterMovement())
			{
				// 가구의 기본 속도(Base)와 플레이어의 현재 속도(걷기 250, 달리기 500) 중 높은 값을 반환
				return FMath::Max(Base, CMC->MaxWalkSpeed);
			}
		}
	}

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
	// 1인 카메라 직결은 목표가 즉각 움직이므로 몸은 더 천천히 쫓고, 회전 속도에 상한을
	// 둔다 — 지수 보간만 쓰면 큰 각도에서 초반 속도가 치솟아 몸이 홱 돌고, 가구 추격이
	// 못 따라와 벌어진다. 상한 180°/s = 가구 회전 상한과 동속 (몸·가구가 같이 돎)
	const bool  bSolo       = GrabbedPlayers.Num() == 1;
	const float InterpSpeed = bSolo ? BodyYawInterpSpeed * 0.4f : BodyYawInterpSpeed;
	if (InterpSpeed > 0.0f)
	{
		// 최단각 보간(래핑 안전): 현재 → 목표를 부드럽게 접근
		const float Delta = FMath::FindDeltaAngleDegrees(CurYaw, DesiredYaw);
		float Step = Delta * FMath::Clamp(DeltaTime * InterpSpeed, 0.0f, 1.0f);
		if (bSolo)
		{
			const float MaxStep = 180.0f * DeltaTime;
			Step = FMath::Clamp(Step, -MaxStep, MaxStep);
		}
		NewYaw = CurYaw + Step;
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
	// 몸통 목표 기준: 인원 충족 1인은 매 틱 카메라 정면 — 앵커 경유는 벽 회전막힘·의도
	// 리셋이 쌓이면 카메라와 몸의 관계가 영구히 틀어진다. 끌기(미달)·2인+ 들것은 앵커 방식.
	const bool bSoloFull = GrabbedPlayers.Num() == 1
		&& (!FurnitureStat || FurnitureStat->GetRequiredPlayer() <= 1);
	float Desired = (bSoloFull && Player)
		? FRotator::NormalizeAxis(Player->GetBaseAimRotation().Yaw)
		: A.InitialPlayerYaw + FMath::FindDeltaAngleDegrees(A.InitialFurnitureYaw, FurnitureYaw);

	// [가구 바라봄 보정] 가구가 정면에서 10° 넘게 벗어난 초과분만 가구 쪽으로 굽힌다 (최대 45°).
	// 몸통 Yaw는 운반 물리에 안 쓰이는 코스메틱, 무상태 계산이라 서버·소유 클라 결과 동일.
	if (FurnitureMesh && Player)
	{
		const FVector ToFurn = FurnitureMesh->Bounds.Origin - Player->GetActorLocation();
		if (ToFurn.SizeSquared2D() > FMath::Square(30.0f))   // 30uu 미만은 방향 노이즈 가드
		{
			const float FurnDirYaw = FMath::RadiansToDegrees(FMath::Atan2(ToFurn.Y, ToFurn.X));
			const float Miss   = FMath::FindDeltaAngleDegrees(Desired, FurnDirYaw);
			// 가구가 거의 뒤(120°+)면 굽힘을 0으로 페이드 — 몸을 획 꺾지 않고 정면 복원이 데려오게 둔다
			const float AbsMiss = FMath::Abs(Miss);
			const float Fade    = FMath::Clamp((130.0f - AbsMiss) / 40.0f, 0.0f, 1.0f);
			const float Excess  = FMath::Min(FMath::Max(AbsMiss - 10.0f, 0.0f), 45.0f) * Fade;
			if (Excess > 0.0f)
			{
				Desired = FRotator::NormalizeAxis(Desired + FMath::Sign(Miss) * Excess);
			}
		}
	}
	return Desired;
}

bool UFurnitureGrabSystem::GetCarryLeash(ACharacter* Player, FVector& OutAttach, float& OutRadius) const
{
	AActor* Owner = GetOwner();
	if (!Owner || !Player || !GrabbedPlayers.Contains(Player) || !Anchors.Contains(Player))
		return false;

	// 클라는 보간된 액터 위치가 서버보다 뒤처져(이동 중) 부착점이 뒤로 밀리고, 그 어긋난
	// 기준으로 전진 입력이 깎여 사선 이동이 된다 — 복제된 서버 트랜스폼 기준으로 계산
	const bool    bAuth   = Owner->HasAuthority();
	const FVector FurnLoc = bAuth ? Owner->GetActorLocation() : ServerLocation;
	const float   FurnYaw = bAuth ? Owner->GetActorRotation().Yaw : ServerRotation.Yaw;
	OutAttach = GetAttachedLocation(Player, FurnLoc - FVector(0.0f, 0.0f, CurrentHeightOffset), FurnYaw);
	// 반경은 운반 보행의 평형 간극(가구 추격 지연 + 가구측 대칭 클램프 48)보다 넓게 —
	// 좁으면 소유 클라 필터가 정상 전진을 벽처럼 깎는다. 부착점을 서버 트랜스폼+동기
	// 오프셋으로 계산해 서버·클라 측정이 일치하므로 반경도 동일하게 둔다.
	// (-25: 줄다리기 벌어짐 완화 튜닝 — 다음 풀빌드 때 LeashRadius 기본값 정리 예정)
	OutRadius = FMath::Max(LeashRadius - 25.0f, 20.0f) + 8.0f;
	// 이동 봉인 중엔 반경을 현재 거리로 동결 — 어느 방향으로도 더 벌어질 수 없다
	if (bMoveConstrained)
	{
		const float Dist = FVector::Dist2D(Player->GetActorLocation(), OutAttach);
		OutRadius = FMath::Min(OutRadius, Dist + 2.0f);
	}
	return true;
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

	if (IsCarryDebugEnabled())
	{
		// 소유 클라 화면용: 주입 중인 견인 속도(파랑 화살표) / 정지 주입(빨강 점) 표시
		const FVector L = Player->GetActorLocation() + FVector(0, 0, 30);
		if (!CarryVelocity.IsNearlyZero(1.0f))
			DrawDebugDirectionalArrow(Player->GetWorld(), L, L + CarryVelocity * 0.25f, 25.0f, FColor::Cyan, false, 0.1f, 0, 3.0f);
		else
			DrawDebugSphere(Player->GetWorld(), L, 8.0f, 8, FColor::Red, false, 0.1f, 0, 2.0f);
	}

	// 몸통 Yaw는 여기서 스냅하지 않는다 — 목표가 급변하는 순간(적재존 진입 등) 몸이 휙
	// 돌아가는 원인이었고, 소유 클라 몸통은 TickComponent의 로컬 동기화가 보간으로 처리한다.
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
