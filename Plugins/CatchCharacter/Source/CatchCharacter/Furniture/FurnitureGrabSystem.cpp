// Fill out your copyright notice in the Description page of Project Settings.
//
// =====================================================================================
//  FurnitureGrabSystem - 가구 운반(그랩) 시스템의 원리
// =====================================================================================
//
// [이 컴포넌트가 하는 일]
//   플레이어(들)가 가구를 "잡고" 옮기면, 가구가 플레이어를 따라 움직이고/회전하게 만든다.
//   1명만 필요한 가구, 2명이 필요한 가구를 모두 지원하며, 멀티플레이(서버-클라)에서 동작한다.
//
// -------------------------------------------------------------------------------------
// [1] 누가 무엇을 계산하나? (서버 권위 + 클라 보간)
// -------------------------------------------------------------------------------------
//   * 서버: "진짜" 계산을 전부 한다. 매 프레임 플레이어 위치를 읽어 가구의 새 위치/회전을
//           정하고, 플레이어들을 가구에 맞게 정렬한다.  (HandleMovement)
//   * 클라(다른 사람 화면): 서버가 알려준 가구의 최종 위치(ServerLocation/ServerRotation)를
//           '부드럽게 따라가기'만 한다.  (UpdateClientInterpolation)
//   왜 이렇게? 멀티플레이는 서버가 정답을 갖고, 클라는 그 정답을 따라가야 일관되기 때문.
//
// -------------------------------------------------------------------------------------
// [2] 가구를 어떻게 움직이나? (핵심: "강체 추종 제안 + 입력량 가중 평균")
// -------------------------------------------------------------------------------------
//   잡는 순간, 각 플레이어에 대해 "가구가 나에 대해 어디에/어느 방향으로 있었는지"를 저장한다.
//     - InitialOffset      : (가구위치 - 내위치)   ← 나로부터 가구까지의 상대 위치
//     - InitialPlayerYaw    : 내 시선 방향
//     - InitialFurnitureYaw : 가구의 방향
//
//   매 프레임, 각 플레이어는 "내가 잡았던 그대로 가구가 나를 따라온다면 가구는 여기 있어야 해"
//   라는 '제안(proposal)'을 만든다. (= 1인 캐리에서 쓰는 그 강체 추종 공식)
//     - 내가 θ만큼 돌았으면 → 가구도 내 주위로 θ만큼 돌아야 하고
//     - 내가 이동했으면     → 가구도 그만큼 따라와야 한다
//
//   2명일 때는 두 제안을 그냥 평균내지 않고 "**입력량(=실제로 움직이거나 돌린 양)으로 가중**"
//   평균낸다.
//     - 가만히 있는 사람 = 제안이 '현재 가구 위치'와 거의 같음 → 가중치 ≈ 0
//     - 실제로 움직/도는 사람 = 제안이 많이 다름            → 가중치 큼
//   결과: **실제로 움직이는 사람이 '중심(피벗)'이 되어** 가구를 끌고/돌리고, 가만히 있던
//         사람은 그 결과에 맞춰 끌려간다. (한 명이 돌리면 반대편이 그 사람 기준으로 공전)
//
// -------------------------------------------------------------------------------------
// [3] 플레이어를 가구에 도로 붙이기 (보정)
// -------------------------------------------------------------------------------------
//   가구를 옮긴 뒤, 각 플레이어를 "가구에 대해 있어야 할 자리(AttachedTarget)"로 끌어온다.
//     - 반대로 당기던 사람/끌려가는 사람은 이 보정으로 '가구에 묶인' 느낌이 난다.
//     - 시선(Yaw)도 가구 회전량만큼 돌려줘서 계속 가구를 바라보게 한다.
//     - 단, 회전을 '직접 구동한 본인'은 보정값이 0에 가까워(이미 맞으므로) 시선이 자유롭다.
//
// -------------------------------------------------------------------------------------
// [4] 덜덜 떨림(지터)을 막는 장치들  ※ 과거 실패 버전의 원인을 정면으로 차단
// -------------------------------------------------------------------------------------
//   (a) 잡는 순간 SetReplicateMovement(false): 클라에서 가구 위치를 쓰는 주체를 '보간' 하나로
//       단일화. (안 끄면 엔진 기본 이동복제와 우리 보간이 서로 덮어써서 떨린다 → 과거 핵심 버그)
//   (b) 플레이어 보정 시 bJustTeleported = true: 무브먼트의 네트워크 스무딩이 이 보정을
//       '실제 속도'로 오해해 출렁이지 않게 한다.
//   (c) AddTickPrerequisiteComponent: 가구 틱을 캐릭터 무브먼트보다 '뒤'로 보내 항상 최신
//       플레이어 위치를 읽게 한다. (반쯤 갱신된 위치를 읽으면 떨린다)
//   (d) 데드존/클램프: 아주 작은 보정은 무시(미세 떨림 제거), 큰 보정은 상한선으로 제한(안전).
//   (e) 가구 이동은 항상 절대좌표 SetActorLocationAndRotation. AddOffset(누적 이동)는 금지
//       — 누적 방식은 변동이 커서 보간이 깨졌었음.
//
// =====================================================================================

