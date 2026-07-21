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
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "CatchCharacter/Furniture/FurnitureCarryShared.h"

// 운반 입력 유무 판정 — 원격 가속도는 ServerMove가 온 프레임에만 실려 사이 프레임엔 0이다
bool UFurnitureGrabSystem::HasCarryMoveInput(ACharacter* P, const UCharacterMovementComponent* CMC)
{
	const int32 Mode = GetCarryInputGateMode();
	if (Mode == 1)
	{
		return P && P->GetVelocity().SizeSquared2D() > FMath::Square(20.0f);
	}

	const bool bRawInput = CMC && CMC->GetCurrentAcceleration().SizeSquared2D() > FMath::Square(10.0f);
	if (Mode != 2 || !P)
	{
		return bRawInput;
	}

	const double Now = FPlatformTime::Seconds();
	if (bRawInput)
	{
		LastInputSeenTime.Add(P, Now);
		return true;
	}
	// 래치 창 — 네트워크 갱신 간격보다 넉넉하되 손 뗀 뒤 지연이 체감되지 않을 만큼 짧게
	const double* Last = LastInputSeenTime.Find(P);
	return Last && (Now - *Last) < 0.2;
}

// =====================================================================
// HandleMovement (서버 운반 틱) — 단계 함수 오케스트레이터
// =====================================================================

void UFurnitureGrabSystem::HandleMovement(float DeltaTime)
{
	FCarryDebugState::HeartBeat(GetOwner(), GrabbedPlayers.Num(),
		FurnitureStat != nullptr, GetOwner() && GetOwner()->HasAuthority());

	FGrabMoveContext Ctx;
	Ctx.DeltaTime = DeltaTime;

	if (!MovePrepare(Ctx))            // 0. 유효 플레이어 수집 + 드레인 + 속도 강제
		return;
	MoveUpdatePairLine(Ctx);          // 0.5 들것 선 회전 의도 누적
	MoveApplyAnchorShaping(Ctx);      // 0.7 거리 테더 + 0.8 정면 복원
	MoveComputeTarget(Ctx);           // 1~2 제안 가중 평균 → 목표 Yaw/위치
	MoveComputeHeight(Ctx);           // 2.5 피치 높이 + 끌림 자세 + 높이 하한
	MoveSweepFurniture(Ctx);          // 3 스윕 이동 + 관통 밸브 + 스텝업 + 회전 가드
	if (!MoveReconcileAnchors(Ctx))   // 3.5~4 막힘 재기록 + 벽 막힘 감지
		return;
	// [리쉬] 이동 봉인 상태를 복제 플래그로 — 클라 입력 필터가 반경을 동결한다
	bMoveConstrained = Ctx.bValveJammed || Ctx.bFurnitureStuck || bCarrierBlockedLastTick;
	MoveDrivePlayers(Ctx);            // 5 플레이어 견인/정지 속도 주입
	MoveFinalize(Ctx);                // 6~7 자동 해제 + 트랜스폼 브로드캐스트
}

bool UFurnitureGrabSystem::MovePrepare(FGrabMoveContext& Ctx)
{
	const float DeltaTime = Ctx.DeltaTime;

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
		return false;
	if (!FurnitureStat)
	{
		// 스탯 포인터 소실 = 운반 틱 전체가 조용히 죽는 치명 상태 — 스로틀 걸고 반드시 기록
		static double GLastNoStatLog = -10.0;
		const double NowT = FPlatformTime::Seconds();
		if (NowT - GLastNoStatLog > 2.0)
		{
			GLastNoStatLog = NowT;
			UE_LOG(LogCarry, Warning, TEXT("[운반틱] %s FurnitureStat=nullptr — 운반 이동 전체 불가"), *Owner->GetName());
		}
		return false;
	}
	// ---- 0. 유효 플레이어 수집 ----
	TArray<ACharacter*> Players;
	Players.Reserve(GrabbedPlayers.Num());
	for (ACharacter* P : GrabbedPlayers)
	{
		if (P && Anchors.Contains(P))
			Players.Add(P);
	}
	if (Players.Num() == 0)
		return false;
	const int32  N             = Players.Num();
	const FVector CurFurnLoc   = Owner->GetActorLocation();
	const float   CurFurnYaw   = Owner->GetActorRotation().Yaw;

	// 인원 미달(2인 가구 솔로 끌기) 판정 — Step 1의 회전 동결과 2.5의 끌림 자세가 함께 사용
	const int32 RequiredPlayers = FurnitureStat->GetRequiredPlayer();
	bool        bUnderManned    = (N == 1) && (RequiredPlayers >= 2);

	// [미달 운반 내구도 드레인] 인원 미달로 '끌며 이동하는 동안'만 초당 일정량 내구도 소모
	// (1초 단위 적용). 제자리에 서 있으면 소모되지 않는다. ApplyDamage 경로라 충돌 무적과 무관.
	if (bUnderManned && UnderMannedHealthDrainPerSec > 0.0f
		&& Players[0]->GetVelocity().SizeSquared2D() > FMath::Square(30.0f))
	{
		UnderMannedDrainAccum += DeltaTime;
		if (UnderMannedDrainAccum >= 1.0f)
		{
			UnderMannedDrainAccum -= 1.0f;
			UGameplayStatics::ApplyDamage(Owner, UnderMannedHealthDrainPerSec, nullptr, nullptr, nullptr);
			// 드레인으로 파괴되면 해제 콜백(TryInteract/AllRelease)이 이 자리에서 동기 실행되어
			// 앵커·그랩 목록이 비워진다 — 아래 코드가 빈 앵커를 읽지 않도록 이번 틱 즉시 중단
			if (GrabbedPlayers.Num() == 0 || !Anchors.Contains(Players[0]))
				return false;
		}
	}
	else
	{
		UnderMannedDrainAccum = 0.0f;
	}

	// [운반 속도 서버 강제] 외부에서 MaxWalkSpeed가 덮어써져도 서버가 매 틱 운반 감속을 재적용한다
	{
		const float CarrySpeed = ComputeCarrySpeed();
		for (ACharacter* P : Players)
		{
			if (UCharacterMovementComponent* CMC = P->GetCharacterMovement())
			{
				if (!FMath::IsNearlyEqual(CMC->MaxWalkSpeed, CarrySpeed))
				{
					CMC->MaxWalkSpeed = CarrySpeed;
				}
			}
		}
	}


	Ctx.Players      = MoveTemp(Players);
	Ctx.N            = N;
	Ctx.CurFurnLoc   = CurFurnLoc;
	Ctx.CurFurnYaw   = CurFurnYaw;
	Ctx.bUnderManned = bUnderManned;
	return true;
}

void UFurnitureGrabSystem::MoveUpdatePairLine(FGrabMoveContext& Ctx)
{
	const float DeltaTime  = Ctx.DeltaTime;
	const TArray<ACharacter*>& Players = Ctx.Players;
	const int32 N          = Ctx.N;
	const float CurFurnYaw = Ctx.CurFurnYaw;

	// ---- 0.5. [들것 회전] 2인 이상: 가구 목표 Yaw = '두 운반자를 잇는 선'의 회전 ----
	// 선 Yaw 변화량을 각자 기여로 분해해 능동(직접 걷는) 몫만 누적한다 — 피동 이동까지 포함하면
	// 회전→견인→선 회전의 폭주 피드백이 생긴다. 3인 이상은 앞의 두 명이 기준선.
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


	Ctx.bPairLine          = bPairLine;
	Ctx.PairLineIntentRate = PairLineIntentRate;
}

