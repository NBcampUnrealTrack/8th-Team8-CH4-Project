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
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

// 운반 이벤트 로그 카테고리 — Output Log에 항상 기록 ("LogCarry"로 검색)
DEFINE_LOG_CATEGORY_STATIC(LogCarry, Log, All);

namespace
{
	// F9(TC.GrabDebug) 운반 시각화 게이트 — CVar는 TeamCarry 모듈이 등록하므로 조회만 한다 (null이면 재시도)
	bool IsCarryDebugEnabled()
	{
		static IConsoleVariable* CachedCVar = nullptr;
		if (!CachedCVar)
		{
			CachedCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("TC.GrabDebug"));
		}
		return CachedCVar && CachedCVar->GetInt() != 0;
	}

	// 조준 피치를 [-90,90]으로 접는다 — 원격 피치(RemoteViewPitch)는 ±90 밖 랩 값이 올 수 있음
	float FoldAimPitch(float PitchDeg)
	{
		PitchDeg = FRotator::NormalizeAxis(PitchDeg);
		if (PitchDeg > 90.0f)       PitchDeg = 180.0f - PitchDeg;
		else if (PitchDeg < -90.0f) PitchDeg = -180.0f - PitchDeg;
		return PitchDeg;
	}
}

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
				UStaticMeshComponent* OtherMesh = Cast<UStaticMeshComponent>(OtherA->GetRootComponent());
				if (OtherMesh && !OtherMesh->IsSimulatingPhysics())
				{
					OtherMesh->SetSimulatePhysics(true);
					//UE_LOG(LogCarry, Log, TEXT("[스택 웨이크] %s (%s 그랩 시 겹침)"),
					//	*OtherA->GetName(), *Owner->GetName());
				}
			}
		}

		CurrentHeightOffset = 0.0f;

		// [끌기 가구는 초기 리프트 생략] 2인 가구 솔로 그랩은 틱에서 끌림 자세(바닥 접지)로 가므로 들어올리지 않는다
		const bool bWillDrag = FurnitureStat && FurnitureStat->GetRequiredPlayer() >= 2;
		if (!bWillDrag)
		{
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
		}
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
		// [자연 좌표 기준] 앵커는 높이 오프셋을 뺀 자연 좌표로 기록한다 (HandleMovement가 오프셋을 다시 더함)
		Anchor.InitialOffset       = (Owner->GetActorLocation() - FVector(0.f, 0.f, CurrentHeightOffset))
		                           - Grabber->GetActorLocation();
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
				Anc->InitialFurnitureYaw = CurYaw;
				Anc->InitialAimYaw       = P->GetBaseAimRotation().Yaw;
				Anc->InitialPlayerYaw    = P->GetActorRotation().Yaw;
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

	// 인원 미달(2인 가구 솔로 끌기) 판정 — Step 1의 회전 동결과 2.5의 끌림 자세가 함께 사용
	const int32 RequiredPlayers = FurnitureStat->GetRequiredPlayer();
	bool        bUnderManned    = (N == 1) && (RequiredPlayers >= 2);

	// [운반 속도 서버 강제] 외부에서 MaxWalkSpeed가 덮어써져도 서버가 매 틱 운반 감속을 재적용한다
	{
		const float CarrySpeed = ComputeCarrySpeed();
		for (ACharacter* P : Players)
		{
			if (UCharacterMovementComponent* CMC = P->GetCharacterMovement())
			{
				if (!FMath::IsNearlyEqual(CMC->MaxWalkSpeed, CarrySpeed))
				{
					//UE_LOG(LogCarry, Warning, TEXT("[속도 강제] %s %.0f→%.0f (운반 감속이 외부에서 덮어써짐)"),
					//	*P->GetName(), CMC->MaxWalkSpeed, CarrySpeed);
					CMC->MaxWalkSpeed = CarrySpeed;
				}
			}
		}
	}

	// ---- 0.5. [들것 회전] 2인 이상: 가구 목표 Yaw = '두 운반자를 잇는 선'의 회전 ----
	// 카메라를 돌려도 가구는 돌지 않고, 한 사람이 상대를 축으로 걸어 돌면 가구가 따라 돈다.
	// 매 틱 선 Yaw 변화량을 'A만 움직였을 때 / B만 움직였을 때'로 분해해, 능동(직접 걷는)
	// 플레이어의 기여만 회전 의도로 누적한다. 피동(견인) 이동까지 포함하면 회전→견인→
	// 선 회전→재회전의 폭주 피드백이 생기므로 제외. 3인 이상은 앞의 두 명이 기준선.
	const bool bPairLine = bPairLineRotation && N >= 2;
	float PairLineIntentRate = 0.0f;   // 이번 틱 선 회전 의도(도/초) — Step 5 '회전 중 견인 보류'용
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
			PairLineIntentRate = FMath::Abs(IntentDelta) / FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);
		}
		// 두 사람이 너무 가까우면 선 방향이 불안정 → 이번 틱은 의도 누적 없이 유지

		PairLinePrevPosA = PosA;
		PairLinePrevPosB = PosB;
	}
	else
	{
		bPairLineValid = false;
	}

	// ---- 0.7. [운반 거리 테더] ----
	// 가구가 운반자에게서 MaxReach 이상 벌어지면 앵커 오프셋을 틱당 소량씩 재기록해 거리를 회복한다 (1인 운반 한정)
	if (N == 1 && FurnitureMesh)
	{
		const float MaxReach = 110.0f;
		const FVector PPos = Players[0]->GetActorLocation();
		const FVector NearPt = FurnitureMesh->Bounds.GetBox().GetClosestPointTo(PPos);
		FVector Gap(NearPt.X - PPos.X, NearPt.Y - PPos.Y, 0.0f);
		const float GapLen = Gap.Size();
		if (GapLen > MaxReach)
		{
			// 틱당 최대 4uu씩만 (팝 없이 스르륵 회복)
			const float PullLen = FMath::Min(GapLen - MaxReach, 4.0f);
			const FVector PullWorld = Gap.GetSafeNormal() * PullLen;
			if (FGrabAnchor* Anc = Anchors.Find(Players[0]))
			{
				// 오프셋은 그랩 시점 Yaw 기준이므로 현재 회전분을 되돌려 기록.
				// Reliable 멀티캐스트는 틱마다 발사될 수 있어 생략 (클라는 이 XY 오프셋을 배치에 쓰지 않음)
				const float YC = FMath::FindDeltaAngleDegrees(Anc->InitialFurnitureYaw, CurFurnYaw);
				Anc->InitialOffset -= PullWorld.RotateAngleAxis(-YC, FVector::UpVector);
			}
		}
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
		// [들것 회전 — 걷기 기반] 2인 이상은 카메라가 아니라 '두 운반자를 잇는 선'의 회전(0.5 누적)만 반영.
		// 솔로 끌기(인원 미달)와 들것 설정 꺼짐(N≥2)은 회전 동결, 들기(피치)만.
		const float        PlayerYawDelta = bPairLine
			? FMath::FindDeltaAngleDegrees(Anc.InitialFurnitureYaw, PairLineTargetYaw)
			: ((N >= 2 || bUnderManned)
				? 0.0f
				: FMath::FindDeltaAngleDegrees(Anc.InitialAimYaw, PlayerAimYaw));
		const float        ProposalYaw   = Anc.InitialFurnitureYaw + PlayerYawDelta;
		const FVector      ProposalLoc   = P->GetActorLocation() + Anc.InitialOffset.RotateAngleAxis(PlayerYawDelta, FVector::UpVector);
		float              Demand        = FVector(ProposalLoc.X - CurFurnLoc.X, ProposalLoc.Y - CurFurnLoc.Y, 0.0f).Size();
		// [견인 데드존 보완] 데드존 안에서 '서 있는' 플레이어는 낡은 앵커 제안이 가구를
		// 뒤로 당기는 브레이크가 됨 — 가구가 멈췄다가 상대가 견인으로 전환되는 순간 점프.
		// 자기 이동량 이상의 발언권을 주지 않는다: 서 있으면 최소 가중치, 걸으면 즉시 회복.
		// [들것 조향] 들것 모드에서 이동 입력 중인 운반자는 캡을 풀어 가구 위치를 전담시킨다.
		// 서 있는 축은 캡 유지로 위치 발언권이 없고, 회전은 선(0.5) 기준으로만 돈다.
		{
			const UCharacterMovementComponent* WCMC = P->GetCharacterMovement();
			const bool bSteering = bPairLine && WCMC
				&& WCMC->GetCurrentAcceleration().SizeSquared2D() > FMath::Square(10.0f);
			if (!bSteering)
			{
				Demand = FMath::Min(Demand, P->GetVelocity().Size2D() * DeltaTime * 4.0f + 1.0f);
			}
		}
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
			// 교착 복구 경로도 본 루프와 동일한 회전 규칙 적용 (들것 선 회전 / 동결)
			const float        PlayerYawDelta = bPairLine
				? FMath::FindDeltaAngleDegrees(Anc.InitialFurnitureYaw, PairLineTargetYaw)
				: ((N >= 2 || bUnderManned)
					? 0.0f
					: FMath::FindDeltaAngleDegrees(Anc.InitialAimYaw, PlayerAimYaw));
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
	// [들것 회전 속도] 들것 모드는 회전 상한 2배 — 궤도 회전의 선 각속도를 가구 Yaw가 따라잡게 한다
	const float TargetYaw    = FMath::FixedTurn(CurFurnYaw, TargetYawRaw,
		FurnYawRotationSpeed * (bPairLineValid ? 2.0f : 1.0f) * DeltaTime);

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
			// 절대 피치 기준(정면 0°=기본 높이), ×2 감도, ±45° 클램프로 극단 피치의 과대 목표 제한
			const float Hi = FMath::Clamp(FoldAimPitch(Players[i]->GetBaseAimRotation().Pitch), -45.0f, 45.0f)
			               * FurnitureHeightPerPitch * 2.0f;
			HSum += Hi;
			if (i == 0)      { PairHandHeight0 = Hi; }
			else if (i == 1) { PairHandHeight1 = Hi; }
		}
		TargetHeightOffset = HSum / (float)N;
	}

	// [인원 미달 드래그 연출] 2인 가구를 혼자 들면 잡은 쪽만 들리고 반대쪽 끝이 바닥에
	// 끌리는 자세가 되도록 — 기울기(Step 3)와 함께 중심을 낮춰 먼 쪽 끝을 바닥에 붙인다.
	// '이건 혼자 못 드는 가구'라는 걸 시각적으로 즉시 전달. 인원 충족 시 보간으로 수평 복귀.
	// 45°+ 기울어 놓인 가구는 자세 교정·끌림 접지 대상에서 제외하고 일반 들기(피치 높이)로 운반한다
	const bool bUprightEnough = Owner->GetActorQuat().GetUpVector().Z > 0.7f;

	float   UnderMannedTilt = 0.0f;
	FVector UnderMannedDir  = FVector::ZeroVector;
	if (bUnderManned && bUprightEnough)
	{
		// '플레이어 반대쪽' 방향은 피벗이 아니라 메시 바운즈 중심 기준으로 계산한다 (기울기 축·먼 쪽 판정에 사용)
		const FVector FurnCenter = FurnitureMesh ? FurnitureMesh->Bounds.Origin : CurFurnLoc;
		UnderMannedDir = FVector(FurnCenter.X - Players[0]->GetActorLocation().X,
		                         FurnCenter.Y - Players[0]->GetActorLocation().Y, 0.0f);
		if (UnderMannedDir.Normalize())
		{
			// 카메라 피치 → 기울기 각도 조절 (정면=기본 14°, 위=더 들림, 아래=거의 평평). 먼 쪽 끝은 항상 바닥에 끌린다
			const float AimPitchDeg = FoldAimPitch(Players[0]->GetBaseAimRotation().Pitch);
			// 기울기 상한은 고정 각도 대신 '든 쪽 끝 들림 높이(~70uu)' 기준 동적 캡 (작은 가구는 크게, 대형은 낮게)
			const FVector ExtForTilt = FurnitureMesh ? FurnitureMesh->Bounds.BoxExtent : FVector(50.0f);
			const float SpanXY = 2.0f * (FMath::Abs(ExtForTilt.X * UnderMannedDir.X)
			                           + FMath::Abs(ExtForTilt.Y * UnderMannedDir.Y));
			// 상한 28°: 45° 넘어짐 판정까지 17° 안전 마진을 유지한다
			const float TiltMax = FMath::Clamp(FMath::RadiansToDegrees(
				FMath::Atan2(70.0f, FMath::Max(SpanXY, 50.0f))), 8.0f, 28.0f);
			UnderMannedTilt = FMath::Clamp(14.0f + AimPitchDeg * 1.0f, 2.0f, TiltMax);
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
			// [지지면 기준 하강] 접지 높이(FloorZ)는 2점(중심+끌리는 쪽 끝) 최대값, 모드 판정(FloorZGate)은 중심점만 사용
			const float CarrierFootZ = FloorZ;
			float FloorZGate = FloorZ;
			if (FurnitureMesh)
			{
				const FVector BoundsOrigin = FurnitureMesh->Bounds.Origin;
				FCollisionQueryParams DropParams(SCENE_QUERY_STAT(UnderMannedDrop), false, Owner);
				DropParams.AddIgnoredActor(Players[0]);
				const float ExtAlong = FMath::Abs(Ext.X * UnderMannedDir.X) + FMath::Abs(Ext.Y * UnderMannedDir.Y);
				const FVector SamplePts[2] = {
					BoundsOrigin,
					BoundsOrigin + UnderMannedDir * (ExtAlong * 0.5f)
				};
				for (int32 s = 0; s < 2; ++s)
				{
					const FVector& Pt = SamplePts[s];
					const FVector DropStart(Pt.X, Pt.Y, BoundsOrigin.Z - Ext.Z + 5.0f);
					const FVector DropEnd(Pt.X, Pt.Y, FloorZ - 20.0f);
					FHitResult DropHit;
					if (DropStart.Z > DropEnd.Z && GetWorld()->LineTraceSingleByChannel(
							DropHit, DropStart, DropEnd, ECC_Visibility, DropParams))
					{
						FloorZ = FMath::Max(FloorZ, DropHit.ImpactPoint.Z);
						if (s == 0)
							FloorZGate = FMath::Max(FloorZGate, DropHit.ImpactPoint.Z);
					}
				}

				// 풋프린트 판 하향 스윕(정적 전용): 점 트레이스가 놓치는 '일부만 걸친 지지물'을 감지한다.
				// XY 10% 축소 + 상향 노멀만 인정 (벽 측면 히트 배제)
				{
					const FCollisionShape Plate = FCollisionShape::MakeBox(
						FVector(Ext.X * 0.9f, Ext.Y * 0.9f, 2.0f));
					const FVector SweepStart(BoundsOrigin.X, BoundsOrigin.Y, BoundsOrigin.Z - Ext.Z + 60.0f);
					const FVector SweepEnd(BoundsOrigin.X, BoundsOrigin.Y, FloorZ - 20.0f);
					FCollisionObjectQueryParams StaticObj(ECC_WorldStatic);
					FHitResult PlateHit;
					if (SweepStart.Z > SweepEnd.Z && GetWorld()->SweepSingleByObjectType(
							PlateHit, SweepStart, SweepEnd, FQuat::Identity, StaticObj, Plate, DropParams)
						&& !PlateHit.bStartPenetrating
						&& PlateHit.ImpactNormal.Z > 0.7f)
					{
						FloorZ = FMath::Max(FloorZ, PlateHit.Location.Z - 2.0f);
					}
				}
			}
			// [열린 공간 전용] 끌림 자세는 지지면이 운반자 발밑과 비슷한 높이일 때만 발동한다 (중심점 지지면 기준)
			if (FloorZGate <= CarrierFootZ + 40.0f)
			{
				const float CenterOffZ = FurnitureMesh ? (FurnitureMesh->Bounds.Origin.Z - Owner->GetActorLocation().Z) : 0.0f;
				// +3: 관통 없이 '닿아 보이는' 유격 (회전은 스윕이 없어 유격이 작으면 관통함).
				// 중심 절대 상한(발밑+100): 기울어진 대형 가구가 통째로 솟는 것을 제한
				const float DesiredCenterZ = FMath::Min(FloorZ + Ext.Z + 3.0f, CarrierFootZ + 100.0f);
				TargetHeightOffset = DesiredCenterZ - (TargetLoc.Z + CenterOffZ);
			}
			else
			{
				UnderMannedTilt = 0.0f;   // 높은 곳에서 뽑는 중 — 기울기·하강 없이 일반 운반
			}
		}
	}

	// [요건 충족 시 최소 운반 높이] 인원 충족 시 가구 하단이 '평균 발밑+20' 위로 오도록 높이 하한을 끌어올린다.
	// '같이 내려놓기'는 전원이 -25° 이하를 보고 + 가구가 실제로 들려 있을 때만 허용한다
	bool bLoweringTogether = false;
	if (!bUnderManned && FurnitureMesh)
	{
		bool bAllDown = true;
		for (int32 i = 0; i < N && bAllDown; ++i)
		{
			if (FoldAimPitch(Players[i]->GetBaseAimRotation().Pitch) > -25.0f)
				bAllDown = false;
		}
		if (bAllDown)
		{
			float LowFootZ = 0.0f;
			for (int32 i = 0; i < N; ++i)
			{
				float Foot = Players[i]->GetActorLocation().Z - 90.0f;
				if (const UCapsuleComponent* Cap = Players[i]->GetCapsuleComponent())
					Foot = Players[i]->GetActorLocation().Z - Cap->GetScaledCapsuleHalfHeight();
				LowFootZ += Foot;
			}
			LowFootZ /= (float)N;
			const float BottomZ = FurnitureMesh->Bounds.Origin.Z - FurnitureMesh->Bounds.BoxExtent.Z;
			bLoweringTogether = (BottomZ - LowFootZ) > 30.0f;
		}
	}

	if (!bUnderManned && !bLoweringTogether && FurnitureMesh)
	{
		float AvgFootZ = 0.0f;
		for (int32 i = 0; i < N; ++i)
		{
			float Foot = Players[i]->GetActorLocation().Z - 90.0f;
			if (const UCapsuleComponent* Cap = Players[i]->GetCapsuleComponent())
			{
				Foot = Players[i]->GetActorLocation().Z - Cap->GetScaledCapsuleHalfHeight();
			}
			AvgFootZ += Foot;
		}
		AvgFootZ /= (float)N;

		const FVector ExtNow  = FurnitureMesh->Bounds.BoxExtent;
		const float CenterOff = FurnitureMesh->Bounds.Origin.Z - Owner->GetActorLocation().Z;
		const float MinCenterZ = AvgFootZ + 20.0f + ExtNow.Z;
		const float NeededOffset = MinCenterZ - (TargetLoc.Z + CenterOff);
		TargetHeightOffset = FMath::Max(TargetHeightOffset, NeededOffset);
	}

	// 현재 높이 오프셋에서 목표 높이 오프셋으로 부드럽게 보간.
	if (bUnderManned)
	{
		// 끌림 자세는 상승·하강 모두 등속 보간 — 지지면 경계에서 위로 튕기는 바운스를 막는다
		CurrentHeightOffset = FMath::FInterpConstantTo(
			CurrentHeightOffset, TargetHeightOffset, DeltaTime, 180.0f);
	}
	else
	{
		CurrentHeightOffset = FMath::FInterpTo(CurrentHeightOffset, TargetHeightOffset, DeltaTime, FurnitureHeightInterpSpeed);
	}

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
		// 인원 미달 드래그 자세·같이 내려놓기(전원 내려다봄)는 바닥 접지까지 하한 완화
		const float MinZ = AvgPlayerZ + FurnitureHeightMin
			- ((bUnderManned || bLoweringTogether) ? 250.0f : 0.0f);
		// 상한은 '메시 하단 ≤ 플레이어+Max' 기준 — 세로로 길거나 높이 놓인 가구도 들 수 있게 한다
		const float MaxCenterZ = AvgPlayerZ + FurnitureHeightMax
			+ (FurnitureMesh ? FurnitureMesh->Bounds.BoxExtent.Z : 0.0f);
		const float ClampedCenterZ = FMath::Clamp(DesiredCenterZ, MinZ, MaxCenterZ);
		// 윈드업 방지: 범위 밖 입력이 계속 쌓이지 않도록, 실제 적용 가능한 오프셋으로 되돌려 저장
		CurrentHeightOffset = (ClampedCenterZ - MeshCenterOffZ) - TargetLoc.Z;

		// 높이 진단 (F9 동안 0.5초 간격): 피치→목표→클램프 어디서 막히는지 추적
		//if (IsCarryDebugEnabled())
		//{
		//	static double GLastHeightLog = -10.0;
		//	const double NowT = GetWorld()->GetTimeSeconds();
		//	if (NowT - GLastHeightLog > 0.5)
		//	{
		//		GLastHeightLog = NowT;
		//		UE_LOG(LogCarry, Log, TEXT("[높이] N=%d 미달=%d 정립=%d 기움=%.0f° 피치0=%.0f° 목표=%.0f 적용=%.0f 중심Z=%.0f (허용 %.0f~%.0f)"),
		//			N, bUnderManned ? 1 : 0, bUprightEnough ? 1 : 0,
		//			FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
		//				Owner->GetActorQuat().GetUpVector().Z, -1.0f, 1.0f))),
		//			FRotator::NormalizeAxis(Players[0]->GetBaseAimRotation().Pitch),
		//			TargetHeightOffset, CurrentHeightOffset, ClampedCenterZ,
		//			MinZ, MaxCenterZ);
		//	}
		//}
	}

	// ---- 3. 가구 이동 (sweep=true, 가구 자체 충돌) ----
	// Pitch/Roll은 현재 값 유지: 물리로 쓰러진 가구는 그 자세 그대로 운반 (억지로 세우면 바닥 파고듦)
	FRotator TargetRot = Owner->GetActorRotation();
	TargetRot.Yaw = TargetYaw;

	// [들것 기울기 — 측정-보정형] 각자의 피치가 자기 쪽 손 높이, 차이가 목표 경사(45°/s 보정).
	// 공동운반은 정립 게이트 없이 항상 수평 복원한다
	if (bPairLine && N >= 2 && FurnitureHeightPerPitch != 0.0f
		&& FurnitureMesh && FurnitureMesh->GetStaticMesh())
	{
		const FVector PosA = Players[0]->GetActorLocation();
		const FVector PosB = Players[1]->GetActorLocation();
		FVector LineDir(PosB.X - PosA.X, PosB.Y - PosA.Y, 0.0f);
		const float PairDist = LineDir.Size();
		if (PairDist > 1.0f)
		{
			LineDir /= PairDist;

			// 현재 경사 실측 (A→B 방향의 바운즈 표면 지점 두 개의 높이차)
			const FTransform MeshT = FurnitureMesh->GetComponentTransform();
			const FBoxSphereBounds LB = FurnitureMesh->GetStaticMesh()->GetBounds();
			const FVector LDir = MeshT.InverseTransformVectorNoScale(LineDir).GetSafeNormal();
			const FVector BLocal = LB.Origin + FVector(LDir.X * LB.BoxExtent.X, LDir.Y * LB.BoxExtent.Y, LDir.Z * LB.BoxExtent.Z);
			const FVector ALocal = LB.Origin - FVector(LDir.X * LB.BoxExtent.X, LDir.Y * LB.BoxExtent.Y, LDir.Z * LB.BoxExtent.Z);
			const FVector WB = MeshT.TransformPosition(BLocal);
			const FVector WA = MeshT.TransformPosition(ALocal);
			const float HorizDist = FMath::Max(FVector::Dist2D(WB, WA), 10.0f);
			const float SlopeCurDeg = FMath::RadiansToDegrees(FMath::Atan2(WB.Z - WA.Z, HorizDist));

			// B쪽 손이 높으면 B쪽 끝이 올라감(±20° 제한). 손높이차는 최소 '가구 스팬' 위에 펼쳐 경사로 환산.
			// 간격이 최소치 미만이면 경사 0(수평 복원)만 적용한다
			const float SlopeBase = FMath::Max(PairDist, HorizDist);
			const float TargetSlopeDeg = (PairDist > PairLineMinDistance)
				? FMath::Clamp(FMath::RadiansToDegrees(
					FMath::Atan2(PairHandHeight1 - PairHandHeight0, SlopeBase)), -20.0f, 20.0f)
				: 0.0f;

			const float TiltRate = 45.0f;
			const float DeltaDeg = FMath::Clamp(TargetSlopeDeg - SlopeCurDeg,
			                                    -TiltRate * DeltaTime, TiltRate * DeltaTime);
			const FVector TiltAxis = FVector::CrossProduct(LineDir, FVector::UpVector);

			// [측면 수평 보정] 페어 라인의 직교 성분(측면 롤)을 실측해 0으로 복원한다 (끌림 자세와 동일 체계)
			const FVector SideDir  = FVector::CrossProduct(FVector::UpVector, LineDir);
			const FVector LSide    = MeshT.InverseTransformVectorNoScale(SideDir).GetSafeNormal();
			const FVector SideBL   = LB.Origin + FVector(LSide.X * LB.BoxExtent.X, LSide.Y * LB.BoxExtent.Y, LSide.Z * LB.BoxExtent.Z);
			const FVector SideAL   = LB.Origin - FVector(LSide.X * LB.BoxExtent.X, LSide.Y * LB.BoxExtent.Y, LSide.Z * LB.BoxExtent.Z);
			const FVector WSB      = MeshT.TransformPosition(SideBL);
			const FVector WSA      = MeshT.TransformPosition(SideAL);
			const float SideHoriz  = FMath::Max(FVector::Dist2D(WSB, WSA), 10.0f);
			const float SideSlope  = FMath::RadiansToDegrees(FMath::Atan2(WSB.Z - WSA.Z, SideHoriz));
			const float SideDelta  = FMath::Clamp(-SideSlope, -TiltRate * DeltaTime, TiltRate * DeltaTime);
			const FVector SideAxis = FVector::CrossProduct(SideDir, FVector::UpVector);

			const FQuat YawDeltaQ(FVector::UpVector,
				FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(Owner->GetActorRotation().Yaw, TargetYaw)));
			FQuat NewQ = FQuat(TiltAxis, FMath::DegreesToRadians(DeltaDeg))
			           * FQuat(SideAxis, FMath::DegreesToRadians(SideDelta))
			           * YawDeltaQ * Owner->GetActorQuat();
			// 안전핀: 들것도 동일 — 35° 초과 기울기는 이번 틱 기울기 성분 폐기
			if (NewQ.GetUpVector().Z < 0.82f)
			{
				NewQ = YawDeltaQ * Owner->GetActorQuat();
			}
			TargetRot = NewQ.Rotator();
		}
	}
	// [인원 미달 드래그 연출] 잡은 쪽만 들리고 먼 쪽 끝이 바닥으로 기우는 자세 (위 2.5의 중심 낮춤과 세트)
	// '측정-보정형': 실제 경사를 재고 목표 경사로 초당 일정 각도만 회전한다 (절대 자세 스냅 없음)
	else if (bUnderManned && bUprightEnough && UnderMannedTilt > 0.0f && FurnitureMesh && FurnitureMesh->GetStaticMesh())
	{
		const FTransform MeshT = FurnitureMesh->GetComponentTransform();
		const FBoxSphereBounds LB = FurnitureMesh->GetStaticMesh()->GetBounds();
		// D(수평)를 로컬로 가져와 바운즈 표면의 근/원 지점을 잡고 월드 경사를 실측
		const FVector LDir = MeshT.InverseTransformVectorNoScale(UnderMannedDir).GetSafeNormal();
		const FVector FarLocal  = LB.Origin + FVector(LDir.X * LB.BoxExtent.X, LDir.Y * LB.BoxExtent.Y, LDir.Z * LB.BoxExtent.Z);
		const FVector NearLocal = LB.Origin - FVector(LDir.X * LB.BoxExtent.X, LDir.Y * LB.BoxExtent.Y, LDir.Z * LB.BoxExtent.Z);
		const FVector WFar  = MeshT.TransformPosition(FarLocal);
		const FVector WNear = MeshT.TransformPosition(NearLocal);
		const float HorizDist = FMath::Max(FVector::Dist2D(WFar, WNear), 10.0f);
		const float SlopeCurDeg = FMath::RadiansToDegrees(FMath::Atan2(WFar.Z - WNear.Z, HorizDist));

		// 목표: 먼 쪽이 -UnderMannedTilt 만큼 낮게. 부족분만 초당 45°로 보정.
		const float TiltRate = 45.0f;
		const float DeltaDeg = FMath::Clamp(-UnderMannedTilt - SlopeCurDeg,
		                                    -TiltRate * DeltaTime, TiltRate * DeltaTime);
		const FVector TiltAxis = FVector::CrossProduct(UnderMannedDir, FVector::UpVector);

		// [측면 수평 보정] 기울기 축의 직교 성분(측면 롤)을 실측해 0으로 복원한다 (같은 측정-보정 체계)
		const FVector SideDir  = FVector::CrossProduct(FVector::UpVector, UnderMannedDir);
		const FVector LSide    = MeshT.InverseTransformVectorNoScale(SideDir).GetSafeNormal();
		const FVector SideFarL = LB.Origin + FVector(LSide.X * LB.BoxExtent.X, LSide.Y * LB.BoxExtent.Y, LSide.Z * LB.BoxExtent.Z);
		const FVector SideNearL= LB.Origin - FVector(LSide.X * LB.BoxExtent.X, LSide.Y * LB.BoxExtent.Y, LSide.Z * LB.BoxExtent.Z);
		const FVector WSFar    = MeshT.TransformPosition(SideFarL);
		const FVector WSNear   = MeshT.TransformPosition(SideNearL);
		const float SideHoriz  = FMath::Max(FVector::Dist2D(WSFar, WSNear), 10.0f);
		const float SideSlope  = FMath::RadiansToDegrees(FMath::Atan2(WSFar.Z - WSNear.Z, SideHoriz));
		const float SideDelta  = FMath::Clamp(-SideSlope, -TiltRate * DeltaTime, TiltRate * DeltaTime);
		const FVector SideAxis = FVector::CrossProduct(SideDir, FVector::UpVector);

		const FQuat YawDeltaQ(FVector::UpVector,
			FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(Owner->GetActorRotation().Yaw, TargetYaw)));
		FQuat NewQ = FQuat(TiltAxis, FMath::DegreesToRadians(DeltaDeg))
		           * FQuat(SideAxis, FMath::DegreesToRadians(SideDelta))
		           * YawDeltaQ * Owner->GetActorQuat();
		// 안전핀: 결과가 35°를 넘게 기울면 이번 틱 기울기 성분을 폐기한다 (45° 넘어짐 판정 진입 방지)
		if (NewQ.GetUpVector().Z < 0.82f)
		{
			NewQ = YawDeltaQ * Owner->GetActorQuat();
		}
		TargetRot = NewQ.Rotator();
	}

	// HeightOffset 대신 보간된 CurrentHeightOffset 적용
	const FVector DesiredPos = TargetLoc + FVector(0.0f, 0.0f, CurrentHeightOffset);
	const FRotator PreMoveRot = Owner->GetActorRotation();   // 회전 관통 롤백·보정 기준
	// 피벗-중심 보정용: 이동 전 메시 중심의 로컬 오프셋 캡처 (스케일 포함)
	const FVector PreLocalCenter = FurnitureMesh
		? PreMoveRot.Quaternion().Inverse().RotateVector(
			FurnitureMesh->Bounds.Origin - Owner->GetActorLocation())
		: FVector::ZeroVector;

	FHitResult MoveHit;
	Owner->SetActorLocationAndRotation(DesiredPos, TargetRot, true, &MoveHit);

	// [관통 동결 해제 밸브] 시작 시점에 겹쳐 있으면 스윕이 전부 거부되므로,
	// 겹침이 풀릴 때까지 소량씩 수직 텔레포트로 꺼낸 뒤 이동을 한 번 재시도한다
	if (MoveHit.bStartPenetrating && FurnitureMesh)
	{
		FComponentQueryParams UnstickParams(SCENE_QUERY_STAT(CarryUnstick), Owner);
		for (ACharacter* P : Players)
		{
			UnstickParams.AddIgnoredActor(P);
		}
		// 정적+동적(바리케이드·다른 가구) 모두 겹침 검사, 잡은 플레이어는 무시
		FCollisionObjectQueryParams UnstickStatic(ECC_WorldStatic);
		UnstickStatic.AddObjectTypesToQuery(ECC_WorldDynamic);
		UnstickStatic.AddObjectTypesToQuery(ECC_PhysicsBody);
		TArray<FOverlapResult> StuckOverlaps;
		const FVector PreUnstickLoc = Owner->GetActorLocation();
		bool bUnstuck = false;
		for (int32 Step = 0; Step < 12; ++Step)
		{
			StuckOverlaps.Reset();
			if (!GetWorld()->ComponentOverlapMulti(StuckOverlaps, FurnitureMesh,
				FurnitureMesh->GetComponentLocation(),
				FurnitureMesh->GetComponentQuat(), UnstickParams, UnstickStatic))
			{
				bUnstuck = true;
				break;
			}
			Owner->AddActorWorldOffset(FVector(0.0f, 0.0f, 6.0f), false);
		}
		if (bUnstuck)
		{
			Owner->SetActorLocationAndRotation(DesiredPos, TargetRot, true);
		}
		else
		{
			// 상향 탈출 실패(선반 밑 등 위가 막힌 형태)면 원위치 유지
			Owner->SetActorLocation(PreUnstickLoc, false);
		}
		if (IsCarryDebugEnabled() && GEngine)
		{
			DrawDebugBox(GetWorld(), FurnitureMesh->Bounds.Origin, FurnitureMesh->Bounds.BoxExtent,
				FurnitureMesh->GetComponentQuat(), FColor::Magenta, false, 2.0f, 0, 3.0f);
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Magenta,
				FString::Printf(TEXT("[운반] 관통 밸브 발동 → %s"), bUnstuck ? TEXT("상향 탈출") : TEXT("탈출 실패·원위치")));
		}
		//{
		//	// 연속 발동(파고듦 루프)은 0.5초 스로틀로만 기록 — 대신 발동 횟수를 함께 남긴다
		//	static double GLastValveLogTime = -10.0;
		//	static int32  GValveCountSinceLog = 0;
		//	++GValveCountSinceLog;
		//	const double NowT = GetWorld()->GetTimeSeconds();
		//	if (NowT - GLastValveLogTime > 0.5)
		//	{
		//		GLastValveLogTime = NowT;
		//		UE_LOG(LogCarry, Log, TEXT("[밸브] %s 시작 관통 → %s (0.5초간 %d회)"),
		//			*Owner->GetName(), bUnstuck ? TEXT("상향 탈출") : TEXT("탈출 실패·원위치"),
		//			GValveCountSinceLog);
		//		GValveCountSinceLog = 0;
		//	}
		//}
	}

	// [가구 스텝업] XY 이동이 낮은 장애물에 막히면 '들어 올려 → 재시도 → 안착'으로 타고 넘는다.
	// 벽처럼 높은 장애물은 리프트 상태에서도 막혀 순 이동 0 = 기존 차단 동작
	{
		const FVector AfterMove = Owner->GetActorLocation();
		const FVector WantXY(DesiredPos.X - AfterMove.X, DesiredPos.Y - AfterMove.Y, 0.0f);
		if (WantXY.SizeSquared() > FMath::Square(8.0f))
		{
			// 리프트를 낮은 것부터 점진 시도 — 필요한 만큼만 올라가고, 실효 이동이 없으면 통째로 원위치.
			// [자기 타격 방지] 리프트 스윕의 히트가 충돌 데미지로 들어가지 않도록 시도 동안 잠깐 무적.
			if (UFurnitureDamage* DamageComp = Owner->FindComponentByClass<UFurnitureDamage>())
			{
				DamageComp->SetInvincible(0.25f);
			}
			const FVector PreStepPos = Owner->GetActorLocation();
			// 틱당 이동량 제한: 여러 틱에 걸쳐 조금씩 옮겨 성공 순간의 점프를 막는다
			const FVector StepXY = WantXY.GetClampedToMaxSize(12.0f);
			bool bStepped = false;
			for (const float LiftH : { 15.0f, 30.0f, 45.0f })
			{
				Owner->SetActorLocation(PreStepPos, false);
				Owner->AddActorWorldOffset(FVector(0.0f, 0.0f, LiftH), true);   // ① 수직 리프트
				Owner->AddActorWorldOffset(StepXY, true);                        // ② 리프트 상태 XY 재시도
				Owner->AddActorWorldOffset(FVector(0.0f, 0.0f, -LiftH), true);  // ③ 하강 안착
				const FVector Gain = Owner->GetActorLocation() - PreStepPos;
				if (FVector(Gain.X, Gain.Y, 0.0f).SizeSquared() >= FMath::Square(4.0f))
				{
					bStepped = true;
					break;
				}
			}
			if (!bStepped)
			{
				Owner->SetActorLocation(PreStepPos, false);
			}
			//else
			//{
			//	// '툭 올라감' 추적용 (0.5초 스로틀)
			//	static double GLastStepLog = -10.0;
			//	const double NowT = GetWorld()->GetTimeSeconds();
			//	if (NowT - GLastStepLog > 0.5)
			//	{
			//		GLastStepLog = NowT;
			//		const FVector StepGain = Owner->GetActorLocation() - PreStepPos;
			//		UE_LOG(LogCarry, Log, TEXT("[스텝업] %s 순이동 XY=%.0f Z=%+.0f"),
			//			*Owner->GetName(), FVector(StepGain.X, StepGain.Y, 0).Size(), StepGain.Z);
			//	}
			//}
		}
	}

	// [회전 관통 방지] 회전에는 스윕이 없어 관통 가능 — 겹침 검사로 검증해 회전만 직전 값으로
	// 되돌린다(이동은 유지). 검사 위치 +3uu: 지지면에 얹힌 정상 접촉의 겹침 오탐 방지.
	if (FurnitureMesh && !PreMoveRot.Equals(Owner->GetActorRotation(), 0.05f))
	{
		FComponentQueryParams RotOverlapParams(SCENE_QUERY_STAT(CarryRotOverlap), Owner);
		for (ACharacter* P : Players)
		{
			RotOverlapParams.AddIgnoredActor(P);
		}
		FCollisionObjectQueryParams StaticOnly(ECC_WorldStatic);
		TArray<FOverlapResult> RotOverlaps;
		const bool bPenetrates = GetWorld()->ComponentOverlapMulti(
			RotOverlaps, FurnitureMesh,
			FurnitureMesh->GetComponentLocation() + FVector(0.f, 0.f, 3.f),
			FurnitureMesh->GetComponentQuat(), RotOverlapParams, StaticOnly);
		if (bPenetrates)
		{
			// 절반·1/4 부분 회전을 시도해 닿기 직전까지는 따라오게 한다
			const FQuat FromQ = PreMoveRot.Quaternion();
			const FQuat ToQ   = Owner->GetActorQuat();
			bool bResolved = false;
			for (const float T : { 0.5f, 0.25f })
			{
				Owner->SetActorRotation(FQuat::Slerp(FromQ, ToQ, T));
				RotOverlaps.Reset();
				if (!GetWorld()->ComponentOverlapMulti(RotOverlaps, FurnitureMesh,
					FurnitureMesh->GetComponentLocation() + FVector(0.f, 0.f, 3.f),
					FurnitureMesh->GetComponentQuat(), RotOverlapParams, StaticOnly))
				{
					bResolved = true;
					break;
				}
			}
			if (!bResolved)
			{
				Owner->SetActorRotation(PreMoveRot);
			}
			//{
			//	// '기울기/회전 고정' 추적용 (0.5초 스로틀)
			//	static double GLastRotRollbackLog = -10.0;
			//	const double NowT = GetWorld()->GetTimeSeconds();
			//	if (NowT - GLastRotRollbackLog > 0.5)
			//	{
			//		GLastRotRollbackLog = NowT;
			//		UE_LOG(LogCarry, Log, TEXT("[회전 롤백] %s %s"),
			//			*Owner->GetName(), bResolved ? TEXT("부분(1/2·1/4) 적용") : TEXT("전량 취소"));
			//	}
			//}
		}
	}

	// [피벗-중심 기울기 보정] '확정된'(롤백 반영 후) 회전 변화 기준으로 메시 중심 변위를 상쇄 —
	// 요 성분은 앵커가 피벗 기준이라 제외. [1인 전용] 피벗을 이동시키는 보정이라 2인 운반에선 끈다.
	if (FurnitureMesh && N == 1)
	{
		const FQuat OldQ = PreMoveRot.Quaternion();
		const FQuat NewQ = Owner->GetActorQuat();             // 롤백까지 반영된 확정 회전
		if (!NewQ.Equals(OldQ, 1.e-6f))
		{
			const FQuat YawOldQ = FRotator(0.0f, PreMoveRot.Yaw, 0.0f).Quaternion();
			const FQuat YawNewQ = FRotator(0.0f, Owner->GetActorRotation().Yaw, 0.0f).Quaternion();
			const FQuat RefQ = YawNewQ * (YawOldQ.Inverse() * OldQ);
			const FVector Comp = RefQ.RotateVector(PreLocalCenter) - NewQ.RotateVector(PreLocalCenter);
			if (!Comp.IsNearlyZero(0.01f))
			{
				Owner->AddActorWorldOffset(Comp, true);
			}
		}
	}

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
			if (bPairLineValid)
			{
				// [들것 회전] 의도 리셋만 수행 — 위치 재앵커는 피동 운반자의 견인 거리(Delta) 누적을 깨므로 하지 않는다
				PairLineTargetYaw = ActualYaw;
			}
			else
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
	}

	// ---- 3.6. Z 상승 막힘(천장 등) → Z 오프셋 리셋 ----
	// '위로 막힘'(목표 > 실제)만 재기록한다 (실제가 더 높은 경우는 스윕이 자연 하강시킴)
	if (TargetLoc.Z - ActualLoc.Z > CorrectionDeadzone)
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
			const bool bPathHit = GetWorld()->SweepSingleByProfile(Hit, StartPos + LiftZ, EndPos + LiftZ, FQuat::Identity,
				Cap->GetCollisionProfileName(), Shape, QP);
			if (bPathHit)
			{
				const FVector Shortfall(Att.X - Hit.Location.X, Att.Y - Hit.Location.Y, 0.0f);
				if (Shortfall.SizeSquared() > WorstBlock.SizeSquared())
					WorstBlock = Shortfall;
			}
			if (IsCarryDebugEnabled())
			{
				// 운반자 경로 판정: 초록=통과, 빨강=막힘(가구 후퇴 후보). 히트 지점에 구체.
				DrawDebugLine(GetWorld(), StartPos + LiftZ, EndPos + LiftZ,
					bPathHit ? FColor::Red : FColor::Green, false, -1.0f, 0, 2.0f);
				if (bPathHit)
				{
					DrawDebugSphere(GetWorld(), Hit.Location + LiftZ, 12.0f, 8, FColor::Red, false, -1.0f, 0, 1.5f);
					if (Hit.bStartPenetrating && GEngine)
					{
						GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red,
							FString::Printf(TEXT("[운반] %s 경로 스윕이 시작부터 관통 (벽 밀착?)"), *P->GetName()));
					}
				}
			}
		}
		if (WorstBlock.SizeSquared() > FMath::Square(BlockStopThreshold))
		{
			if (IsCarryDebugEnabled() && GEngine)
			{
				// 가구 후퇴 발동 표시 (F9 디버그)
				const FVector ArrowBase = (FurnitureMesh ? FurnitureMesh->Bounds.Origin : ActualLoc) + FVector(0, 0, 60);
				DrawDebugDirectionalArrow(GetWorld(), ArrowBase, ArrowBase - WorstBlock, 30.0f,
					FColor::Red, false, 0.5f, 0, 4.0f);
				GEngine->AddOnScreenDebugMessage(9238, 1.0f, FColor::Red,
					FString::Printf(TEXT("[운반] 가구 후퇴 %.0fuu (운반자 경로 막힘)"), WorstBlock.Size()));
			}
			//{
			//	// 후퇴는 벽 옆에서 매 틱 연속 발동이 정상이라 0.5초 스로틀로만 기록
			//	static double GLastRetreatLogTime = -10.0;
			//	const double NowT = GetWorld()->GetTimeSeconds();
			//	if (NowT - GLastRetreatLogTime > 0.5)
			//	{
			//		GLastRetreatLogTime = NowT;
			//		UE_LOG(LogCarry, Log, TEXT("[후퇴] %s %.0fuu (운반자 경로 막힘)"),
			//			*Owner->GetName(), WorstBlock.Size());
			//	}
			//}
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

	if (IsCarryDebugEnabled() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9239, 0.2f, FColor::White,
			FString::Printf(TEXT("[운반] N=%d 미달끌기=%d 높이 %.0f→%.0f 운반자막힘=%d"),
				N, bUnderManned ? 1 : 0, CurrentHeightOffset, TargetHeightOffset,
				bCarrierBlockedLastTick ? 1 : 0));
	}

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
		// [들것 한계 견인] 진입(한계)은 넓게 — 축 회전 중 목표 흔들림에 견인이 걸리지 않게.
		// 한계를 넘으면 Exit까지 '연속 추종'한다.
		const float PairHoldRadius = 70.0f;   // 첫 진입(한계) — 축 회전 중 목표 흔들림 보호
		// 이탈 반경은 주행 평형 지연보다 낮게 유지한다 (도달↔재견인 채터링 방지)
		const float PairHoldExit   = 8.0f;
		// 직전 틱에 도달한 직후엔 재진입도 좁게 — 완전히 정착한 뒤에만 넓은 진입 반경으로 복귀한다
		const bool bRecentlyDragged = bWasDragged || StoppedDraggingLastTick.Contains(P);
		const float AtTargetRadius = bPairLineValid
			? (bRecentlyDragged ? PairHoldExit : PairHoldRadius)
			: (bWasDragged ? CorrectionDeadzone
			               : FMath::Max(PullStartRadius, CorrectionDeadzone));
		// [겹침 방지] 데드존 여유는 옆·뒤 방향까지만 — 앵커 자리에서 '가구 중심 방향'으로
		// 일정 이상 파고들면(운반자-가구 충돌은 그랩 중 꺼져 있어 몸이 가구를 관통해 보임)
		// 도달 판정을 깨고 견인을 발동시켜 대형을 복원한다. 견인 램프(0.12s) 덕에 부드럽게 밀려남.
		bool bIntrudesFurniture = false;
		// 1인 + 들것 피동에 적용 — 운반 중 상호 충돌이 꺼져 있으므로 이 밀어냄이 겹침을 막는다
		if (Players.Num() == 1 || bPairLineValid)
		{
			// 방향 기준은 피벗(ActualLoc)이 아니라 메시 바운즈 중심 (피벗이 메시 밖인 가구의 방향 반전 방지)
			const FVector FurnCenterNow = FurnitureMesh ? FurnitureMesh->Bounds.Origin : ActualLoc;
			FVector DirToFurn(FurnCenterNow.X - Att.X, FurnCenterNow.Y - Att.Y, 0.0f);
			if (DirToFurn.Normalize())
			{
				const float TowardFurn = FVector::DotProduct(FVector(-Delta.X, -Delta.Y, 0.0f), DirToFurn);
				bIntrudesFurniture = TowardFurn > 12.0f;
			}
		}
		const bool bAtTarget   = !bIntrudesFurniture
			&& Delta.SizeSquared() <= FMath::Square(AtTargetRadius);

		// [입력 우선] 이동 입력 중인 운반자는 견인 대상에서 제외하고 능동으로 취급한다 (침범 밀어냄은 예외)
		const bool bHasMoveInput = CMC->GetCurrentAcceleration().SizeSquared2D() > FMath::Square(10.0f);
		const bool bActiveNow    = (bAtTarget && !bWasDragged) || (bHasMoveInput && !bIntrudesFurniture);

		if (IsCarryDebugEnabled() && GEngine)
		{
			// 선 = 플레이어→자기 대형 목표(Att). 초록=능동 / 노랑=견인 / 주황=도달 정지 / 빨강=침범 밀어냄.
			const int32  Idx = Players.IndexOfByKey(P);
			const FColor C = bIntrudesFurniture ? FColor::Red
				: (bActiveNow ? FColor::Green
				: (!bAtTarget ? FColor::Yellow : FColor::Orange));
			DrawDebugLine(GetWorld(), P->GetActorLocation(), Att, C, false, -1.0f, 0, 2.5f);
			DrawDebugSphere(GetWorld(), Att, 10.0f, 8, C, false, -1.0f, 0, 1.5f);
			GEngine->AddOnScreenDebugMessage(9240 + Idx, 0.2f, C,
				FString::Printf(TEXT("[운반 P%d] %s Δ=%.0f/허용%.0f 직전drag=%d 가속=%.0f"),
					Idx + 1,
					bIntrudesFurniture ? TEXT("침범→밀어냄")
						: (bActiveNow ? TEXT("능동")
						: (!bAtTarget ? TEXT("견인") : TEXT("도달정지"))),
					Delta.Size(), AtTargetRadius, bWasDragged ? 1 : 0,
					CMC->GetCurrentAcceleration().Size2D()));
		}

		if (bActiveNow)
		{
			//if (bWasDragged && bHasMoveInput)
			//{
			//	UE_LOG(LogCarry, Log, TEXT("[견인→능동] %s 이동 입력으로 견인 이탈 (Δ=%.0f)"),
			//		*P->GetName(), Delta.Size());
			//}
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

		// 회전 중(의도 >10°/s) 피동: 한계 안이면 완전 보류(축 유지), 밖이면 아래에서 초과분만 소프트 견인.
		const bool bRotationBusy = bPairLineValid && PairLineIntentRate > 10.0f;
		if (bRotationBusy && Delta.SizeSquared() <= FMath::Square(PairHoldRadius))
		{
			continue;
		}

		if (bAtTarget && bWasDragged)
		{
			//UE_LOG(LogCarry, Log, TEXT("[견인 도달] %s 정지 주입%s"), *P->GetName(),
			//	bPairLineValid ? TEXT(" (들것: 재기록 없음)") : TEXT(" + 앵커 재기록"));
			// 피동 플레이어가 방금 목표에 도달 → XY 정지 (관성 슬라이딩 방지)
			// Z는 보존: 낙하 중이면 중력 속도를 지워선 안 됨 (공중 정지/슬로모 방지)
			// [서버+클라 동시 주입] 서버와 소유 클라가 같은 값을 주입해 move 재생 결과를 일치시킨다
			CMC->Velocity = FVector(0.0f, 0.0f, CMC->Velocity.Z);
			Multicast_ApplyPlayerCorrection(P, FVector::ZeroVector, DesiredYaw);
			StoppedDraggingThisTick.Add(P);

			// 앵커 갱신: 도달 시점의 가구 상태를 새 기준점으로 설정
			// 갱신하지 않으면 다음 틱 ProposalYaw = grab당시InitFurnYaw + 카메라Delta
			// = 이전 가구 Yaw 기준 → 능동 플레이어의 ProposalYaw와 충돌 → 역회전 → 상호 피동 진동.
			// 갱신하면 ProposalYaw = ActualYaw + 0 = 현재 가구 Yaw → 두 플레이어 Yaw 제안 일치 → 안정.
			// [들것 제외] 들것은 Yaw 제안이 선 목표로 일치해 재기록이 불필요하다 (재기록하면 벌어진 간격이 새 대형이 됨)
			if (!bPairLineValid)
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
		// 들것 램프 0.10: 평형 지연 > 이탈 반경 유지 (주행 중 도달 채터링 방지)
		const float PullCloseTime = FMath::Max(SafeDeltaTime, bPairLineValid ? 0.10f : 0.12f);
		// 평시 전량(연속 추종), 회전 중엔 한계 초과분만(소프트 테더 — 축 유지)
		FVector PullDelta = Delta;
		if (bRotationBusy)
		{
			PullDelta = Delta.GetSafeNormal() * FMath::Max(Delta.Size() - PairHoldRadius, 0.0f);
		}
		// 들것 견인 속도 상한 = 운반 이속 ×1.5 (전속 주입 방지)
		const float PullSpeedCap = bPairLineValid
			? FMath::Min(MaxCorrectionSpeed, ComputeCarrySpeed() * 1.5f)
			: MaxCorrectionSpeed;
		const FVector NeededVelocity = (PullDelta / PullCloseTime).GetClampedToMaxSize(PullSpeedCap);
		const FVector CarryVelocity  = (NeededVelocity + NeededVelocity.GetSafeNormal() * CMC->BrakingDecelerationWalking * DeltaTime)
		                               .GetClampedToMaxSize(PullSpeedCap);
		// XY만 견인, Z는 보존: CarryVelocity.Z=0이라 통째로 대입하면 낙하 속도가 매 틱 0으로
		// 리셋되어 공중에서 슬로모션으로 떨어지는 현상 발생
		// [서버+클라 동시 주입] 위 도달 블록과 동일 — 서버도 같은 값을 주입해야 원격 폰 견인이 유효하다
		CMC->Velocity = FVector(CarryVelocity.X, CarryVelocity.Y, CMC->Velocity.Z);
		Multicast_ApplyPlayerCorrection(P, CarryVelocity, DesiredYaw);
		//if (!bWasDragged)
		//{
		//	UE_LOG(LogCarry, Log, TEXT("[견인 시작] %s Δ=%.0f 허용=%.0f 침범=%d 가속=%.0f"),
		//		*P->GetName(), Delta.Size(), AtTargetRadius, bIntrudesFurniture ? 1 : 0,
		//		CMC->GetCurrentAcceleration().Size2D());
		//}
		CurrentTickDragged.Add(P);
	}

	DraggedLastTick          = MoveTemp(CurrentTickDragged);
	StoppedDraggingLastTick  = MoveTemp(StoppedDraggingThisTick);

	// ---- 6. 안전장치 처리 ----
	for (ACharacter* P : ToRelease)
	{
		if (IsCarryDebugEnabled() && GEngine)
		{
			// 자동 해제 지점에 빨간 X(5초) + 사유 텍스트 표시 (F9 디버그)
			const FVector L = P->GetActorLocation();
			DrawDebugLine(GetWorld(), L + FVector(-50, -50, 0), L + FVector(50, 50, 0), FColor::Red, false, 5.0f, 0, 5.0f);
			DrawDebugLine(GetWorld(), L + FVector(-50, 50, 0), L + FVector(50, -50, 0), FColor::Red, false, 5.0f, 0, 5.0f);
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
				FString::Printf(TEXT("[운반] 자동 해제: %s 대형 이탈 %.0fuu (허용 %.0f)"),
					*P->GetName(),
					FVector::Dist2D(P->GetActorLocation(), GetAttachedLocation(P, ActualLoc, ActualYaw)),
					MaxGrabSeparationDistance));
		}
		//UE_LOG(LogCarry, Warning, TEXT("[자동 해제] %s 대형 이탈 %.0fuu (허용 %.0f)"),
		//	*P->GetName(),
		//	FVector::Dist2D(P->GetActorLocation(), GetAttachedLocation(P, ActualLoc, ActualYaw)),
		//	MaxGrabSeparationDistance);
		Release(P);
	}

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
	// F9 디버그가 켜진 동안만 발사 — 상시 발사하면 팀 패키지(Development)에서 속도 HUD가
	// 계속 뜨고 매 틱 멀티캐스트 대역폭을 소모한다
	if (IsCarryDebugEnabled())
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

	if (IsCarryDebugEnabled())
	{
		// 소유 클라 화면용: 주입 중인 견인 속도(파랑 화살표) / 정지 주입(빨강 점) 표시
		const FVector L = Player->GetActorLocation() + FVector(0, 0, 30);
		if (!CarryVelocity.IsNearlyZero(1.0f))
			DrawDebugDirectionalArrow(Player->GetWorld(), L, L + CarryVelocity * 0.25f, 25.0f, FColor::Cyan, false, 0.1f, 0, 3.0f);
		else
			DrawDebugSphere(Player->GetWorld(), L, 8.0f, 8, FColor::Red, false, 0.1f, 0, 2.0f);
	}

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