#include "FurnitureGrabSystem.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "CatchCharacter/Furniture/FurnitureStat.h"

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
	// 스텟이 없으면 판단 불가 -> 막아둔다. 잡은 인원이 요구 인원보다 적으면 더 받을 수 있다.
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

void UFurnitureGrabSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner)
		return;

	const bool bAuthority = Owner->HasAuthority();
	const bool bGrabbed = GrabbedPlayers.Num() > 0;

	if (bAuthority && bGrabbed)
	{
		// 서버: 가구를 절대좌표로 이동시키고 플레이어를 추종시킨다.
		HandleMovement(DeltaTime);
	}
	else if (!bAuthority && bGrabbed)
	{
		// 클라: 서버가 보내준 단일 트랜스폼을 부드럽게 보간한다.
		UpdateClientInterpolation(DeltaTime);
	}
	else if (!bAuthority && !bGrabbed)
	{
		// 잡힌 상태가 아니면 다음 보간의 출발점을 현재 위치로 동기화.
		PreviousClientLoc = Owner->GetActorLocation();
		PreviousClientRot = Owner->GetActorRotation();
		bHasClientInterpInit = false;
	}
}

void UFurnitureGrabSystem::Grab(ACharacter* Grabber, FVector height, UPrimitiveComponent* GrabberComponent)
{
	AActor* Owner = GetOwner();

	// Grab 은 서버에서만 작업한다.
	if (!Owner || !Owner->HasAuthority() || !Grabber || GrabbedPlayers.Contains(Grabber))
		return;

	// 가구가 요구하는 인원까지만 잡을 수 있도록 제한.
	if (FurnitureStat && GrabbedPlayers.Num() >= FurnitureStat->GetRequiredPlayer())
		return;

	// 첫 번째로 잡는 사람: 물리를 끄고 살짝 들어올린 뒤, 우리가 직접 보간하도록 이동복제를 끈다.
	if (GrabbedPlayers.Num() == 0 && FurnitureMesh)
	{
		FurnitureMesh->SetSimulatePhysics(false);
		FurnitureMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

		// AddOffset 누적 이동 금지(조건5). 한 번의 셋업성 들어올림은 절대좌표 텔레포트로 처리.
		const FVector Lifted = Owner->GetActorLocation() + height;
		Owner->SetActorLocation(Lifted, false, nullptr, ETeleportType::TeleportPhysics);

		// 엔진 기본 이동복제를 끈다 -> 클라 가구 트랜스폼의 writer 를 보간 하나로 단일화(덜덜거림 차단).
		Owner->SetReplicateMovement(false);

		// 클라가 첫 프레임부터 유효한 목표를 가지도록 즉시 초기화.
		ServerLocation = Owner->GetActorLocation();
		ServerRotation = Owner->GetActorRotation();
	}

	// 잡은 플레이어 등록 (잡는 순간 가구를 바라보게 하는 정렬은 의도적으로 하지 않는다 - 카메라 강제 회전 방지)
	GrabbedPlayers.Add(Grabber);

	// 추종 기준값 기록: 잡은 순간의 상대 위치(가구-플레이어)와 양측 Yaw 를 절대 기준으로 저장.
	{
		FGrabAnchor Anchor;
		Anchor.InitialOffset = Owner->GetActorLocation() - Grabber->GetActorLocation();
		Anchor.InitialFurnitureYaw = Owner->GetActorRotation().Yaw;
		Anchor.InitialPlayerYaw = Grabber->GetActorRotation().Yaw;
		Anchors.Add(Grabber, Anchor);
	}

	// 충돌무시 + 틱순서(캐릭터 무브먼트 뒤에 가구가 틱하도록) 셋업
	SetGrabCollisionState(Grabber, true);

	// 이동속도: 가구 스텟에 정의된 운반 속도로 세팅(원래값 백업)
	if (UCharacterMovementComponent* CMC = Grabber->GetCharacterMovement())
	{
		if (!OriginalMaxWalkSpeeds.Contains(Grabber))
		{
			OriginalMaxWalkSpeeds.Add(Grabber, CMC->MaxWalkSpeed);
		}
		if (FurnitureStat)
		{
			CMC->MaxWalkSpeed = FurnitureStat->GetBaseSpeed();
		}
	}

	if (FurnitureStat)
	{
		FurnitureStat->UpdateGrabbedPlayers(GrabbedPlayers.Num());
	}
}