void UFurnitureGrabSystem::MoveApplyAnchorShaping(FGrabMoveContext& Ctx)
{
	AActor* Owner = GetOwner();
	const float DeltaTime    = Ctx.DeltaTime;
	const TArray<ACharacter*>& Players = Ctx.Players;
	const int32 N            = Ctx.N;
	const float CurFurnYaw   = Ctx.CurFurnYaw;
	const bool  bUnderManned = Ctx.bUnderManned;

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

	// ---- 0.8. [정면 복원] 1인 운반: 막힘·후퇴·재기록으로 옆으로 틀어진 XY 배치를
	// '몸 정면' 배치로 매 틱 서서히 되돌린다 (들것 대형이 있는 2인 이상은 제외)
	if (N == 1 && FurnitureMesh)
	{
		if (FGrabAnchor* Anc = Anchors.Find(Players[0]))
		{
			const FVector PLoc       = Players[0]->GetActorLocation();
			const FVector FurnCenter = FurnitureMesh->Bounds.Origin;
			// 몸통 '실제' 방향 기준 — 몸이 카메라 목표를 천천히 쫓으므로 가구도 호를 그리며
			// 따라온다. 카메라를 직접 쓰면 급회전 시 목표가 즉시 반대편으로 점프해 오프셋
			// 보간 경로가 몸 중심을 관통한다 (가구가 플레이어를 뚫고 지나가는 원인)
			const float   BodyYaw    = Players[0]->GetActorRotation().Yaw;
			const FVector FrontDir   = FRotator(0.0f, BodyYaw, 0.0f).Vector();
			// 목표 거리: 가구의 정면 방향 반폭 + 캡슐 여유 — 충돌로 밀려난 거리를 유지하지 않고
			// 몸 앞 선호 거리로 함께 회복한다. 끌림 자세(미달)는 가구가 기울어 다가오므로 여유를 크게.
			const FVector ExtF       = FurnitureMesh->Bounds.BoxExtent;
			const float   FrontHalf  = FMath::Abs(ExtF.X * FrontDir.X) + FMath::Abs(ExtF.Y * FrontDir.Y);
			const float   Margin     = bUnderManned ? 45.0f : 10.0f;
			const float   Preferred  = FMath::Clamp(35.0f + FrontHalf + Margin, 40.0f, 170.0f);
			// 캡슐이 가구 안으로 파고든 거리는 추적하지 않는다 — 침입 거리를 목표로 삼으면
			// 대형 목표(Att)까지 함께 파고들어 Step 5의 침범 밀어냄이 무력화된다
			const float   MinKeepOut = FMath::Min(35.0f + FrontHalf, Preferred);
			const float   Dist       = FMath::FInterpTo(
				FMath::Max(FVector::Dist2D(FurnCenter, PLoc), MinKeepOut), Preferred, DeltaTime, 2.0f);
			// 목표 오프셋(월드): 정면 × 거리 + 피벗-중심 보정. 저장 기준계(그랩 요)로 역회전.
			const FVector CenterToPivot = Owner->GetActorLocation() - FurnCenter;
			FVector DesiredWorld = FrontDir * Dist + FVector(CenterToPivot.X, CenterToPivot.Y, 0.0f);
			const float YC = FMath::FindDeltaAngleDegrees(Anc->InitialFurnitureYaw, CurFurnYaw);
			const FVector DesiredStored = DesiredWorld.RotateAngleAxis(-YC, FVector::UpVector);
			// [극좌표 접근] 성분(X/Y) 보간은 목표 방향이 크게 돌 때(급회전) 경로가 0점을
			// 지나 가구가 몸을 관통한다 — 방향은 회전, 거리는 보간으로 분리해 항상 호를 그린다
			const FVector2D CurOff(Anc->InitialOffset.X, Anc->InitialOffset.Y);
			const FVector2D DesOff(DesiredStored.X, DesiredStored.Y);
			if (CurOff.SizeSquared() > 1.0f && DesOff.SizeSquared() > 1.0f)
			{
				const float CurAng = FMath::RadiansToDegrees(FMath::Atan2(CurOff.Y, CurOff.X));
				const float DesAng = FMath::RadiansToDegrees(FMath::Atan2(DesOff.Y, DesOff.X));
				const float NewAng = FMath::DegreesToRadians(FMath::FixedTurn(CurAng, DesAng, 120.0f * DeltaTime));
				const float NewLen = FMath::FInterpTo(CurOff.Size(), DesOff.Size(), DeltaTime, 3.0f);
				Anc->InitialOffset.X = NewLen * FMath::Cos(NewAng);
				Anc->InitialOffset.Y = NewLen * FMath::Sin(NewAng);
			}
			else
			{
				const float A = FMath::Clamp(DeltaTime * 3.0f, 0.0f, 1.0f);
				Anc->InitialOffset.X = FMath::Lerp(Anc->InitialOffset.X, DesiredStored.X, A);
				Anc->InitialOffset.Y = FMath::Lerp(Anc->InitialOffset.Y, DesiredStored.Y, A);
			}

			// 테더·정면 복원이 매 틱 다듬는 오프셋은 이벤트 멀티캐스트(그랩·리셋)에 안 실려
			// 클라 리쉬 부착점이 수십 uu 어긋난다 — 소유 클라 입력 필터가 전진을 벽처럼 깎는
			// 원인이라 0.5초마다 재동기화 (Reliable RPC 과다 방지 스로틀)
			if (GFrameCounter % 30 == 0)
			{
				Multicast_SetPlayerAnchor(Players[0], Anc->InitialFurnitureYaw,
					Anc->InitialPlayerYaw, Anc->InitialAimYaw, Anc->InitialOffset);
			}
		}
	}

}

