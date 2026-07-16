// GrabComponent.cpp

#include "Player/Component/GrabComponent.h"
#include "Player/Character/TCPlayerCharacter.h"
#include "Player/Interface/TCInteractable.h"
#include "Core/TeamCarryGameState.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "CatchCharacter/Furniture/FurnitureGrabSystem.h"
#include "CatchCharacter/Furniture/FurnitureStat.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// 잡기 스캔 디버그 시각화 — 콘솔 "TC.GrabDebug 1" 또는 "TC.GrabDebugToggle"(F9). 로컬 조종 캐릭터에서만 그림
	TAutoConsoleVariable<int32> CVarGrabDebug(
		TEXT("TC.GrabDebug"), 0,
		TEXT("잡기 스캔 디버그 표시 (0=끔, 1=켬)"));

	// 운반 테스트 치트: 잡은 로컬 폰이 가구 반대 방향으로 자동 이동 입력 —
	// 한 키보드로 '서로 반대 당김'(줄다리기)을 재현한다. 입력은 리쉬 필터를 그대로 통과시킨다
	TAutoConsoleVariable<int32> CVarCarryAutoPull(
		TEXT("TC.Carry.AutoPull"), 0,
		TEXT("운반 줄다리기 테스트: 잡은 로컬 폰이 가구 반대로 자동 입력 (0=끔, 1=켬)"));
	FAutoConsoleCommand CmdGrabDebugToggle(
		TEXT("TC.GrabDebugToggle"),
		TEXT("잡기 스캔 디버그 표시 토글 (F9)"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const int32 NewVal = CVarGrabDebug.GetValueOnGameThread() ? 0 : 1;
			CVarGrabDebug->Set(NewVal, ECVF_SetByConsole);
			// 화면 표시와 로그 게이트가 같은 CVar를 보는지 검증용 — 로그에도 상태를 남긴다
			UE_LOG(LogTemp, Warning, TEXT("[운반 디버그] TC.GrabDebug=%d (F9 토글)"), NewVal);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(9100, 2.f, FColor::Yellow,
					FString::Printf(TEXT("[잡기 디버그] %s"), NewVal ? TEXT("ON") : TEXT("OFF")));
			}
		}));

	// 가시선 검사(단일 시작점): 벽 등 비상호작용 차단물에 막히면 잡기 불가.
	// 가구·폰은 차단으로 치지 않고 무시 후 재시도(최대 6겹)
	bool HasGrabLineOfSightFrom(UWorld* World, AActor* OwnerActor, AActor* Target, const FVector& Start)
	{
		if (!World || !Target)
		{
			return false;
		}
		// 트레이스 목표는 '플레이어에서 가장 가까운 바운즈 지점' (벽 뒤 가구는 여전히 차단됨)
		const FVector TargetPoint = Target->GetComponentsBoundingBox().GetClosestPointTo(Start);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GrabLOS), /*bTraceComplex=*/false);
		Params.AddIgnoredActor(OwnerActor);
		for (int32 Depth = 0; Depth < 6; ++Depth)
		{
			FHitResult Hit;
			if (!World->LineTraceSingleByChannel(Hit, Start, TargetPoint, ECC_Visibility, Params))
			{
				return true; // 아무것도 안 막힘
			}
			AActor* HitActor = Hit.GetActor();
			if (HitActor == Target)
			{
				return true; // 대상 도달
			}
			// 가구(Interactable)와 플레이어(폰)는 시야 차단으로 치지 않는다 — 무시 목록에 넣고 재시도
			if (HitActor && (HitActor->Implements<UTCInteractable>() || Cast<APawn>(HitActor) != nullptr))
			{
				Params.AddIgnoredActor(HitActor);
				continue;
			}
			return false; // 벽 등 비상호작용 차단물
		}
		return false; // 여러 겹 가려짐 — 사실상 도달 불가로 간주
	}

	// 가시선 검사: 카메라 기준이 막혀도 '캐릭터 눈높이' 기준으로 한 번 더 본다 —
	// 상부장 아래 조리대 위 소품처럼 카메라(머리 위)에서는 선반에 가리지만 캐릭터는
	// 정면으로 보고 있는 배치를 잡을 수 있게 한다 (캐릭터 기준이라 벽 뒤 악용은 여전히 차단)
	bool HasGrabLineOfSight(UWorld* World, AActor* OwnerActor, AActor* Target, const FVector& Start)
	{
		if (HasGrabLineOfSightFrom(World, OwnerActor, Target, Start))
		{
			return true;
		}
		if (OwnerActor)
		{
			const FVector EyeStart = OwnerActor->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
			return HasGrabLineOfSightFrom(World, OwnerActor, Target, EyeStart);
		}
		return false;
	}
}

