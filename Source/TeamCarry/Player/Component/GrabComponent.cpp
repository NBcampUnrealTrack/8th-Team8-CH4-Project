
// GrabComponent.cpp

#include "Player/Component/GrabComponent.h"
#include "Player/Character/TCPlayerCharacter.h"
#include "Player/Interface/TCInteractable.h"
#include "Kismet/KismetSystemLibrary.h"

// 틱 활성화 여부 및 초기화
UGrabComponent::UGrabComponent()
{
	// 매 프레임 스캔을 위해 틱 활성화
	PrimaryComponentTick.bCanEverTick = true;
	CurrentBestTarget = nullptr;

}

// 매 프레임마다 상호작용 대상 탐색
void UGrabComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 로컬 플레이어(내 화면)에서만 트레이스를 쏘도록 제한
	if (GetOwner()->GetLocalRole() == ROLE_AutonomousProxy || GetOwner()->GetLocalRole() == ROLE_Authority)
	{
		ScanBestTarget();
	}

}

// 대상이 존재하면 서버로 상호작용-잡기 시도 요청
void UGrabComponent::TryInteract()
{
	// 현재 내 화면에 잡힌 타겟이 있다면 서버로 해당 타겟을 전송
	if (CurrentBestTarget)
	{
		ServerTryInteract(CurrentBestTarget);
	}
}

// 서버로 상호작용-던지기 시도 요청
void UGrabComponent::TryThrow()
{
	// 나중에 가구 액터가 들어오면 TryInteract()처럼 if문 추가하고 수정할 예정
	// 지금은 뼈대만 설계해서 바로 서버로 던지기 요청
	ServerTryThrow();
}

// 멀티 박스 트레이스 발사해서 최적 대상 판별
void UGrabComponent::ScanBestTarget()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	// 박스 트레이스 범위 설정 (200cm, 50x50x50)
	FVector Start = OwnerActor->GetActorLocation();
	FVector ForwardVector = OwnerActor->GetActorForwardVector();
	FVector End = Start + (ForwardVector * 200.0f);
	FVector HalfSize = FVector(50.f, 50.f, 50.f);

	// 충돌 검사 결과를 담기 위한 배열
	TArray<FHitResult> HitResults;

	// 충돌 검사에서 무시할 액터 배열에 자신 추가하기
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(OwnerActor); // 자기 자신은 탐색에서 제외

	// 멀티 박스 트레이스 발사
	bool bHit = UKismetSystemLibrary::BoxTraceMulti(
		this, Start, End, HalfSize, OwnerActor->GetActorRotation(),
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false, ActorsToIgnore, EDrawDebugTrace::ForOneFrame, // 디버그 선 보려면 수정(None, ForOneFrame)
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

// Server - 상호작용-던지기 실행
void UGrabComponent::ServerTryThrow_Implementation()
{
	// 나중에 가구와 연결되면 수정할 예정
	// 전방으로 AddImpulse(힘)을 가해서 날리는 로직 작성할 예정
	UE_LOG(LogTemp, Warning, TEXT("[Server] 던지기 실행 준비 완료"));
}

// Server - 상호작용-잡기 실행
void UGrabComponent::ServerTryInteract_Implementation(AActor* TargetActor)
{
	// 서버 검증
	if (TargetActor && TargetActor->Implements<UTCInteractable>())
	{
		ATCPlayerCharacter* OwnerCharacter = Cast<ATCPlayerCharacter>(GetOwner());

		// 대상 가구가 지금 잡을 수 있는 상태인지 검증 (정원 초과 등 확인)
		if (ITCInteractable::Execute_CanInteract(TargetActor, OwnerCharacter))
		{
			// 검증을 통과했다면 가구의 OnInteract 실행 (가구 쪽에서 물리 연결 처리)
			ITCInteractable::Execute_OnInteract(TargetActor, OwnerCharacter);
		}
	}
}