void UFurnitureGrabSystem::MoveComputeTarget(FGrabMoveContext& Ctx)
{
	const float DeltaTime    = Ctx.DeltaTime;
	const TArray<ACharacter*>& Players = Ctx.Players;
	const int32 N            = Ctx.N;
	const FVector CurFurnLoc = Ctx.CurFurnLoc;
	const float CurFurnYaw   = Ctx.CurFurnYaw;
	const bool  bPairLine    = Ctx.bPairLine;
	const bool  bUnderManned = Ctx.bUnderManned;

	CarryDbg.TrackInputGate(Players, N,
		[this](ACharacter* P, const UCharacterMovementComponent* CMC) { return HasCarryMoveInput(P, CMC); });

	// ---- 1. 각 플레이어의 "내가 주도한다면 가구는 여기" 제안 + 활동량 가중치 계산 ----
	//   활동량 = 현재 가구 위치에서 제안 위치까지의 거리. 더 많이 움직인 사람이 더 큰 가중치.
	//   피동(끌려가는) 플레이어는 가중치 0: 뒤처진 피동 플레이어의 제안이 가구를 역방향으로 당기는 것을 방지.
	const float Eps = 0.01f;
	TArray<double> Weights;
	Weights.Init(0.0, N);
	TArray<float> ProposalYaws;        // 스무딩 후 방향 합을 다시 계산하기 위해 보관
	ProposalYaws.Init(0.0f, N);
	double WTotal = 0.0, WSumSin = 0.0, WSumCos = 0.0;

	for (int32 i = 0; i < N; ++i)
	{
		ACharacter* P = Players[i];
		if (DraggedLastTick.Contains(P) || StoppedDraggingLastTick.Contains(P))
			continue;  // Weights[i] = 0 (이미 초기화됨)

		const FGrabAnchor& Anc           = Anchors[P];
		const float        PlayerAimYaw   = P->GetBaseAimRotation().Yaw;
		// [들것 회전 — 걷기 기반] 2인 이상은 카메라가 아니라 '두 운반자를 잇는 선'의 회전(0.5 누적)만 반영.
		// 솔로 끌기(인원 미달)와 들것 설정 꺼짐(N≥2)은 회전 동결 — 끌기는 몸으로 끌어 방향을 잡는다.
		float              PlayerYawDelta = bPairLine
			? FMath::FindDeltaAngleDegrees(Anc.InitialFurnitureYaw, PairLineTargetYaw)
			: ((N >= 2 || bUnderManned)
				? 0.0f
				: FMath::FindDeltaAngleDegrees(Anc.InitialAimYaw, PlayerAimYaw));
		// [선행 제한] 1인 카메라 회전 의도는 실제 가구 회전 진행보다 ±45°까지만 앞서게 —
		// 전량 반영하면 급회전 시 제안 위치가 반대편으로 점프해 가구가 몸을 가로질러 관통한다.
		// 제한하면 가구가 호를 그리며 따라온다 (들것 PairLineTargetYaw의 ±45 선행 제한과 동일 원리)
		if (!bPairLine && N == 1 && !bUnderManned)
		{
			const float ActualDelta = FMath::FindDeltaAngleDegrees(Anc.InitialFurnitureYaw, CurFurnYaw);
			const float Lead        = FMath::FindDeltaAngleDegrees(ActualDelta, PlayerYawDelta);
			PlayerYawDelta = ActualDelta + FMath::Clamp(Lead, -45.0f, 45.0f);
		}
		const float        ProposalYaw   = Anc.InitialFurnitureYaw + PlayerYawDelta;
		const FVector      ProposalLoc   = P->GetActorLocation() + Anc.InitialOffset.RotateAngleAxis(PlayerYawDelta, FVector::UpVector);
		float              Demand        = FVector(ProposalLoc.X - CurFurnLoc.X, ProposalLoc.Y - CurFurnLoc.Y, 0.0f).Size();
		// [견인 데드존 보완] 데드존 안에 서 있는 플레이어의 낡은 앵커 제안이 가구를 뒤로 당기는
		// 브레이크가 된다 — 자기 이동량 이상의 발언권을 주지 않는다(서면 최소, 걸으면 회복).
		// [들것 조향] 이동 입력 중인 운반자는 캡을 풀어 가구 위치를 전담시킨다.
		{
			const UCharacterMovementComponent* WCMC = P->GetCharacterMovement();
			const bool bSteering = bPairLine && HasCarryMoveInput(P, WCMC);
			if (!bSteering)
			{
				Demand = FMath::Min(Demand, P->GetVelocity().Size2D() * DeltaTime * 4.0f + 1.0f);
			}
		}
		const double       W             = Demand + Eps;

		Weights[i]      = W;
		ProposalYaws[i] = ProposalYaw;
		WTotal         += W;
	}

	// [가중치 비율 스무딩] 가중치가 오차 크기라 가구가 가까워지면 발언권을 잃어 자기발진이 된다.
	// 크기가 아니라 비율만 감쇠 — 같은 방향일 때는 비율이 안 변해 지연이 없고, 총량은 원본 복원.
	if (const float Tau = GetCarryWeightSmoothTau(); Tau > 0.0f && WTotal > 0.0)
	{
		const double Alpha    = 1.0 - FMath::Exp(-DeltaTime / Tau);
		const double RawTotal = WTotal;
		double ShareTotal = 0.0;
		// 피동 판정(가중치 0)으로의 전환도 함께 감쇠 — 여기서 끊으면 목표가 한 틱에 튄다.
		for (int32 i = 0; i < N; ++i)
		{
			const double Share = Weights[i] / RawTotal;
			double& Prev = SmoothedWeight.FindOrAdd(Players[i], Share);
			Prev = FMath::Lerp(Prev, Share, Alpha);
			Weights[i]  = Prev;
			ShareTotal += Prev;
		}
		if (ShareTotal > 0.0)
		{
			for (int32 i = 0; i < N; ++i)
			{
				Weights[i] = Weights[i] / ShareTotal * RawTotal;
			}
		}
	}

	// 방향 합은 최종 가중치로 계산 — 스무딩 결과가 회전 교착 판정에도 반영되도록
	WTotal = 0.0;
	for (int32 i = 0; i < N; ++i)
	{
		WTotal   += Weights[i];
		WSumSin  += Weights[i] * FMath::Sin(FMath::DegreesToRadians(ProposalYaws[i]));
		WSumCos  += Weights[i] * FMath::Cos(FMath::DegreesToRadians(ProposalYaws[i]));
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
	// [회전 교착] 제안 방향 합벡터 크기로 일치도를 재고, 의견이 갈리면 회전하지 않는다(히스테리시스).
	// 일치도 0에서 Atan2를 쓰면 Yaw가 붕괴해 몸통·시선까지 틀어진다.
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

	// [운반자 막힘 → 회전 보류] 낀 사람을 두고 가구만 돌면 두 사람의 앵커 기준 시점이 어긋나
	// 제안 방향이 불일치해 제어불능이 된다. FixedTurn으로 틱당 회전량도 제한.
	const float TargetYawRaw = (bYawStalemate || bCarrierBlockedLastTick)
		? CurFurnYaw
		: FMath::RadiansToDegrees(FMath::Atan2(WSumSin, WSumCos));
	// [회전 속도] 들것·1인 카메라 회전은 상한 2배 — 궤도(위치) 회전의 선 각속도를
	// 가구 Yaw가 따라잡게 한다 (1인은 몸 회전 대비 가구가 눈에 띄게 굼떠지는 것 방지)
	const float TargetYaw    = FMath::FixedTurn(CurFurnYaw, TargetYawRaw,
		FurnYawRotationSpeed * ((bPairLineValid || N == 1) ? 2.0f : 1.0f) * DeltaTime);

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

	CarryDbg.TrackTarget(Weights, N, WTotal, TargetLoc);


	Ctx.TargetYaw = TargetYaw;
	Ctx.TargetLoc = TargetLoc;
}

void UFurnitureGrabSystem::MoveComputeHeight(FGrabMoveContext& Ctx)
{
	AActor* Owner = GetOwner();
	const float DeltaTime    = Ctx.DeltaTime;
	const TArray<ACharacter*>& Players = Ctx.Players;
	const int32 N            = Ctx.N;
	const FVector CurFurnLoc = Ctx.CurFurnLoc;
	const bool  bUnderManned = Ctx.bUnderManned;
	const FVector TargetLoc  = Ctx.TargetLoc;

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

	// [인원 미달 드래그 연출] 혼자 들면 잡은 쪽만 들리고 반대쪽이 바닥에 끌리게 중심을 낮춘다.
	// 45°+ 기울어 놓인 가구는 제외하고 일반 들기로 운반.
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
			// 기울어진 가구 바닥이 발밑에 닿는 중심 높이를 역산. BoxExtent는 이미 회전이 반영된
			// 월드 AABB라 기울기 성분을 또 더하면 이중 가산으로 공중에 뜬다.
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
				// 복합 콜리전까지 지지면으로 인정 — 심플 콜리전 없는 그레이박스 바닥(모델링 툴 메시) 대응
				FCollisionQueryParams DropParams(SCENE_QUERY_STAT(UnderMannedDrop), true, Owner);
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
					// 동적(스탠드·다른 가구)도 지지물로 인정 — 정적 전용이면 스탠드 위 가구를
					// 바닥 지지로 오판해 하강 목표가 스탠드를 뚫고 내려가 밸브가 연발한다
					FCollisionObjectQueryParams StaticObj(ECC_WorldStatic);
					StaticObj.AddObjectTypesToQuery(ECC_WorldDynamic);
					StaticObj.AddObjectTypesToQuery(ECC_PhysicsBody);
					FHitResult PlateHit;
					if (SweepStart.Z > SweepEnd.Z && GetWorld()->SweepSingleByObjectType(
							PlateHit, SweepStart, SweepEnd, FQuat::Identity, StaticObj, Plate, DropParams)
						&& !PlateHit.bStartPenetrating
						&& PlateHit.ImpactNormal.Z > 0.7f)
					{
						FloorZ = FMath::Max(FloorZ, PlateHit.Location.Z - 2.0f);
						// 풋프린트 바로 아래 지지물이 높으면 모드 판정에도 반영 —
						// 스탠드 위에 걸쳐 있는 동안은 끌림 자세 대신 일반 들기로 뽑는다
						FloorZGate = FMath::Max(FloorZGate, PlateHit.Location.Z - 2.0f);
					}
				}
			}
			// [열린 공간 전용] 끌림 자세는 지지면이 운반자 발밑과 비슷한 높이일 때만 발동한다 (중심점 지지면 기준)
			if (FloorZGate <= CarrierFootZ + 40.0f)
			{
				const float CenterOffZ = FurnitureMesh ? (FurnitureMesh->Bounds.Origin.Z - Owner->GetActorLocation().Z) : 0.0f;
				// +12: 바닥에서 살짝 띄운 유격 — 끌림 자세는 유지하되 바닥과 접촉하지 않아
				// 긁힘 데미지·모서리 박힘이 발생하지 않는다 (혼자 드는 부담은 내구도 드레인이 담당).
				// 중심 절대 상한(발밑+100): 기울어진 대형 가구가 통째로 솟는 것을 제한
				const float DesiredCenterZ = FMath::Min(FloorZ + Ext.Z + 12.0f, CarrierFootZ + 100.0f);
				TargetHeightOffset = DesiredCenterZ - (TargetLoc.Z + CenterOffZ);
			}
			else
			{
				UnderMannedTilt = 0.0f;   // 높은 곳에서 뽑는 중 — 기울기·하강 없이 일반 운반
				// 지지물 위 +12uu 유격을 하한으로 — 파묻힌 채 끌려 밸브가 연발하는 것 방지
				const float CenterOffZ = FurnitureMesh
					? (FurnitureMesh->Bounds.Origin.Z - Owner->GetActorLocation().Z) : 0.0f;
				TargetHeightOffset = FMath::Max(TargetHeightOffset,
					(FloorZ + Ext.Z + 12.0f) - (TargetLoc.Z + CenterOffZ));
			}
		}
	}

	// [요건 충족 시 최소 운반 높이] 인원 충족 시 가구 하단이 '평균 발밑+20' 위로 오도록 높이 하한을 끌어올린다.
	// '같이 내려놓기'는 전원이 -25° 이하를 보고 + 가구가 실제로 들려 있을 때만 허용한다
	bool bAllLookingDown = false;
	bool bLoweringTogether = false;
	float WorstAimPitch = 0.0f;   // 진단: 전원 중 가장 위를 보는 피치
	if (!bUnderManned && FurnitureMesh)
	{
		// 전원 AND 판정 — 원격 피치는 양자화 복제라 임계 근처에서 흔들린다. 모드 2가 이력으로 흡수.
		const int32 RelaxMode  = GetCarryLookDownRelaxMode();
		const float Threshold  = (RelaxMode == 2 && bLookDownRelaxLatched) ? -15.0f : -25.0f;

		bool bAllDown = true;
		for (int32 i = 0; i < N; ++i)
		{
			const float Pitch = FoldAimPitch(Players[i]->GetBaseAimRotation().Pitch);
			WorstAimPitch = (i == 0) ? Pitch : FMath::Max(WorstAimPitch, Pitch);
			if (Pitch > Threshold)
				bAllDown = false;
		}
		if (bAllDown != bLookDownRelaxLatched)
		{
			bLookDownRelaxLatched = bAllDown;
			CarryDbg.OnLookDownFlip();
		}
		bAllLookingDown = (RelaxMode == 0) ? false : bAllDown;
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

	if (!bUnderManned && FurnitureMesh)
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
		// 내려놓는 중에는 발밑 호버 하한을 풀고 아래 지지면 접지 하한만 남긴다 — 목표가 지지면
		// 아래로 내려가지 않아 심플 콜리전 없는 바닥에서도 파묻히지 않는다 (물리 스윕 의존 제거)
		float MinCenterZ = bLoweringTogether ? -FLT_MAX : AvgFootZ + 20.0f + ExtNow.Z;
		float HitFloorZ  = FLT_MAX;   // 진단: 이번 틱 지지면 히트 Z (미탐지면 FLT_MAX)
		// [가구 밑 바닥 체크] 발밑 기준만으론 가구 아래 단차·지지물을 모른다 — 실제 지지면 위
		// 12uu 유격을 보장해 바닥에 눌린 채 스윕이 막히는 것(끌기·들기 불능)을 방지
		{
			const FVector BO = FurnitureMesh->Bounds.Origin;
			// 복합 콜리전까지 지지면으로 인정 — 심플 없는 그레이박스 바닥 대응
			FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(CarryFloorCheck),
				IsCarryFloorComplexEnabled(), Owner);
			for (ACharacter* P : Players)
				FloorParams.AddIgnoredActor(P);
			// 풋프린트 판 하향 스윕 — 점 트레이스는 가구가 지지물에 이미 파묻힌 경우 시작점이
			// 표면 아래라 지지물을 놓친다. 바닥+60에서 시작해 얹힌/파묻힌 지지물도 감지 (동적 포함)
			const FCollisionShape Plate = FCollisionShape::MakeBox(
				FVector(ExtNow.X * 0.9f, ExtNow.Y * 0.9f, 2.0f));
			const FVector SweepStart(BO.X, BO.Y, BO.Z - ExtNow.Z + 60.0f);
			// 내려놓기는 발밑 아래 단차(낮은 지지면)까지 탐색을 연장한다
			const FVector SweepEnd(BO.X, BO.Y, AvgFootZ - (bLoweringTogether ? 300.0f : 50.0f));
			FCollisionObjectQueryParams FloorObj(ECC_WorldStatic);
			FloorObj.AddObjectTypesToQuery(ECC_WorldDynamic);
			FloorObj.AddObjectTypesToQuery(ECC_PhysicsBody);
			FHitResult FloorHit;
			if (SweepStart.Z > SweepEnd.Z && GetWorld()->SweepSingleByObjectType(
					FloorHit, SweepStart, SweepEnd, FQuat::Identity, FloorObj, Plate, FloorParams)
				&& !FloorHit.bStartPenetrating
				&& FloorHit.ImpactNormal.Z > 0.7f)
			{
				// 내려놓기는 접지(+2), 평상시는 유격(+12)
				const float Clearance = bLoweringTogether ? 2.0f : 12.0f;
				MinCenterZ = FMath::Max(MinCenterZ, FloorHit.Location.Z - 2.0f + ExtNow.Z + Clearance);
				HitFloorZ  = FloorHit.Location.Z;
			}
		}
		CarryDbg.TrackFloor(HitFloorZ);

		if (MinCenterZ > -FLT_MAX * 0.5f)
		{
			const float NeededOffset = MinCenterZ - (TargetLoc.Z + CenterOff);
			TargetHeightOffset = FMath::Max(TargetHeightOffset, NeededOffset);
		}
	}

	// 폭주 안전핀: 정상 시나리오(피치 ±270 · 호버/접지 하한 ≤ ~400)를 넘는 목표는 버그 신호 —
	// 앵커 재기록 랠리 등이 만드는 무한 증식을 여기서 끊는다
	TargetHeightOffset = FMath::Clamp(TargetHeightOffset, -500.0f, 500.0f);

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
		// 인원 미달 드래그 자세·같이 내려놓기(전원 내려다봄)는 바닥 접지까지 하한 완화.
		// 완화는 '들림 게이트'가 아니라 '내려다봄 의도' 기준 — 게이트는 바닥 근처에서 매 틱
		// 깜빡이므로 게이트 기준 완화 해제는 즉시 스냅 상승(가구 튐)을 만든다
		const float MinZ = AvgPlayerZ + FurnitureHeightMin
			- ((bUnderManned || bAllLookingDown) ? 250.0f : 0.0f);
		// 상한은 '메시 하단 ≤ 플레이어+Max' 기준 — 세로로 길거나 높이 놓인 가구도 들 수 있게 한다
		const float MaxCenterZ = AvgPlayerZ + FurnitureHeightMax
			+ (FurnitureMesh ? FurnitureMesh->Bounds.BoxExtent.Z : 0.0f);
		const float ClampedCenterZ = FMath::Clamp(DesiredCenterZ, MinZ, MaxCenterZ);
		// 윈드업 방지: 범위 밖 입력이 계속 쌓이지 않도록, 실제 적용 가능한 오프셋으로 되돌려 저장
		CurrentHeightOffset = (ClampedCenterZ - MeshCenterOffZ) - TargetLoc.Z;

		CarryDbg.TrackHeightOffset(CurrentHeightOffset);
		CarryDbg.ReportJitter(Owner, N, bAllLookingDown, WorstAimPitch);

		CarryDbg.LogHeight(N, bUnderManned, bUprightEnough,
			FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
				Owner->GetActorQuat().GetUpVector().Z, -1.0f, 1.0f))),
			FRotator::NormalizeAxis(Players[0]->GetBaseAimRotation().Pitch),
			TargetHeightOffset, CurrentHeightOffset, ClampedCenterZ, MinZ, MaxCenterZ);
	}


	Ctx.TargetHeightOffset = TargetHeightOffset;
	Ctx.PairHandHeight0    = PairHandHeight0;
	Ctx.PairHandHeight1    = PairHandHeight1;
	Ctx.bUprightEnough     = bUprightEnough;
	Ctx.UnderMannedTilt    = UnderMannedTilt;
	Ctx.UnderMannedDir     = UnderMannedDir;
}