// 틱 활성화 여부 및 초기화
UGrabComponent::UGrabComponent()
{
	// 매 프레임 스캔을 위해 틱 활성화
	PrimaryComponentTick.bCanEverTick = true;
	CurrentBestTarget = nullptr;

	// 잡고 있는 대상 초기화
	GrabbedActor = nullptr;

	// 컴포넌트 자체의 네트워크 동기화 기능 활성화
	SetIsReplicatedByDefault(true);
}

// 매 프레임마다 상호작용 대상 탐색
void UGrabComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 모든 플레이어의 트레이스 실행
	ScanBestTarget();

	// 운반 시야 확보는 '잡힌 가구의 카메라 채널 무시'(FurnitureGrabSystem::Grab)로 처리한다

	// [유령 잡기 정리] GrabSystem에서만 해제된 잔여 GrabbedActor 참조를 서버가 매 틱 대조해 지운다 (Replicated라 클라도 복구)
	if (GrabbedActor && GetOwner() && GetOwner()->HasAuthority())
	{
		ACharacter* OwnerCharForHeal = Cast<ACharacter>(GetOwner());
		const UFurnitureGrabSystem* GrabSys = GrabbedActor->FindComponentByClass<UFurnitureGrabSystem>();
		if (OwnerCharForHeal && GrabSys && !GrabSys->IsGrabbedBy(OwnerCharForHeal))
		{
			//UE_LOG(LogTemp, Warning, TEXT("[운반] 유령 잡기 정리: %s → %s (GrabSystem엔 이미 없음)"),
			//	*OwnerCharForHeal->GetName(), *GrabbedActor->GetName());
			GrabbedActor = nullptr;
		}
	}

	// 가구를 들고 체공 시 강제 드랍
	if (GrabbedActor)
	{
		if (ATCPlayerCharacter* OwnerChar = Cast<ATCPlayerCharacter>(GetOwner()))
		{
			if (UCharacterMovementComponent* CMC = OwnerChar->GetCharacterMovement())
			{
				// 캐릭터가 공중에 있는지 확인
				if (CMC->IsFalling())
				{
					CurrentFallTime += DeltaTime;

					// 설정한 체공 시간을 초과하면 강제로 놓기
					if (CurrentFallTime >= MaxFallTimeToDrop)
					{
						// 로컬 클라 + 서버에서 실행 (중복 통신 방지)
						if (OwnerChar->IsLocallyControlled() || OwnerChar->HasAuthority())
						{
							if (CVarGrabDebug.GetValueOnGameThread() != 0 && GEngine)
							{
								// 체공 자동 드랍 발생 표시 (F9 디버그)
								GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
									FString::Printf(TEXT("[운반] 체공 자동 드랍: %s 체공 %.2fs (허용 %.2fs)"),
										*OwnerChar->GetName(), CurrentFallTime, MaxFallTimeToDrop));
								DrawDebugSphere(GetWorld(), OwnerChar->GetActorLocation(), 40.0f, 12,
									FColor::Red, false, 5.0f, 0, 3.0f);
							}
							// 사후 로그 분석용 — F9 여부와 무관하게 기록 (Output Log에서 "체공" 검색)
							//UE_LOG(LogTemp, Warning, TEXT("[운반] 체공 자동 드랍: %s 체공 %.2fs (허용 %.2fs)"),
							//	*OwnerChar->GetName(), CurrentFallTime, MaxFallTimeToDrop);
							// 가구 내려놓기 로직 호출
							TryInteract();
						}
						// 초기화
						CurrentFallTime = 0.0f;
					}
				}
				else
				{
					// 땅에 닿으면 체공 시간 초기화
					CurrentFallTime = 0.0f;
				}
			}
		}
	}
	else
	{
		// 가구를 들고 있지 않으면 시간 초기화
		CurrentFallTime = 0.0f;
	}

	// [리쉬] 소유 클라 관성 클램프 — 입력은 필터가 막지만 남은 관성이 반경을 넘으면
	// 바깥 성분만 깎는다 (서버 Step 5와 동일 규칙 → 예측 일치, 보정 왕복 없음)
	if (GrabbedActor)
	{
		ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
		if (OwnerChar && OwnerChar->IsLocallyControlled() && !OwnerChar->HasAuthority())
		{
			if (UFurnitureGrabSystem* FGS = GrabbedActor->FindComponentByClass<UFurnitureGrabSystem>())
			{
				FVector Att;
				float   R = 0.0f;
				if (FGS->bLeashMovement && FGS->GetCarryLeash(OwnerChar, Att, R))
				{
					if (UCharacterMovementComponent* CMC = OwnerChar->GetCharacterMovement())
					{
						FVector ToAtt(Att.X - OwnerChar->GetActorLocation().X,
						              Att.Y - OwnerChar->GetActorLocation().Y, 0.0f);
						const float Dist = ToAtt.Size();
						if (Dist > R)
						{
							const FVector Away = -ToAtt / Dist;
							const float Outward = FVector::DotProduct(
								FVector(CMC->Velocity.X, CMC->Velocity.Y, 0.0f), Away);
							if (Outward > 0.0f)
							{
								CMC->Velocity -= Away * Outward;
							}
						}
					}
				}
			}
		}
	}

	// [테스트] TC.Carry.AutoPull=1: 잡은 로컬 폰 전원이 가구 반대 방향으로 자동 입력 —
	// PIE 두 창의 폰이 서로 반대로 당기는 줄다리기가 재현된다 (실제 입력 경로처럼 리쉬 필터 통과)
	if (GrabbedActor && CVarCarryAutoPull.GetValueOnGameThread() != 0)
	{
		ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
		if (OwnerChar && OwnerChar->IsLocallyControlled())
		{
			FVector Away = OwnerChar->GetActorLocation() - GrabbedActor->GetActorLocation();
			Away.Z = 0.0f;
			if (Away.Normalize())
			{
				const FVector Filtered = FilterCarryInput(Away);
				if (!Filtered.IsNearlyZero())
				{
					OwnerChar->AddMovementInput(Filtered, 1.0f);
				}
			}
		}
	}
}