void UFurnitureGrabSystem::Release(ACharacter* Grabber)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !Grabber || !GrabbedPlayers.Contains(Grabber))
		return;

	// 충돌무시/틱순서 해제
	SetGrabCollisionState(Grabber, false);

	// 이동속도 원복
	if (UCharacterMovementComponent* CMC = Grabber->GetCharacterMovement())
	{
		if (OriginalMaxWalkSpeeds.Contains(Grabber))
		{
			CMC->MaxWalkSpeed = OriginalMaxWalkSpeeds[Grabber];
		}
	}
	OriginalMaxWalkSpeeds.Remove(Grabber);

	GrabbedPlayers.Remove(Grabber);
	Anchors.Remove(Grabber);

	// 마지막 사람이 놓으면 물리/이동복제 원복.
	if (GrabbedPlayers.Num() == 0 && FurnitureMesh)
	{
		FurnitureMesh->SetSimulatePhysics(true);
		FurnitureMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		Owner->SetReplicateMovement(true);
	}

	if (FurnitureStat)
	{
		FurnitureStat->UpdateGrabbedPlayers(GrabbedPlayers.Num());
	}
}

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
		{
			Players.Add(P);
		}
	}
	const int32 N = Players.Num();
	if (N == 0)
		return;

	const float CurFurnitureZ = Owner->GetActorLocation().Z;
	const FVector CurFurnitureLoc = Owner->GetActorLocation();

	// ---- 1~2. 입력량 가중 강체-추종 평균으로 가구 목표 트랜스폼 계산 ----
	//   각 플레이어 i 는 "자기를 강체로 따라오는 가구"(1인 캐리와 동일한 추종)를 제안한다:
	//     제안Yaw_i = 잡을때 가구Yaw + 플레이어가 잡은 이후 회전량
	//     제안Loc_i = 플레이어 위치 + (상대오프셋을 그 회전량만큼 돌린 것)
	//   이를 "현재 가구에서 얼마나 많이 바꾸려 하는가(=실제로 움직이거나 돌리는가)"로 가중 평균한다.
	//   => 가만히 있는 사람은 가중치≈0, 실제로 움직이는/돌리는 사람이 피벗이 되어 전체를 끌고/돌린다.
	//      (한 명이 돌리면 가구·반대편이 그 사람 기준으로 '전체' 회전 - 조건6)
	//   각도 평균은 wrap 안전을 위해 sin/cos 가중 합산으로 계산.
	const float Eps = 0.01f; // 모두 정지 시 50:50 으로 안정화시키는 미세 가중치
	double WSumSin = 0.0;
	double WSumCos = 0.0;
	FVector WLocSum = FVector::ZeroVector;
	double WTotal = 0.0;
	for (ACharacter* P : Players)
	{
		const FGrabAnchor& Anchor = Anchors[P];
		const float PlayerYawChange = FMath::FindDeltaAngleDegrees(Anchor.InitialPlayerYaw, P->GetActorRotation().Yaw);
		const float ProposalYaw = Anchor.InitialFurnitureYaw + PlayerYawChange;
		FVector ProposalLoc = P->GetActorLocation() + Anchor.InitialOffset.RotateAngleAxis(PlayerYawChange, FVector::UpVector);
		ProposalLoc.Z = CurFurnitureZ;

		// 가중치 = 이 플레이어가 가구를 현재 위치에서 얼마나 멀리 옮기려 하는가(병진+회전 레버암 모두 반영).
		//   예) A가 90도 돌리면 A의 제안 위치는 현재 가구에서 멀리 떨어짐 → 가중치 큼.
		//       B가 가만히 있으면 B의 제안 위치 ≈ 현재 가구 → 가중치 ≈ 0(Eps만 남음).
		//       => 최종 가구는 A의 제안 쪽으로 거의 100% 따라가고, B는 [3]단계에서 끌려온다.
		const float Demand = FVector(ProposalLoc.X - CurFurnitureLoc.X, ProposalLoc.Y - CurFurnitureLoc.Y, 0.0f).Size();
		const double W = Demand + Eps;

		WTotal += W;
		WLocSum += W * ProposalLoc;
		WSumSin += W * FMath::Sin(FMath::DegreesToRadians(ProposalYaw));
		WSumCos += W * FMath::Cos(FMath::DegreesToRadians(ProposalYaw));
	}

	// 각도(Yaw)는 그냥 숫자 평균하면 안 된다. 예: 350도와 10도의 '진짜' 평균은 0도인데
	// 숫자로 평균하면 180도(정반대)가 나온다. 그래서 각도를 (cos,sin) 화살표로 바꿔 더한 뒤
	// 다시 atan2로 각도를 복원한다 → 경계(360→0)를 안전하게 넘어간다.
	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(WSumSin, WSumCos));
	FVector TargetLoc = (WTotal > 0.0) ? (WLocSum / WTotal) : CurFurnitureLoc;
	TargetLoc.Z = CurFurnitureZ;

	// ---- 3. 가구 이동: 절대좌표 + sweep(벽 충돌). AddOffset 누적 금지(조건5) ----
	Owner->SetActorLocationAndRotation(TargetLoc, FRotator(0.0f, TargetYaw, 0.0f), true);
	FVector ActualLoc = Owner->GetActorLocation();
	const float ActualYaw = Owner->GetActorRotation().Yaw;

	// 각 플레이어가 가구에 대해 있어야 할 위치(앵커) 계산 헬퍼.
	auto AttachedTargetFor = [&](ACharacter* P, const FVector& FurnitureLoc) -> FVector
	{
		const FGrabAnchor& Anchor = Anchors[P];
		const float YawChange = FMath::FindDeltaAngleDegrees(Anchor.InitialFurnitureYaw, ActualYaw);
		const FVector RotatedOffset = Anchor.InitialOffset.RotateAngleAxis(YawChange, FVector::UpVector);
		FVector Target = FurnitureLoc - RotatedOffset; // 가구위치 - 상대오프셋 = 플레이어 이상위치
		Target.Z = P->GetActorLocation().Z; // 높이는 플레이어 자신의 중력/지면을 따른다.
		return Target;
	};

	// 각 플레이어가 유지해야 할 시선(Yaw) = 잡을때 시선 + 가구가 잡은 이후 실제 회전한 양.
	//   - 회전을 '구동한' 본인: 가구가 자기를 따라 돌았으므로 desired ≈ 현재 Yaw -> 보정 0(시선 자유).
	//   - 끌려가는 반대편: 가구 회전량만큼 시선을 돌려줘 가구를 계속 바라보게 한다(조건6).
	auto DesiredYawFor = [&](ACharacter* P) -> float
	{
		const FGrabAnchor& Anchor = Anchors[P];
		return Anchor.InitialPlayerYaw + FMath::FindDeltaAngleDegrees(Anchor.InitialFurnitureYaw, ActualYaw);
	};

	// 안전장치(자동 놓기): 보정 전 위치가 이상위치에서 너무 벌어졌는지 미리 확인.
	TArray<ACharacter*> ToRelease;
	const float MaxSepSq = FMath::Square(MaxGrabSeparationDistance);
	for (ACharacter* P : Players)
	{
		const FVector Att = AttachedTargetFor(P, ActualLoc);
		const FVector D = P->GetActorLocation() - Att;
		if (FVector(D.X, D.Y, 0.0f).SizeSquared() > MaxSepSq)
		{
			ToRelease.Add(P);
		}
	}

	// ---- 4. 끌려가는 사람이 벽에 막히면 가구를 그만큼 후퇴시켜 정지(조건3) ----
	if (bBlockedCarrierStopsFurniture)
	{
		FVector WorstBlock = FVector::ZeroVector;
		for (ACharacter* P : Players)
		{
			const FVector Att = AttachedTargetFor(P, ActualLoc);
			FHitResult Hit;
			P->SetActorLocation(Att, true, &Hit); // sweep: 벽이면 막혀서 못 감
			FVector Shortfall = Att - P->GetActorLocation();
			Shortfall.Z = 0.0f;
			if (Shortfall.SizeSquared() > WorstBlock.SizeSquared())
			{
				WorstBlock = Shortfall;
			}
		}

		// 가장 많이 막힌 양만큼 가구를 뒤로 물려서, 막힌 사람 기준으로 전체가 멈추게 한다.
		if (WorstBlock.SizeSquared() > FMath::Square(BlockStopThreshold))
		{
			ActualLoc -= WorstBlock;
			ActualLoc.Z = CurFurnitureZ;
			Owner->SetActorLocation(ActualLoc, false);
			ActualLoc = Owner->GetActorLocation();
		}
	}

	// ---- 5. 플레이어를 최종 이상위치/시선으로 보정하고 소유 클라이언트에 전달 ----
	const float MaxStep = MaxCorrectionSpeed * DeltaTime;
	for (ACharacter* P : Players)
	{
		const FVector Att = AttachedTargetFor(P, ActualLoc);
		const FVector PlayerLoc = P->GetActorLocation();

		FVector Correction(Att.X - PlayerLoc.X, Att.Y - PlayerLoc.Y, 0.0f);
		Correction = Correction.GetClampedToMaxSize(MaxStep);

		const float CurYaw = P->GetActorRotation().Yaw;
		const float DesiredYaw = DesiredYawFor(P);
		const float YawDelta = FMath::FindDeltaAngleDegrees(CurYaw, DesiredYaw);

		const bool bPos = Correction.SizeSquared() > FMath::Square(CorrectionDeadzone);
		const bool bYaw = FMath::Abs(YawDelta) > YawCorrectionDeadzone;

		// 둘 다 데드존이면(구동 본인/1인 캐리) RPC 도 안 나간다 -> 시선/이동 완전 자유.
		if (!bPos && !bYaw)
			continue;

		const FVector NewLoc = bPos ? (PlayerLoc + Correction) : PlayerLoc;
		const float NewYaw = bYaw ? DesiredYaw : CurYaw;

		// 서버 적용 (sweep=false: 이중 충돌검사로 인한 모서리 덜덜거림 차단)
		FRotator NewRot = P->GetActorRotation();
		NewRot.Yaw = NewYaw;
		P->SetActorLocationAndRotation(NewLoc, NewRot, false);
		if (UCharacterMovementComponent* CMC = P->GetCharacterMovement())
		{
			CMC->bJustTeleported = true; // 네트워크 스무딩이 보정을 실제 속도로 오인하지 않도록.
		}

		// 소유 클라이언트의 예측 무브먼트도 같은 절대 목표/시선으로 끌어준다.
		Multicast_ApplyPlayerCorrection(P, NewLoc, NewYaw);
	}

	// ---- 6. 안전장치 처리 ----
	for (ACharacter* P : ToRelease)
	{
		Release(P);
	}

	// ---- 7. 클라 보간용 단일 트랜스폼 갱신 ----
	ServerLocation = Owner->GetActorLocation();
	ServerRotation = Owner->GetActorRotation();
}