void UFurnitureGrabSystem::MoveSweepFurniture(FGrabMoveContext& Ctx)
{
	AActor* Owner = GetOwner();
	const float DeltaTime        = Ctx.DeltaTime;
	const TArray<ACharacter*>& Players = Ctx.Players;
	const int32 N                = Ctx.N;
	const bool  bPairLine        = Ctx.bPairLine;
	const bool  bUnderManned     = Ctx.bUnderManned;
	const float TargetYaw        = Ctx.TargetYaw;
	const FVector TargetLoc      = Ctx.TargetLoc;
	const float PairHandHeight0  = Ctx.PairHandHeight0;
	const float PairHandHeight1  = Ctx.PairHandHeight1;
	const bool  bUprightEnough   = Ctx.bUprightEnough;
	const float UnderMannedTilt  = Ctx.UnderMannedTilt;
	const FVector UnderMannedDir = Ctx.UnderMannedDir;

	// ---- 3. 가구 이동 (sweep=true, 가구 자체 충돌) ----
	// Pitch/Roll은 유지 — 쓰러진 가구를 억지로 세우면 바닥을 파고든다.
	// Yaw는 .Yaw 대입이 아니라 월드 Z축 쿼터니언 델타로 — 짐벌 자세에서 비-요 성분이 섞인다.
	const FQuat SoloYawDeltaQ(FVector::UpVector, FMath::DegreesToRadians(
		FMath::FindDeltaAngleDegrees(Owner->GetActorRotation().Yaw, TargetYaw)));
	FRotator TargetRot = (SoloYawDeltaQ * Owner->GetActorQuat()).Rotator();

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
	FVector DesiredPos = TargetLoc + FVector(0.0f, 0.0f, CurrentHeightOffset);

	// [목표 이동 상한] 2인 동시 입력으로 제안이 어긋나면 거리 비례 가중 평균이 틱마다
	// 반대편으로 출렁여 가구가 달달거린다 — 한 틱 XY 이동량을 운반 속도 기준으로 제한해
	// 평형점으로 수렴시킨다 (정상 주행의 틱당 이동량은 상한보다 훨씬 작아 영향 없음)
	{
		const FVector CurLoc = Owner->GetActorLocation();
		FVector StepXY(DesiredPos.X - CurLoc.X, DesiredPos.Y - CurLoc.Y, 0.0f);
		const float MaxStep = (ComputeCarrySpeed() * 1.5f + 200.0f) * DeltaTime;
		if (MaxStep > 0.0f && StepXY.SizeSquared() > FMath::Square(MaxStep))
		{
			StepXY = StepXY.GetClampedToMaxSize(MaxStep);
			DesiredPos.X = CurLoc.X + StepXY.X;
			DesiredPos.Y = CurLoc.Y + StepXY.Y;
		}
	}

	// [리쉬 대칭 클램프] 가구도 모든 운반자의 리쉬 안에 묶는다 — 한 명이 서 있으면 그 리쉬
	// 끝에서 가구가 멈추고, 걷는 쪽의 대형 지점도 동결돼 입력 필터가 걷기를 차단한다
	// (서 있는 운반자를 끌고 가거나 대형이 자동 해제 거리까지 벌어지는 것 자체를 방지)
	if (bLeashMovement)
	{
		// GetCarryLeash의 유효 반경과 일치 + 소여유. 보정은 원본 기준 동시 계산 후 합산 —
		// 축차 적용하면 앞사람 보정이 뒷사람 위반을 키워 한쪽으로 쏠린다.
		const float LimitR = FMath::Max(LeashRadius - 25.0f, 20.0f) + 3.0f;
		FVector TotalFix = FVector::ZeroVector;
		for (ACharacter* P : Players)
		{
			const FGrabAnchor* Anc = Anchors.Find(P);
			if (!Anc)
				continue;
			const float   YC  = FMath::FindDeltaAngleDegrees(Anc->InitialFurnitureYaw, TargetYaw);
			const FVector Off = Anc->InitialOffset.RotateAngleAxis(YC, FVector::UpVector);
			FVector ToPlayer(P->GetActorLocation().X - (DesiredPos.X - Off.X),
			                 P->GetActorLocation().Y - (DesiredPos.Y - Off.Y), 0.0f);
			const float Gap = ToPlayer.Size();
			if (Gap > LimitR)
			{
				TotalFix += ToPlayer.GetSafeNormal() * (Gap - LimitR);
			}
		}
		DesiredPos.X += TotalFix.X;
		DesiredPos.Y += TotalFix.Y;
	}

	const FRotator PreMoveRot = Owner->GetActorRotation();   // 회전 관통 롤백·보정 기준
	// 피벗-중심 보정용: 이동 전 메시 중심의 로컬 오프셋 캡처 (스케일 포함)
	const FVector PreLocalCenter = FurnitureMesh
		? PreMoveRot.Quaternion().Inverse().RotateVector(
			FurnitureMesh->Bounds.Origin - Owner->GetActorLocation())
		: FVector::ZeroVector;

	bool bValveJammed = false;   // 밸브 탈출 실패(낀 상태 지속) — Step 5에서 운반자 이동 봉인
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
		FCollisionObjectQueryParams UnstickObj(ECC_WorldStatic);
		UnstickObj.AddObjectTypesToQuery(ECC_WorldDynamic);
		UnstickObj.AddObjectTypesToQuery(ECC_PhysicsBody);

		// 1차 탈출은 수직(위) — 무조건적 수평 텔레포트는 얇은 벽을 건너뛰어 벽 통과 악용이 된다.
		TArray<FOverlapResult> StuckOverlaps;
		const FVector PreUnstickLoc = Owner->GetActorLocation();
		bool bUnstuck = false;
		for (int32 Step = 0; Step < 12; ++Step)
		{
			StuckOverlaps.Reset();
			if (!GetWorld()->ComponentOverlapMulti(StuckOverlaps, FurnitureMesh,
				FurnitureMesh->GetComponentLocation(),
				FurnitureMesh->GetComponentQuat(), UnstickParams, UnstickObj))
			{
				bUnstuck = true;
				break;
			}
			Owner->AddActorWorldOffset(FVector(0.0f, 0.0f, 6.0f), false);
		}
		// 2차: 수직 실패(위가 막힌 틈새·선반 밑) → 원위치 후 운반자 방향 수평 탈출.
		// 프로브 스윕이 확인한 첫 장애물 앞까지만 이동해 얇은 벽 터널링 악용을 차단한다
		if (!bUnstuck && Players.Num() > 0)
		{
			Owner->SetActorLocation(PreUnstickLoc, false);
			FVector EscDir = Players[0]->GetActorLocation() - FurnitureMesh->Bounds.Origin;
			EscDir.Z = 0.0f;
			if (EscDir.Normalize())
			{
				const FVector ProbeStart = FurnitureMesh->Bounds.Origin;
				float MaxEsc = 72.0f;
				FHitResult ProbeHit;
				if (GetWorld()->SweepSingleByChannel(ProbeHit, ProbeStart, ProbeStart + EscDir * 84.0f,
						FQuat::Identity, ECC_Visibility, FCollisionShape::MakeBox(FVector(5.0f)), UnstickParams))
				{
					MaxEsc = FMath::Min(MaxEsc, FMath::Max(ProbeHit.Distance - 6.0f, 0.0f));
				}
				for (float Moved = 6.0f; Moved <= MaxEsc; Moved += 6.0f)
				{
					Owner->AddActorWorldOffset(EscDir * 6.0f, false);
					StuckOverlaps.Reset();
					if (!GetWorld()->ComponentOverlapMulti(StuckOverlaps, FurnitureMesh,
						FurnitureMesh->GetComponentLocation(),
						FurnitureMesh->GetComponentQuat(), UnstickParams, UnstickObj))
					{
						bUnstuck = true;
						break;
					}
				}
			}
		}
		if (bUnstuck)
		{
			Owner->SetActorLocationAndRotation(DesiredPos, TargetRot, true);
		}
		else
		{
			// 탈출 실패(낀 상태 지속) — 원위치 유지하고 운반자 이동도 봉인해
			// 벽에 비벼 관통시키는 악용을 차단한다 (Step 5에서 입력 면제 해제)
			Owner->SetActorLocation(PreUnstickLoc, false);
			bValveJammed = true;

			// 겹침 상대가 물리 가구(위에 얹힘 등)면 무적을 걸고 살짝 밀어내 겹침을 해소한다 —
			// 수직 탈출만으론 위에 얹힌 상대를 벗어날 수 없어 잼이 지속된다
			for (const FOverlapResult& Ov : StuckOverlaps)
			{
				UPrimitiveComponent* OvComp = Ov.GetComponent();
				AActor* OvActor = Ov.GetActor();
				if (!OvComp || !OvActor || OvActor == Owner || !OvComp->IsSimulatingPhysics())
					continue;
				if (UFurnitureDamage* OvDmg = OvActor->FindComponentByClass<UFurnitureDamage>())
					OvDmg->SetInvincible(2.0f);
				if (FBodyInstance* BI = OvComp->GetBodyInstance())
					BI->SetMaxDepenetrationVelocity(120.0f);
				FVector Away = OvComp->Bounds.Origin - FurnitureMesh->Bounds.Origin;
				Away.Z = FMath::Max(Away.Z, 20.0f);
				OvComp->WakeAllRigidBodies();
				OvComp->AddImpulse(Away.GetSafeNormal() * 150.0f, NAME_None, true);
			}
		}
		if (IsCarryDebugEnabled())
		{
			// 연속 발동(파고듦 루프)은 0.5초 스로틀 + 누적 횟수로 기록, 겹친 상대를 함께 남긴다
			static double GLastValveLogTime = -10.0;
			static int32  GValveCountSinceLog = 0;
			++GValveCountSinceLog;
			const double NowT = FPlatformTime::Seconds();
			if (NowT - GLastValveLogTime > 0.5)
			{
				GLastValveLogTime = NowT;
				const AActor* StuckIn = (StuckOverlaps.Num() > 0) ? StuckOverlaps[0].GetActor() : nullptr;
				UE_LOG(LogCarry, Log, TEXT("[밸브] %s 시작 관통 → %s (0.5초간 %d회, 겹침=%s)"),
					*Owner->GetName(), bUnstuck ? TEXT("상향 탈출") : TEXT("탈출 실패·원위치"),
					GValveCountSinceLog, StuckIn ? *StuckIn->GetName() : TEXT("-"));
				GValveCountSinceLog = 0;
			}
		}
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
		}
	}

	// [가구 전진 막힘] 스윕·밸브·스텝업까지 끝난 실제 위치가 목표 XY에 크게 못 미치고,
	// 막힘 노멀이 요구 방향과 정면 반대(벽에 밀어붙이기)일 때만 Step 5에서 입력자의 견인 면제를
	// 풀어 이동을 봉인한다. 바닥 접촉(상향 노멀)·측면 스침(선반에서 비스듬히 빼기)은 봉인 대상이 아니다
	bool bFurnitureStuck = false;
	if (FVector::DistSquared2D(Owner->GetActorLocation(), DesiredPos) > FMath::Square(30.0f))
	{
		const FVector DemandXY = FVector(DesiredPos.X - Owner->GetActorLocation().X,
			DesiredPos.Y - Owner->GetActorLocation().Y, 0.0f).GetSafeNormal();
		const FVector BlockNXY = FVector(MoveHit.ImpactNormal.X, MoveHit.ImpactNormal.Y, 0.0f).GetSafeNormal();
		bFurnitureStuck = MoveHit.bStartPenetrating
			|| (MoveHit.IsValidBlockingHit() && MoveHit.ImpactNormal.Z <= 0.7f
				&& FVector::DotProduct(BlockNXY, DemandXY) < -0.5f);
		if (bFurnitureStuck && IsCarryDebugEnabled())
		{
			// 봉인 진단 (0.5초 스로틀)
			static double GLastStuckLog = -10.0;
			const double NowT = FPlatformTime::Seconds();
			if (NowT - GLastStuckLog > 0.5)
			{
				GLastStuckLog = NowT;
				UE_LOG(LogCarry, Log, TEXT("[가구막힘] %s 부족XY=%.0f 관통=%d 노멀Z=%.2f dot=%.2f"),
					*Owner->GetName(), FVector::Dist2D(Owner->GetActorLocation(), DesiredPos),
					MoveHit.bStartPenetrating ? 1 : 0, MoveHit.ImpactNormal.Z,
					FVector::DotProduct(BlockNXY, DemandXY));
			}
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
		// 동적(스탠드·다른 가구)도 포함 — 끌림 기울기가 옆 가구에 파고들면 다음 틱 밸브 연발이 된다
		FCollisionObjectQueryParams RotObj(ECC_WorldStatic);
		RotObj.AddObjectTypesToQuery(ECC_WorldDynamic);
		RotObj.AddObjectTypesToQuery(ECC_PhysicsBody);
		TArray<FOverlapResult> RotOverlaps;
		const bool bPenetrates = GetWorld()->ComponentOverlapMulti(
			RotOverlaps, FurnitureMesh,
			FurnitureMesh->GetComponentLocation() + FVector(0.f, 0.f, 3.f),
			FurnitureMesh->GetComponentQuat(), RotOverlapParams, RotObj);
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
					FurnitureMesh->GetComponentQuat(), RotOverlapParams, RotObj))
				{
					bResolved = true;
					break;
				}
			}
			if (!bResolved)
			{
				Owner->SetActorRotation(PreMoveRot);
			}
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

				// [피벗보정 진단] 보정 발동량 추적 — 접지 회전 시 피벗 드리프트 감시 (F9, 0.5초)
				if (IsCarryDebugEnabled() && Comp.Size() > 1.0f)
				{
					static double GLastCompLog = -10.0;
					const double NowT = FPlatformTime::Seconds();
					if (NowT - GLastCompLog > 0.5)
					{
						GLastCompLog = NowT;
						UE_LOG(LogCarry, Log, TEXT("[피벗보정] %s Comp=(%.1f, %.1f, %.1f)"),
							*Owner->GetName(), Comp.X, Comp.Y, Comp.Z);
					}
				}
			}
		}
	}

	// [스윕Z 진단] 명령 대비 실제 Z 미달 프로파일 — 상승을 깎는 지점 특정용 (F9, 0.5초)
	if (IsCarryDebugEnabled())
	{
		const float ZShort = DesiredPos.Z - Owner->GetActorLocation().Z;
		if (FMath::Abs(ZShort) > 2.0f)
		{
			static double GLastZProbeLog = -10.0;
			const double NowT = FPlatformTime::Seconds();
			if (NowT - GLastZProbeLog > 0.5)
			{
				GLastZProbeLog = NowT;
				UE_LOG(LogCarry, Log, TEXT("[스윕Z] %s 미달=%.1fuu 명령Z=%.0f 실제Z=%.0f 차단=%s%s"),
					*Owner->GetName(), ZShort, DesiredPos.Z, Owner->GetActorLocation().Z,
					MoveHit.GetActor() ? *MoveHit.GetActor()->GetName() : TEXT("-"),
					MoveHit.bStartPenetrating ? TEXT("(관통)") : TEXT(""));
			}
		}
	}

	Ctx.bValveJammed    = bValveJammed;
	Ctx.bFurnitureStuck = bFurnitureStuck;
}