// [리쉬] 운반 중 이동 입력 필터 — 반경 밖 원심 성분 제거 (접선 유지, 벽 따라 돌기는 가능)
FVector UGrabComponent::FilterCarryInput(const FVector& WorldInput) const
{
	AActor* Grabbed = GetGrabbedActor();
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!Grabbed || !OwnerChar || WorldInput.IsNearlyZero())
		return WorldInput;

	UFurnitureGrabSystem* FGS = Grabbed->FindComponentByClass<UFurnitureGrabSystem>();
	if (!FGS || !FGS->bLeashMovement)
		return WorldInput;

	FVector Att;
	float   R = 0.0f;
	if (!FGS->GetCarryLeash(OwnerChar, Att, R))
		return WorldInput;

	FVector ToAtt(Att.X - OwnerChar->GetActorLocation().X,
	              Att.Y - OwnerChar->GetActorLocation().Y, 0.0f);
	const float Dist = ToAtt.Size();
	if (Dist <= R || Dist <= KINDA_SMALL_NUMBER)
		return WorldInput;

	const FVector Away = -ToAtt / Dist;
	const float Outward = FVector::DotProduct(WorldInput, Away);
	return (Outward > 0.0f) ? WorldInput - Away * Outward : WorldInput;
}

// 대상이 존재하면 서버로 상호작용-잡기 시도 요청
bool UGrabComponent::TryInteract()
{
	// 대상이 있는지 확인
	if (CurrentBestTarget != nullptr)
	{
		// 대상이 있다면 서버로 상호작용 처리 요청
		ServerTryInteract(CurrentBestTarget);

		// 그랩 몽타주는 실제로 손에 들 수 있는 가구(FurnitureGrabSystem 보유)일 때만 재생한다.
		// 문/게시판 등 단순 토글형 상호작용까지 가구 잡기 애니메이션이 나가던 문제 수정.
		return CurrentBestTarget->FindComponentByClass<UFurnitureGrabSystem>() != nullptr;
	}

	// 이미 가구를 들고 있는 경우 (내려놓기)
	if (GrabbedActor != nullptr)
	{
		ServerTryInteract(nullptr); // 서버에 내려놓기 처리 요청
		return false; // 잡기 몽타주를 재생하지 않도록 false 반환
	}

	// 대상이 없다면 false 반환
	return false;

}