void UFurnitureGrabSystem::Multicast_ApplyPlayerCorrection_Implementation(ACharacter* Player, FVector TargetLocation, float TargetYaw)
{
	if (!Player)
		return;

	// 서버는 이미 직접 적용했으므로 중복 적용 방지.
	if (GetOwner() && GetOwner()->HasAuthority())
		return;

	// 자기 자신을 예측 조종 중인 소유 클라이언트만 보정한다.
	// (다른 클라의 캐릭터는 엔진 무브먼트 복제로 따라오므로 건드리지 않는다.)
	if (!Player->IsLocallyControlled())
		return;

	FRotator NewRot = Player->GetActorRotation();
	NewRot.Yaw = TargetYaw;
	Player->SetActorLocationAndRotation(TargetLocation, NewRot, false, nullptr, ETeleportType::TeleportPhysics);

	if (UCharacterMovementComponent* CMC = Player->GetCharacterMovement())
	{
		CMC->bJustTeleported = true;
	}

	// 무브먼트가 컨트롤러 회전으로 Yaw 를 덮어쓰는 설정에서도 시선이 유지되도록 컨트롤 회전도 맞춘다.
	if (AController* C = Player->GetController())
	{
		FRotator CR = C->GetControlRotation();
		CR.Yaw = TargetYaw;
		C->SetControlRotation(CR);
	}
}