bool UFurnitureGrabSystem::MoveReconcileAnchors(FGrabMoveContext& Ctx)
{
	AActor* Owner = GetOwner();
	TArray<ACharacter*>& Players = Ctx.Players;
	const float CurFurnYaw  = Ctx.CurFurnYaw;
	const float TargetYaw   = Ctx.TargetYaw;
	const FVector TargetLoc = Ctx.TargetLoc;

	// sweep 이동 도중 발생한 물리/데미지 이벤트 처리 로직 (기존 코드 유지)
	for (int32 i = Players.Num() - 1; i >= 0; --i)
	{
		if (!Anchors.Contains(Players[i]))
		{
			Players.RemoveAt(i);
		}
	}
	if (Players.Num() == 0)
		return false;
	// 앵커 계산용 자연 좌표 추출 시에도 보간된 CurrentHeightOffset 제거
	FVector ActualLoc = Owner->GetActorLocation() - FVector(0.0f, 0.0f, CurrentHeightOffset);
	const float ActualYaw = Owner->GetActorRotation().Yaw;

	// ---- 3.5. 회전 막힘 감지 → 위치 기준점 리셋 (호 미끄러짐 방지) ----
	// 회전이 막히면 TargetYaw만 누적돼 TargetLoc이 호를 그리며 가구가 벽을 따라 미끄러진다.
	// 앵커를 리셋해 다음 틱 목표를 실제 위치에 붙이고, 남은 회전 의도도 소거한다.
	{
		// 막힘은 '이번 틱 회전이 실제로 정지'했을 때만 — 정상 추격 지연을 오판해 재기록하면
		// 회전 의도가 소멸한다. 낀 틱(밸브 잼)도 제외(높이 와인드업).
		const bool bRotationBlocked = !Ctx.bValveJammed
			&& FMath::Abs(FMath::FindDeltaAngleDegrees(TargetYaw, ActualYaw)) > CorrectionDeadzone
			&& FMath::Abs(FMath::FindDeltaAngleDegrees(CurFurnYaw, ActualYaw)) < 0.05f;
		if (bRotationBlocked)
		{
			if (bPairLineValid)
			{
				// [들것 회전] 의도 리셋만 수행 — 위치 재앵커는 피동 운반자의 견인 거리(Delta) 누적을 깨므로 하지 않는다
				PairLineTargetYaw = ActualYaw;
			}
			else
			{
				for (int32 i = 0; i < Players.Num(); ++i)
				{
					ACharacter* P = Players[i];
					if (!Anchors.Contains(P)) continue;
					if (DraggedLastTick.Contains(P) || StoppedDraggingLastTick.Contains(P)) continue;

					FGrabAnchor& Anc        = Anchors[P];
					// Z는 보존: 회전 리셋은 XY/Yaw 재정렬이며(Z축 회전은 Z 불변),
					// Z까지 재기록하면 상승이 막힌 동안 앵커 Z가 매 틱 깎여 나간다
					Anc.InitialOffset       = FVector(ActualLoc.X - P->GetActorLocation().X,
					                                  ActualLoc.Y - P->GetActorLocation().Y,
					                                  Anc.InitialOffset.Z);
					// 몸통 기준은 델타 시프트로 연속 보존 — 가구기준만 현재로 재기록하면
					// 몸통 목표가 그랩 시점 값으로 되돌아가 몸이 휙 돌아간다
					Anc.InitialPlayerYaw    = FRotator::NormalizeAxis(Anc.InitialPlayerYaw
						+ FMath::FindDeltaAngleDegrees(Anc.InitialFurnitureYaw, ActualYaw));
					Anc.InitialFurnitureYaw = ActualYaw;
					Anc.InitialAimYaw       = P->GetBaseAimRotation().Yaw;
					Multicast_SetPlayerAnchor(P, ActualYaw, Anc.InitialPlayerYaw,
					                          Anc.InitialAimYaw, Anc.InitialOffset);
				}
			}
		}
	}

	// ---- 3.6. Z 상승 막힘(천장 등) → 높이 오프셋 드레인 ----
	// 초과 의도는 앵커가 아니라 높이 오프셋에서 비운다 — 앵커 Z 재기록은 최소 운반 높이의
	// 재상승과 맞물려 앵커↓/오프셋↑ 무한 랠리가 된다. 낀 틱(밸브 잼)은 제외.
	if (!Ctx.bValveJammed && TargetLoc.Z - ActualLoc.Z > CorrectionDeadzone)
	{
		const float Excess = TargetLoc.Z - ActualLoc.Z;
		if (IsCarryDebugEnabled() && Excess > 10.0f)
		{
			// Z 드레인 진단 (0.5초 스로틀): 어디서 상승이 새는지 프로파일용 좌표 포함
			static double GLastZResetLog = -10.0;
			const double NowT = FPlatformTime::Seconds();
			if (NowT - GLastZResetLog > 0.5)
			{
				GLastZResetLog = NowT;
				UE_LOG(LogCarry, Log, TEXT("[Z리셋] %s 초과=%.0fuu 드레인 (목표Z=%.0f 실제Z=%.0f 오프셋=%.0f 액터Z=%.0f)"),
					*Owner->GetName(), Excess, TargetLoc.Z, ActualLoc.Z,
					CurrentHeightOffset, Owner->GetActorLocation().Z);
			}
		}
		CurrentHeightOffset -= Excess;
		ActualLoc.Z = Owner->GetActorLocation().Z - CurrentHeightOffset;
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

			// [턱 오탐 방지] 오를 수 있는 턱을 벽으로 치면 가구 후퇴↔견인이 반복돼 러버밴딩이 된다.
			// 캡슐 밑단을 스텝 높이만큼 올려 스윕하고, 실제 등반은 CMC 스텝업에 맡긴다.
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
			// WorstBlock은 XY 성분만 있음(Shortfall Z=0) → Z는 Step 3 결과를 유지
			// (CurFurnZ로 되돌리면 Z 추종(낙하 따라가기)을 매번 무효화하게 됨)
			ActualLoc -= WorstBlock;
			// 실제 배치는 카메라 높이 오프셋 포함, 앵커용 ActualLoc은 자연 좌표 유지
			// (자연 좌표를 그대로 SetActorLocation하면 후퇴할 때마다 높이가 소실되는 버그)
			Owner->SetActorLocation(ActualLoc + FVector(0.0f, 0.0f, CurrentHeightOffset), false);
			ActualLoc = Owner->GetActorLocation() - FVector(0.0f, 0.0f, CurrentHeightOffset);

			// [상대좌표 보존] 운반자가 막힌 동안은 회전을 보류한다. 앵커를 재기록하지 않는 것이
			// 핵심 — 재기록하면 틀어진 위치가 새 기준으로 구워져 캐릭터와 가구가 벌어진다.
			bCarrierBlockedLastTick = true;
		}
	}


	Ctx.ActualLoc = ActualLoc;
	Ctx.ActualYaw = ActualYaw;
	Ctx.ToRelease = MoveTemp(ToRelease);
	return true;
}