// 서버로 상호작용-던지기 시도 요청
void UGrabComponent::TryThrow()
{
	// 서버로 던지기 요청
	ServerTryThrow();
}

void UGrabComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// GrabbedActor 변수의 상태를 서버와 클라이언트가 실시간으로 일치시킴
	DOREPLIFETIME(UGrabComponent, GrabbedActor);
}

// 클라이언트 - 회전 요청 전달

void UGrabComponent::TryRotateFurniture(FRotator RotationDelta)
{
	/*
	// 가구를 들고 있을 때만 서버에 회전 요청
	if (GrabbedActor)
	{
		ServerRotateFurniture(RotationDelta);
	}
	*/
}


// 멀티 박스 트레이스 발사해서 최적 대상 판별
void UGrabComponent::ScanBestTarget()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	const APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	const bool bDebugDraw = CVarGrabDebug.GetValueOnGameThread() != 0
		&& OwnerPawn && OwnerPawn->IsLocallyControlled();

	// 발밑 가구 제외: 밟고 서 있는 바닥 액터는 후보에서 제외한다 ('가구 타고 부양' 차단)
	AActor* StandingOn = nullptr;
	if (const ACharacter* OwnerChar = Cast<ACharacter>(OwnerActor))
	{
		if (const UCharacterMovementComponent* OwnerCMC = OwnerChar->GetCharacterMovement())
		{
			if (OwnerCMC->CurrentFloor.bBlockingHit)
			{
				StandingOn = OwnerCMC->CurrentFloor.HitResult.GetActor();
			}
		}
	}

	FVector ForwardVector = OwnerActor->GetActorForwardVector();

	// 박스 트레이스 범위 설정 (전방 50cm 지점, 80x80 단면)
	FVector Start = OwnerActor->GetActorLocation() + (ForwardVector * 50.0f);
	FVector End = Start + (ForwardVector * 1.0f);
	// 수직 반경 70: 경사면(사선 통로)의 낮은 가구도 스캔에 걸리도록 넓게 잡는다
	FVector HalfSize = FVector(40.f, 40.f, 70.f);

	// 충돌 검사 결과를 담기 위한 배열
	TArray<FHitResult> HitResults;

	// 충돌 검사에서 무시할 액터 배열에 자신 추가하기
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(OwnerActor); // 자기 자신은 탐색에서 제외

	// 멀티 박스 트레이스 발사
	bool bHit = UKismetSystemLibrary::BoxTraceMulti(
		this, Start, End, HalfSize, OwnerActor->GetActorRotation(),
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false, ActorsToIgnore,
		bDebugDraw ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None,
		HitResults, true
	);

	AActor* NewBestTarget = nullptr;
	float HighestScore = -1.0f; // 여러 대상이 감지되면 점수로 우선 순위를 계산해서 추려내기

	// 후보 수집용 (같은 액터는 최고점 하나만) — 스택 우선순위 후처리를 위해 즉시 선정하지 않는다
	struct FGrabCandidate { AActor* Actor; float Score; FBox Bounds; };
	TArray<FGrabCandidate> Candidates;

	if (bHit)
	{
		for (const FHitResult& Hit : HitResults)
		{
			AActor* HitActor = Hit.GetActor();

			// 대상이 Interactable 인터페이스를 상속받았는지 확인
			if (HitActor && HitActor->Implements<UTCInteractable>())
			{
				// 밟고 있는 가구는 잡기 불가 (위 StandingOn 주석 참고)
				if (HitActor == StandingOn)
				{
					if (bDebugDraw)
					{
						DrawDebugLine(GetWorld(), Start,
							HitActor->GetComponentsBoundingBox().GetCenter(),
							FColor::Blue, false, -1.f, 0, 1.5f);
					}
					continue;
				}

				// 벽 너머 가구 차단: 박스에는 걸렸어도 가시선이 벽에 막히면 후보에서 제외
				if (!HasGrabLineOfSight(GetWorld(), OwnerActor, HitActor, Start))
				{
					if (bDebugDraw)
					{
						DrawDebugLine(GetWorld(), Start,
							HitActor->GetComponentsBoundingBox().GetCenter(),
							FColor::Red, false, -1.f, 0, 1.5f);
					}
					continue;
				}

				// 방향·거리는 피벗(ActorLocation)이 아니라 '박스가 실제로 맞힌 지점(ImpactPoint)' 기준으로 계산한다
				const FVector AimPoint = Hit.ImpactPoint;
				FVector DirectionToTarget = (AimPoint - Start).GetSafeNormal();
				float Distance = FVector::Distance(Start, AimPoint);

				// 시선 방향과 대상 방향의 내적 (1.0에 가까울수록 완벽한 정면)
				float DotProduct = FVector::DotProduct(ForwardVector, DirectionToTarget);

				// [지근거리 예외] 품 안 거리(시작 겹침 포함)는 각도 검사를 생략한다
				const bool bPointBlank = Hit.bStartPenetrating || Distance < 60.0f;

				// 등 뒤에 있거나 시야각(약 90도)을 벗어난 대상은 무시
				if (!bPointBlank && DotProduct < 0.0f)
				{
					if (bDebugDraw)
					{
						DrawDebugLine(GetWorld(), Start, AimPoint,
							FColor::Magenta, false, -1.f, 0, 1.5f);
					}
					continue;
				}

				if (bDebugDraw)
				{
					DrawDebugLine(GetWorld(), Start, AimPoint,
						FColor::Green, false, -1.f, 0, 1.5f);
				}

				// 점수 산정 (정면일수록 가점, 가까울수록 가점)
				// 가중치(W1, W2)는 게임 플레이에 맞춰 조정 가능
				float Score = (DotProduct * 1000.0f) + (1000.0f / (Distance + 1.0f));

				// [지근거리 점수 보정] 품 안 후보는 방향 점수 대신 고정 최우선 점수 + 근접 보너스로 산정한다
				if (bPointBlank)
				{
					const float NearDist = FVector::Distance(Start,
						HitActor->GetComponentsBoundingBox().GetClosestPointTo(Start));
					Score = 2000.0f + 1000.0f / (NearDist + 1.0f);
				}

				// 후보 수집 (같은 액터의 중복 히트는 최고점만 유지)
				bool bMerged = false;
				for (FGrabCandidate& C : Candidates)
				{
					if (C.Actor == HitActor)
					{
						C.Score = FMath::Max(C.Score, Score);
						bMerged = true;
						break;
					}
				}
				if (!bMerged)
				{
					Candidates.Add({ HitActor, Score, HitActor->GetComponentsBoundingBox() });
				}
			}
		}
	}

	// [겹침 스택 우선순위] 후보끼리 바운즈가 겹치면 더 큰(감싸는) 쪽을 감점해 위에 얹힌 작은 물건을 우선한다
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		for (int32 j = i + 1; j < Candidates.Num(); ++j)
		{
			if (!Candidates[i].Bounds.Intersect(Candidates[j].Bounds))
				continue;
			const bool bIBigger = Candidates[i].Bounds.GetVolume() > Candidates[j].Bounds.GetVolume();
			(bIBigger ? Candidates[i] : Candidates[j]).Score *= 0.4f;
		}
	}
	for (const FGrabCandidate& C : Candidates)
	{
		if (C.Score > HighestScore)
		{
			HighestScore = C.Score;
			NewBestTarget = C.Actor;
		}
	}

	// 디버그: 현재 대상과 상호작용 가능 여부(정원/파괴 등)를 화면에 표시
	if (bDebugDraw && GEngine)
	{
		if (NewBestTarget)
		{
			const bool bCan = ITCInteractable::Execute_CanInteract(
				NewBestTarget, Cast<ATCPlayerCharacter>(OwnerActor));
			GEngine->AddOnScreenDebugMessage(9101, 0.f, bCan ? FColor::Green : FColor::Red,
				FString::Printf(TEXT("[잡기 디버그] 대상: %s | CanInteract: %s"),
					*NewBestTarget->GetName(),
					bCan ? TEXT("가능") : TEXT("불가 — 정원초과/파괴/시스템 없음")));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(9101, 0.f, FColor::Orange,
				TEXT("[잡기 디버그] 대상 없음 — 박스 미히트 / 빨강=가시선 차단 / 보라=후방각"));
		}
	}

	// 대상이 바뀌었을 때 포커스 이벤트 처리
	if (CurrentBestTarget != NewBestTarget)
	{
		// 포커스 하이라이트(노랑)는 개인별 표시 — 포커스 이벤트는 로컬 조종 캐릭터에서만 발화한다
		const bool bLocalViewer = OwnerPawn && OwnerPawn->IsLocallyControlled();

		if (bLocalViewer && CurrentBestTarget)
		{
			// 기존 대상의 포커스 해제 알림
			ITCInteractable::Execute_OnUnfocus(CurrentBestTarget);
		}

		if (bLocalViewer && NewBestTarget)
		{
			// 새로운 대상에 포커스 획득 알림
			ITCInteractable::Execute_OnFocus(NewBestTarget);
		}

		CurrentBestTarget = NewBestTarget;
	}
}

