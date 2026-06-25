// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/Item/TCBatItem.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TCStunnable.h"
#include "Core/TCIDamageable.h"
#include "Core/TC_UsableItem.h" 
#include "Player/Character/TCPlayerCharacter.h"

#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

ATCBatItem::ATCBatItem()
{
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
}

void ATCBatItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATCBatItem, Holder);
}

// ── ITCInteractable ──
bool ATCBatItem::CanInteract_Implementation(ATCPlayerCharacter* /*Player*/)
{
	return Holder == nullptr; // 아무도 안 들었을 때만 줍기 가능
}

void ATCBatItem::OnFocus_Implementation() { Mesh->SetRenderCustomDepth(true); }
void ATCBatItem::OnUnfocus_Implementation() { Mesh->SetRenderCustomDepth(false); }

void ATCBatItem::OnInteract_Implementation(ATCPlayerCharacter* Player)
{
	// ServerTryInteract 경유 → 서버에서 호출됨
	if (!HasAuthority() || !Player) return;

	Holder = Player;
	AttachToHolder(Player);
}

void ATCBatItem::OnRep_Holder()
{
	AttachToHolder(Holder); // 클라에서도 어태치 반영
}

void ATCBatItem::AttachToHolder(AActor* NewHolder)
{
	if (NewHolder)
	{
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// 손 소켓에 어태치 (캐릭터 메시에 "WeaponSocket" 소켓 있다고 가정)
		if (ACharacter* Char = Cast<ACharacter>(NewHolder))
		{
			AttachToComponent(Char->GetMesh(),
				FAttachmentTransformRules::SnapToTargetIncludingScale,
				TEXT("WeaponSocket"));
		}
	}
	else
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetSimulatePhysics(true);
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}

// ── 휘두르기 ──
void ATCBatItem::Swing(AActor* User)
{
	if (!HasAuthority()) return;

	// 주인 앞쪽으로 구체 트레이스
	AActor* Wielder = Holder ? Holder.Get() : User;
	if (!Wielder) return;

	const FVector Start = Wielder->GetActorLocation();
	const FVector End = Start + Wielder->GetActorForwardVector() * SwingRange;

	TArray<FHitResult> Hits;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(SwingRadius);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Wielder);

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);

	GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjParams, Sphere, Params);

	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!Target || Target == Wielder) continue;

		// 강도 → 기절
		if (Target->Implements<UTCStunnable>())
		{
			ITCStunnable::Execute_ReceiveStun(Target, StunDuration, Wielder);
		}

		// 창문/벽 → 파손
		if (Target->Implements<UTCIDamageable>())
		{
			ITCIDamageable::Execute_ApplyDamage(Target, 1, Wielder);
		}
	}
}

void ATCBatItem::OnUse_Implementation(AActor* User)
{
	Swing(User);
}