void UFurnitureGrabSystem::MoveDrivePlayers(FGrabMoveContext& Ctx)
{
	const float DeltaTime   = Ctx.DeltaTime;
	const TArray<ACharacter*>& Players = Ctx.Players;
	const int32 N           = Ctx.N;
	const FVector ActualLoc = Ctx.ActualLoc;
	const float ActualYaw   = Ctx.ActualYaw;
	const bool  bUnderManned       = Ctx.bUnderManned;
	const float PairLineIntentRate = Ctx.PairLineIntentRate;
	const float TargetHeightOffset = Ctx.TargetHeightOffset;
	const bool  bValveJammed       = Ctx.bValveJammed;
	const bool  bFurnitureStuck    = Ctx.bFurnitureStuck;

	// ---- 5. CMC 속도 주입으로 플레이어 이동 제어 ----
	// DraggedLastTick으로 이전 틱 피동 여부를 추적해 판별 — 능동 주도자는 CMC·Yaw 모두
	// 간섭하지 않고, 피동만 CarryVelocity(도달 시 ZeroVector)를 주입한다.
	// 가속도·가구이동량 기반 판별은 폐기 (원격에서 0으로 읽히고 프레임레이트에 의존).

	if (IsCarryDebugEnabled() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9239, 0.2f, FColor::White,
			FString::Printf(TEXT("[운반] N=%d 미달끌기=%d 높이 %.0f→%.0f 운반자막힘=%d 가구막힘=%d"),
				N, bUnderManned ? 1 : 0, CurrentHeightOffset, TargetHeightOffset,
				bCarrierBlockedLastTick ? 1 : 0, bFurnitureStuck ? 1 : 0));
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
		// [견인 데드존] 진입은 PullStartRadius까지 넓게, 걸리면 CorrectionDeadzone까지 끌어 복원.
		// 해제 반경까지 넓히면 도달 앵커 재기록에 오차가 구워져 피동자가 대형에서 누적 이탈한다.
		const float PairHoldRadius = 70.0f;   // 첫 진입(한계) — 축 회전 중 목표 흔들림 보호
		// 이탈 반경은 주행 평형 지연보다 낮게 유지한다 (도달↔재견인 채터링 방지)
		const float PairHoldExit   = 8.0f;
		// 직전 틱에 도달한 직후엔 재진입도 좁게 — 완전히 정착한 뒤에만 넓은 진입 반경으로 복귀한다
		const bool bRecentlyDragged = bWasDragged || StoppedDraggingLastTick.Contains(P);
		float AtTargetRadius = bPairLineValid
			? (bRecentlyDragged ? PairHoldExit : PairHoldRadius)
			: (bWasDragged ? CorrectionDeadzone
			               : FMath::Max(PullStartRadius, CorrectionDeadzone));
		// [리쉬 모드] 무입력 피동의 견인 진입은 대칭 클램프 한계(유효 48)보다 안쪽에서 —
		// 가구가 서 있는 운반자의 리쉬 끝에 멈춰 견인 거리(70)에 못 닿아 아무도
		// 못 움직이는 교착을 방지한다 (회전 중 축 보호는 아래 bRotationBusy 보류가 담당)
		{
			const bool bHasInputEarly = HasCarryMoveInput(P, CMC);
			if (bLeashMovement && !bHasInputEarly && !bRecentlyDragged)
			{
				AtTargetRadius = FMath::Min(AtTargetRadius, 25.0f);
			}
		}
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
		// [리쉬/견인 분담] 입력자와 1인 운반은 리쉬(주입 없음), 2인+의 무입력 피동만 기존 견인으로
		// 가구를 따라 끌려온다 — 서 있는 파트너가 방치되거나(리쉬만) 되끌리는(견인만) 문제의 절충
		const bool bLeashOnly = bLeashMovement && (bHasMoveInput || Players.Num() < 2);
		// 입력 면제는 가구가 막혀 후퇴/낀 상태가 아닐 때만 — 가구가 못 가면 입력자도 견인에
		// 붙잡혀 함께 멈춘다 (막힌 가구를 두고 걸어가 대형이 벌어지거나 벽에 비벼 관통시키는 것 방지)
		const bool bActiveNow    = (bAtTarget && !bWasDragged)
			|| (bHasMoveInput && !bIntrudesFurniture && !bCarrierBlockedLastTick
				&& !bValveJammed && !bFurnitureStuck);

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
			// 피동 플레이어가 방금 목표에 도달 → XY 정지 (관성 슬라이딩 방지)
			// Z는 보존: 낙하 중이면 중력 속도를 지워선 안 됨 (공중 정지/슬로모 방지)
			// [서버+클라 동시 주입] 서버와 소유 클라가 같은 값을 주입해 move 재생 결과를 일치시킨다
			// [리쉬 모드] 입력자·1인 운반은 정지 주입 없음 — 입력 필터가 이탈을 막고 관성은 자연 감쇠.
			// 견인으로 끌려온 무입력 피동은 기존대로 정지 주입 (도달 후 관성 슬라이딩 방지)
			if (!bLeashOnly)
			{
				CMC->Velocity = FVector(0.0f, 0.0f, CMC->Velocity.Z);
				Multicast_ApplyPlayerCorrection(P, FVector::ZeroVector, DesiredYaw);
			}
			StoppedDraggingThisTick.Add(P);

			// 앵커 갱신: 도달 시점의 가구 상태를 새 기준점으로 — 갱신하지 않으면 두 사람의
			// Yaw 제안 기준이 어긋나 역회전·상호 피동 진동이 된다.
			// 들것(이미 선 목표로 일치)·낀 상태(튄 위치가 굳음)는 제외.
			if (!bPairLineValid && !bValveJammed && !bFurnitureStuck && Anchors.Contains(P))
			{
				FGrabAnchor& Anc        = Anchors[P];
				Anc.InitialOffset       = ActualLoc - P->GetActorLocation();
				// 몸통 기준은 델타 시프트로 연속 보존 — 옆으로 밀린 가구 방향을 새 기준으로
				// 구우면 몸이 옆을 보고 걷는다. GetDesiredYaw 결과가 재기록 전후 불변.
				Anc.InitialPlayerYaw    = FRotator::NormalizeAxis(Anc.InitialPlayerYaw
					+ FMath::FindDeltaAngleDegrees(Anc.InitialFurnitureYaw, ActualYaw));
				Anc.InitialFurnitureYaw = ActualYaw;
				Anc.InitialAimYaw       = P->GetBaseAimRotation().Yaw;
				Multicast_SetPlayerAnchor(P, ActualYaw, Anc.InitialPlayerYaw, Anc.InitialAimYaw, Anc.InitialOffset);
			}
			continue;
		}

		// [견인 중 회전 기준점 추종] 카메라를 안 움직이는 동안은 기준점을 현재 가구로 재정렬해
		// 회전 의도를 0으로 유지 — 그랩 시점에 머물면 나중에 돌릴 때 제안이 크게 튄다.
		// 카메라를 움직이면 재정렬을 건너뛰어 그 입력이 주도권으로 살아남는다.
		if (FGrabAnchor* AncFound = Anchors.Find(P))
		{
			FGrabAnchor& Anc   = *AncFound;
			const float  CurAim = P->GetBaseAimRotation().Yaw;
			const float  AimMoved = FMath::Abs(FMath::FindDeltaAngleDegrees(Anc.PrevAimYaw, CurAim));
			if (AimMoved < 0.1f)   // 카메라 정지 = 순수 견인 → 기준점 현재로 추종(의도 0)
			{
				const float OldYC = FMath::FindDeltaAngleDegrees(Anc.InitialFurnitureYaw, ActualYaw);
				Anc.InitialOffset       = Anc.InitialOffset.RotateAngleAxis(OldYC, FVector::UpVector);
				Anc.InitialFurnitureYaw = ActualYaw;
				Anc.InitialAimYaw       = CurAim;
				// 몸통 Yaw 기준은 회전량(OldYC)만큼 함께 이동시켜 재정렬 전후 불변으로 유지한다.
				// 현재 몸통을 대입하면 서버·클라 기준이 어긋나 원격에서 회전이 이중 적용된다.
				Anc.InitialPlayerYaw    = FRotator::NormalizeAxis(Anc.InitialPlayerYaw + OldYC);
			}
			Anc.PrevAimYaw = CurAim;
		}

		// [리쉬 모드] 입력자·1인 운반의 견인 주입 대체 — 대형 반경 밖 '바깥 방향' 속도 성분만
		// 깎는다 (서버 안전망; 소유 클라는 입력 필터가 같은 규칙이라 예측 보정 왕복 없음).
		// 2인+의 무입력 피동은 여기 안 타고 아래 기존 견인으로 가구를 따라 끌려온다.
		if (bLeashOnly)
		{
			FVector LeashAtt;
			float   LeashR = 0.0f;
			if (GetCarryLeash(P, LeashAtt, LeashR))
			{
				FVector ToAtt(LeashAtt.X - P->GetActorLocation().X,
				              LeashAtt.Y - P->GetActorLocation().Y, 0.0f);
				const float DistL = ToAtt.Size();
				// 소유 클라 입력 필터의 소프트 구간(R~R+10) 밖에서만 개입 — 구간 안에서
				// 서버가 속도를 깎으면 클라 예측과 어긋나 보정 왕복(밴딩)이 된다
				if (DistL > LeashR + 10.0f)
				{
					const FVector Away = -ToAtt / DistL;
					const float Outward = FVector::DotProduct(
						FVector(CMC->Velocity.X, CMC->Velocity.Y, 0.0f), Away);
					if (Outward > 0.0f)
					{
						CMC->Velocity -= Away * Outward;
					}
				}
			}
			// 피동 추적은 2인+만 — 1인 운반자는 유일한 회전 의도원이라 피동으로 지우면
			// 제자리(무입력) 카메라 회전이 가중치 0으로 통째로 무시돼 가구·몸이 동결된다
			if (Players.Num() >= 2)
			{
				CurrentTickDragged.Add(P);   // 피동 추적 유지 (가중치·선 회전 의도 제외 계산용)
			}
			continue;
		}

		// !bAtTarget: 피동 → 목표를 향해 끌어당김. CMC가 다음 틱에 감쇠시키는 BrakingDecel만큼
		// 더 주입해 실질 이동거리를 Delta와 맞춘다 (DeltaTime=0 나눗셈 가드 포함).
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
		CurrentTickDragged.Add(P);
	}

	DraggedLastTick          = MoveTemp(CurrentTickDragged);
	StoppedDraggingLastTick  = MoveTemp(StoppedDraggingThisTick);

}

void UFurnitureGrabSystem::MoveFinalize(FGrabMoveContext& Ctx)
{
	AActor* Owner = GetOwner();
	const float DeltaTime    = Ctx.DeltaTime;
	const TArray<ACharacter*>& Players = Ctx.Players;
	const FVector CurFurnLoc = Ctx.CurFurnLoc;
	const FVector ActualLoc  = Ctx.ActualLoc;
	const float ActualYaw    = Ctx.ActualYaw;
	const TArray<ACharacter*>& ToRelease = Ctx.ToRelease;

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
		Release(P);
	}

	// ---- 7. 클라 보간용 트랜스폼 갱신 ----	
	ServerLocation = Owner->GetActorLocation();
	ServerRotation = Owner->GetActorRotation();
	Multicast_UpdateFurnitureTransform(ServerLocation, ServerRotation, SystemOffsetSequence);

	// 리슨서버 호스트 보정: LocalSyncTargetYaw 갱신이 Multicast 수신부에만 있어 서버에선
	// 낡은 값으로 스냅된다 — 호스트도 클라와 동일하게 최신 가구 Yaw로 갱신.
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