// 게임이 이미 종료됐는지 서버 권위(GameState 복제값) 기준으로 판정(명세 4장-8).
bool UGrabComponent::IsGameFinishedAuthoritative() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const ATeamCarryGameState* GS = World->GetGameState<ATeamCarryGameState>())
		{
			return GS->bIsGameFinished;
		}
	}
	return false;
}

// Server - 실제 가구 회전 적용

void UGrabComponent::ServerRotateFurniture_Implementation(FRotator RotationDelta)
{
	/*
	if (IsGameFinishedAuthoritative())
	{
		return;
	}

	if (GrabbedActor)
	{
		// 가구 액터에서 FurnitureGrabSystem 컴포넌트 찾기
		UFurnitureGrabSystem* GrabSystem = GrabbedActor->FindComponentByClass<UFurnitureGrabSystem>();

		if (GrabSystem)
		{
			// 시스템에 구현된 오프셋 적용 함수 사용 (LocationOffset, YawOffset, PitchOffset 순서)
			GrabSystem->AddFurnitureOffset(FVector::ZeroVector, RotationDelta.Yaw, RotationDelta.Pitch);

			UE_LOG(LogTemp, Warning, TEXT("[Server] 가구 회전 적용(GrabSystem): %s"), *RotationDelta.ToString());
		}
	}
	*/
}