void UFurnitureGrabSystem::UpdateClientInterpolation(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner)
		return;

	// 보간 출발점 초기화(잡힌 첫 프레임): 현재 화면상의 위치에서 시작해 튐을 막는다.
	if (!bHasClientInterpInit)
	{
		PreviousClientLoc = Owner->GetActorLocation();
		PreviousClientRot = Owner->GetActorRotation();
		bHasClientInterpInit = true;
	}

	const FVector EstimatedLoc = FMath::VInterpTo(PreviousClientLoc, ServerLocation, DeltaTime, ClientInterpSpeed);
	const FRotator EstimatedRot = FMath::RInterpTo(PreviousClientRot, ServerRotation, DeltaTime, ClientInterpSpeed);

	Owner->SetActorLocationAndRotation(EstimatedLoc, EstimatedRot, false);

	PreviousClientLoc = EstimatedLoc;
	PreviousClientRot = EstimatedRot;
}

void UFurnitureGrabSystem::OnRep_GrabbedPlayers()
{
	// 물리/충돌 프로파일을 잡힘 상태에 맞춘다(클라).
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

	// 더 이상 잡고 있지 않은 플레이어 정리.
	for (int32 i = ClientTrackedPlayers.Num() - 1; i >= 0; --i)
	{
		ACharacter* P = ClientTrackedPlayers[i];
		if (!P || !GrabbedPlayers.Contains(P))
		{
			if (P)
			{
				SetGrabCollisionState(P, false);
			}
			ClientTrackedPlayers.RemoveAt(i);
		}
	}

	// 새로 잡은 플레이어 셋업.
	for (ACharacter* P : GrabbedPlayers)
	{
		if (P && !ClientTrackedPlayers.Contains(P))
		{
			SetGrabCollisionState(P, true);
			ClientTrackedPlayers.Add(P);
		}
	}

	UpdateLocalWalkSpeed();
}

