#include "Level/AI/TCRobberCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

ATCRobberCharacter::ATCRobberCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	CarrySocket = CreateDefaultSubobject<USceneComponent>(TEXT("CarrySocket"));
	CarrySocket->SetupAttachment(GetRootComponent());
	CarrySocket->SetRelativeLocation(FVector(60.f, 0.f, 0.f));

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 400.f;
}

void ATCRobberCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATCRobberCharacter, State);
	DOREPLIFETIME(ATCRobberCharacter, CarriedFurniture);
}

// 짐 집기/놓기 (서버)
void ATCRobberCharacter::GrabFurniture(AActor* Furniture)
{
	if (!HasAuthority() || !Furniture) return;
	if (State == ERobberState::Stunned || CarriedFurniture) return; // 기절 중엔 못 집도록

	CarriedFurniture = Furniture;
	AttachFurniture(Furniture);
	UpdateCarryState();
}

void ATCRobberCharacter::ReleaseFurniture()
{
	if (!HasAuthority() || !CarriedFurniture) return;

	DetachFurniture(CarriedFurniture);
	CarriedFurniture = nullptr;
	UpdateCarryState();
}

// 기절 진입점 (서버 권위)
void ATCRobberCharacter::ApplyStun(float Duration)
{
	if (!HasAuthority()) return;
	if (State == ERobberState::Stunned) return; // 이미 기절이면 무시(추후 기획방향을 통해 시간 연장으로 변경가능)

	const float StunTime = (Duration > 0.f) ? Duration : DefaultStunDuration;

	// 1.짐 놓침
	if (CarriedFurniture)
	{
		DetachFurniture(CarriedFurniture);
		CarriedFurniture = nullptr;
	}

	// 2.상태 전환시 복제,OnRep_State에서 각 클라 래그돌
	State = ERobberState::Stunned;

	// 3.서버: 이동 정지 + AI 정지
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AICon->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Stunned"));
		}
	}

	// 4.서버에서도 래그돌 시각 적용 
	SetRagdoll(true);

	// 5.회복 기간
	GetWorldTimerManager().SetTimer(StunTimer, this, &ATCRobberCharacter::RecoverFromStun, StunTime, false);
}

void ATCRobberCharacter::RecoverFromStun()
{
	if (!HasAuthority()) return;

	// 래그돌 해제 + 이동 복구
	SetRagdoll(false);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	State = ERobberState::Normal; // OnRep_State에서 클라도 래그돌 해제

	// AI 재가동
	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AICon->GetBrainComponent())
		{
			Brain->RestartLogic();
		}
	}
}

// 상태는 서버에서 자동으로 판정
void ATCRobberCharacter::UpdateCarryState()
{
	if (State == ERobberState::Stunned) return; // 기절이 최우선
	State = CarriedFurniture ? ERobberState::Carrying : ERobberState::Normal;
}

// OnRep:클라 시각 반영
void ATCRobberCharacter::OnRep_State()
{
	// 상태에 맞춰 각 클라에서 래그돌 on/off
	SetRagdoll(State == ERobberState::Stunned);
}

void ATCRobberCharacter::OnRep_Carried()
{
	// 서버가 Attach한 결과는 어태치 자체가 복제되지만,
	// 혹시 물리 on/off 같은 로컬 처리가 필요하면 여기서 구현되도록 확장성
	if (CarriedFurniture && !PrevCarried) { AttachFurniture(CarriedFurniture); }
	else if (!CarriedFurniture && PrevCarried) { DetachFurniture(PrevCarried); }
	PrevCarried = CarriedFurniture;
}

// Attach/Detach 구현 
void ATCRobberCharacter::AttachFurniture(AActor* Furniture)
{
	if (!Furniture) return;

	// 짐 물리 끄고 소켓에 부착 (떨어지지 않게)
	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Furniture->GetRootComponent()))
	{
		Root->SetSimulatePhysics(false);
		Root->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 끌고 갈 때 걸리적 안 거리게
	}
	Furniture->AttachToComponent(CarrySocket, FAttachmentTransformRules::SnapToTargetIncludingScale);
}

void ATCRobberCharacter::DetachFurniture(AActor* Furniture)
{
	if (!Furniture) return;

	Furniture->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Furniture->GetRootComponent()))
	{
		Root->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Root->SetSimulatePhysics(true); // 다시 물리,바닥에 떨어짐
	}
}

// 래그돌 on/off
void ATCRobberCharacter::SetRagdoll(bool bEnable)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp) return;

	if (bEnable)
	{
		// 캡슐 충돌 끄고 메시 물리 시뮬 (몸 축 처짐)
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetAllBodiesSimulatePhysics(true);
		MeshComp->SetSimulatePhysics(true);
		MeshComp->WakeAllRigidBodies();
	}
	else
	{
		// 물리 끄고 캡슐로 복귀
		MeshComp->SetAllBodiesSimulatePhysics(false);
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MeshComp->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		// 메시를 캡슐 기준 원위치로 (BeginPlay에서 캐시한 값으로 복원하면 더 정확)
	}
}