// Server - 상호작용-던지기 실행
void UGrabComponent::ServerTryThrow_Implementation()
{
	if (IsGameFinishedAuthoritative())
	{
		return;
	}

	// 서버 측 2명 이상 운반 검증
	if (GrabbedActor)
	{
		UFurnitureGrabSystem* FGS = GrabbedActor->FindComponentByClass<UFurnitureGrabSystem>();
		UFurnitureStat* Stat = GrabbedActor->FindComponentByClass<UFurnitureStat>();

		// 2명 이상 들고 있으면 가구를 던지지 않고 함수 종료
		if (FGS && Stat && (FGS->GetGrabbedPlayers().Num() >= 2 || Stat->GetRequiredPlayer() >= 2))
		{
			return;
		}
	}

	// 캐릭터가 들고 있는 대상이 있거나 상호작용이 가능한 객체인지 확인
	if (GrabbedActor && GrabbedActor->Implements<UTCInteractable>())
	{
		ATCPlayerCharacter* Player = Cast<ATCPlayerCharacter>(GetOwner());

		// 기존 상호작용 함수를 호출하여 가구 잡기 상태 해제 (손에서 놓기)
		ITCInteractable::Execute_OnInteract(GrabbedActor, Player);

		// 가구의 물리 컴포넌트를 가져와서 밀어내는 힘 가하기
		UPrimitiveComponent* MeshComp = Cast<UPrimitiveComponent>(GrabbedActor->GetRootComponent());
		if (MeshComp)
		{
			if (Player)
			{
				// 멀티플레이 동기화가 보장되는 플레이어 컨트롤러의 시선 방향 벡터 가져오기
				FVector ThrowDirection = Player->GetControlRotation().Vector();

				// 놓는 타이밍에 물리 엔진이 꺼져있을 수 있으므로 강제로 활성화
				MeshComp->SetSimulatePhysics(true);

				// 던지는 힘 설정
				float ThrowForce = 1000.0f;

				// 가구에 순간적인 힘(Impulse) 적용
				MeshComp->AddImpulse(ThrowDirection * ThrowForce, NAME_None, true);
			}
		}

		// 던졌으므로 손에 들고 있는 대상 참조 비우기
		GrabbedActor = nullptr;

		// 확인용 로그
		UE_LOG(LogTemp, Warning, TEXT("[Server] 던지기 완료"));
	}
}

