// GrabComponent.cpp

#include "Player/Component/GrabComponent.h"
#include "Player/Character/TCPlayerCharacter.h"
#include "Player/Interface/TCInteractable.h"
#include "Core/TeamCarryGameState.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "CatchCharacter/Furniture/FurnitureGrabSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

namespace
{
	// 벽 너머(가시선 차단) 대상 판정.
	// 탐색용 박스 트레이스는 부피가 있어 얇은 벽 반대편 가구까지 히트로 돌려주므로,
	// 시작점→대상 라인 트레이스가 '벽 등 비상호작용 차단물'에 먼저 막히면 잡기 불가로 본다.
	// 도중의 다른 가구(Interactable)는 시야 차단으로 치지 않는다 — 가구 무더기 뒤의
	// 가구도 잡을 수 있어야 하므로, 가구 히트는 무시 목록에 넣고 재시도한다(최대 4겹).
	bool HasGrabLineOfSight(UWorld* World, AActor* OwnerActor, AActor* Target, const FVector& Start)
	{
		if (!World || !Target)
		{
			return false;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GrabLOS), /*bTraceComplex=*/false);
		Params.AddIgnoredActor(OwnerActor);
		for (int32 Depth = 0; Depth < 4; ++Depth)
		{
			FHitResult Hit;
			if (!World->LineTraceSingleByChannel(Hit, Start, Target->GetActorLocation(), ECC_Visibility, Params))
			{
				return true; // 아무것도 안 막힘
			}
			AActor* HitActor = Hit.GetActor();
			if (HitActor == Target)
			{
				return true; // 대상 도달
			}
			if (HitActor && HitActor->Implements<UTCInteractable>())
			{
				Params.AddIgnoredActor(HitActor); // 가구는 통과 — 다음 겹 검사
				continue;
			}
			return false; // 벽 등 비상호작용 차단물
		}
		return false; // 4겹 이상 가려짐 — 사실상 도달 불가로 간주
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

					// 설정한 체공 시간(0.8초)을 초과하면 강제로 놓기
					if (CurrentFallTime >= MaxFallTimeToDrop)
					{
						// 로컬 클라 + 서버에서 실행 (중복 통신 방지)
						if (OwnerChar->IsLocallyControlled() || OwnerChar->HasAuthority())
						{
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
}

// 대상이 존재하면 서버로 상호작용-잡기 시도 요청
bool UGrabComponent::TryInteract()
{
	// 대상이 있는지 확인
	if (CurrentBestTarget != nullptr)
	{
		// 대상이 있다면 서버로 상호작용 처리 요청
		ServerTryInteract(CurrentBestTarget);

		// 잡기에 성공했으면 true 반환
		return true;
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
	// 가구를 들고 있을 때만 서버에 회전 요청
	if (GrabbedActor)
	{
		ServerRotateFurniture(RotationDelta);
	}
}

// 멀티 박스 트레이스 발사해서 최적 대상 판별
void UGrabComponent::ScanBestTarget()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	FVector ForwardVector = OwnerActor->GetActorForwardVector();

	// 박스 트레이스 범위 설정 (50cm, 2525x25)
	FVector Start = OwnerActor->GetActorLocation() + (ForwardVector * 50.0f);
	FVector End = Start + (ForwardVector * 1.0f);
	FVector HalfSize = FVector(40.f, 40.f, 40.f);

	// 충돌 검사 결과를 담기 위한 배열
	TArray<FHitResult> HitResults;

	// 충돌 검사에서 무시할 액터 배열에 자신 추가하기
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(OwnerActor); // 자기 자신은 탐색에서 제외

	// 멀티 박스 트레이스 발사
	bool bHit = UKismetSystemLibrary::BoxTraceMulti(
		this, Start, End, HalfSize, OwnerActor->GetActorRotation(),
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false, ActorsToIgnore, EDrawDebugTrace::None, // 디버그 선 보려면 수정(None, ForOneFrame)
		HitResults, true
	);

	AActor* NewBestTarget = nullptr;
	float HighestScore = -1.0f; // 여러 대상이 감지되면 점수로 우선 순위를 계산해서 추려내기

	if (bHit)
	{
		for (const FHitResult& Hit : HitResults)
		{
			AActor* HitActor = Hit.GetActor();

			// 대상이 Interactable 인터페이스를 상속받았는지 확인
			if (HitActor && HitActor->Implements<UTCInteractable>())
			{
				// 벽 너머 가구 차단: 박스에는 걸렸어도 가시선이 벽에 막히면 후보에서 제외
				if (!HasGrabLineOfSight(GetWorld(), OwnerActor, HitActor, Start))
				{
					continue;
				}

				// 대상까지의 방향과 거리 계산
				FVector DirectionToTarget = (HitActor->GetActorLocation() - Start).GetSafeNormal();
				float Distance = FVector::Distance(Start, HitActor->GetActorLocation());

				// 시선 방향과 대상 방향의 내적 (1.0에 가까울수록 완벽한 정면)
				float DotProduct = FVector::DotProduct(ForwardVector, DirectionToTarget);

				// 등 뒤에 있거나 시야각(약 90도)을 벗어난 대상은 무시
				if (DotProduct < 0.0f) continue;

				// 점수 산정 (정면일수록 가점, 가까울수록 가점)
				// 가중치(W1, W2)는 게임 플레이에 맞춰 조정 가능
				float Score = (DotProduct * 1000.0f) + (1000.0f / (Distance + 1.0f));

				// 기존 최고 점수보다 높다면 갱신
				if (Score > HighestScore)
				{
					HighestScore = Score;
					NewBestTarget = HitActor;
				}
			}
		}
	}

	// 대상이 바뀌었을 때 포커스 이벤트 처리
	if (CurrentBestTarget != NewBestTarget)
	{
		if (CurrentBestTarget)
		{
			// 기존 대상의 포커스 해제 알림
			ITCInteractable::Execute_OnUnfocus(CurrentBestTarget);
		}

		if (NewBestTarget)
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
		// 2명 이상 들고 있으면 가구를 던지지 않고 함수 종료
		if (FGS && FGS->GetGrabbedPlayers().Num() >= 2)
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
				return;
			}
		}

		// 대상 가구가 지금 잡을 수 있는 상태인지 검증 (정원 초과 등 확인)
		if (ITCInteractable::Execute_CanInteract(TargetActor, OwnerCharacter))
		{
			// 검증을 통과했다면 가구의 OnInteract 실행 (가구 쪽에서 물리 연결 처리)
			ITCInteractable::Execute_OnInteract(TargetActor, OwnerCharacter);
			// 방금 잡은 대상을 변수에 저장하여 기억
			GrabbedActor = TargetActor;
		}
	}
}