void UFurnitureGrabSystem::UpdateLocalWalkSpeed()
{
	if (!GetWorld())
		return;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ACharacter* LocalChar = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!LocalChar)
		return;

	UCharacterMovementComponent* CMC = LocalChar->GetCharacterMovement();
	if (!CMC)
		return;

	const bool bGrabbingNow = GrabbedPlayers.Contains(LocalChar);

	if (bGrabbingNow && !bLocalSpeedReduced)
	{
		LocalOriginalMaxWalkSpeed = CMC->MaxWalkSpeed;
		if (FurnitureStat)
		{
			CMC->MaxWalkSpeed = FurnitureStat->GetBaseSpeed();
		}
		bLocalSpeedReduced = true;
	}
	else if (!bGrabbingNow && bLocalSpeedReduced)
	{
		CMC->MaxWalkSpeed = LocalOriginalMaxWalkSpeed;
		bLocalSpeedReduced = false;
	}
}

void UFurnitureGrabSystem::SetGrabCollisionState(ACharacter* Player, bool bEnable)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Player)
		return;

	// 가구 <-> 잡은 캐릭터 상호 충돌 무시 (서로 밀어내며 떨리는 현상 방지).
	if (bEnable)
	{
		Player->MoveIgnoreActorAdd(Owner);
	}
	else
	{
		Player->MoveIgnoreActorRemove(Owner);
	}

	if (FurnitureMesh && Player->GetCapsuleComponent())
	{
		FurnitureMesh->IgnoreComponentWhenMoving(Player->GetCapsuleComponent(), bEnable);
		Player->GetCapsuleComponent()->IgnoreComponentWhenMoving(FurnitureMesh, bEnable);
	}

	// 틱 순서: 가구 이동 시스템이 캐릭터 무브먼트보다 항상 늦게 틱하도록 (절반갱신 위치 읽기 방지).
	if (UCharacterMovementComponent* CMC = Player->GetCharacterMovement())
	{
		if (bEnable)
		{
			AddTickPrerequisiteComponent(CMC);
		}
		else
		{
			RemoveTickPrerequisiteComponent(CMC);
		}
	}
}