// Server - 상호작용-잡기 실행
void UGrabComponent::ServerTryInteract_Implementation(AActor* TargetActor)
{
	if (IsGameFinishedAuthoritative())
	{
		return;
	}

	// 이미 무언가를 잡고 있다면 내려놓기 우선 처리
	if (GrabbedActor)
	{
		ITCInteractable::Execute_OnInteract(GrabbedActor, Cast<ATCPlayerCharacter>(GetOwner()));
		GrabbedActor = nullptr; // 참조 비우기
	}
	// 서버 검증
	else if (TargetActor && TargetActor->Implements<UTCInteractable>())
	{
		ATCPlayerCharacter* OwnerCharacter = Cast<ATCPlayerCharacter>(GetOwner());

		// 서버 측 가시선 재검증 — 클라이언트 스캔 시점과 서버 상태가 다르거나
		// 조작된 요청이 와도 벽 너머 가구는 잡을 수 없게 막는다.
		if (OwnerCharacter)
		{
			const FVector GrabOrigin = OwnerCharacter->GetActorLocation()
				+ OwnerCharacter->GetActorForwardVector() * 50.0f;
			if (!HasGrabLineOfSight(GetWorld(), OwnerCharacter, TargetActor, GrabOrigin))
			{
				UE_LOG(LogTemp, Warning, TEXT("[ServerTryInteract] HasGrabLineOfSight 실패: %s"), *TargetActor->GetName());
				return;
			}

			// 발밑 가구 서버 재검증 — 밟고 있는 가구를 들면 '가구 타고 부양'이 되므로 차단
			if (const UCharacterMovementComponent* OwnerCMC = OwnerCharacter->GetCharacterMovement())
			{
				if (OwnerCMC->CurrentFloor.bBlockingHit
					&& OwnerCMC->CurrentFloor.HitResult.GetActor() == TargetActor)
				{
					return;
				}
			}
		}

		// 대상 가구가 지금 잡을 수 있는 상태인지 검증 (정원 초과 등 확인)
		if (ITCInteractable::Execute_CanInteract(TargetActor, OwnerCharacter))
		{
			UE_LOG(LogTemp, Warning, TEXT("[ServerTryInteract] OnInteract 호출: %s"), *TargetActor->GetName());
			// 검증을 통과했다면 가구의 OnInteract 실행 (가구 쪽에서 물리 연결 처리)
			ITCInteractable::Execute_OnInteract(TargetActor, OwnerCharacter);

			// 실제로 들 수 있는 가구(FurnitureGrabSystem 보유)만 GrabbedActor로 기억한다.
			// 문/게시판처럼 단순 토글형 상호작용 대상까지 손에 든 것으로 취급하면
			// 이동속도 잠금(StartRun/StopRun의 GetGrabbedActor() 체크)이 잘못 걸린다.
			if (TargetActor->FindComponentByClass<UFurnitureGrabSystem>())
			{
				GrabbedActor = TargetActor;
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ServerTryInteract] 서버측 CanInteract 실패: %s"), *TargetActor->GetName());
		}
	}
}