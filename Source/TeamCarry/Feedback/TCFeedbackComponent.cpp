// Fill out your copyright notice in the Description page of Project Settings.

#include "Feedback/TCFeedbackComponent.h"
#include "Feedback/TCFeedbackOverride.h"
#include "CatchCharacter/Furniture/FurnitureGrabSystem.h"
#include "Level/Vehicle/TCMovingTruck.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

namespace
{
	// 플레이스홀더 기본 에셋 (정식 에셋 도입 시 여기만 교체하거나 인스턴스에서 오버라이드)
	const TCHAR* DefaultPickupSound = TEXT("/Game/Developers/goldb/Audio/SW_Pickup.SW_Pickup");
	const TCHAR* DefaultDropSound = TEXT("/Game/Developers/goldb/Audio/SW_Drop.SW_Drop");
	const TCHAR* DefaultPickupFX = TEXT("/Game/Developers/goldb/VFX/NS_GrabPuff.NS_GrabPuff");
	const TCHAR* DefaultBreakSound = TEXT("/Game/Developers/goldb/Audio/SW_Impact.SW_Impact");
	const TCHAR* DefaultBreakFX = TEXT("/Game/Developers/goldb/VFX/NS_ImpactPuff.NS_ImpactPuff");
}

UTCFeedbackComponent::UTCFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f; // 상태 관찰용이라 20Hz면 충분
}

void UTCFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	// 데디서버는 코스메틱 없음
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		SetComponentTickEnabled(false);
		return;
	}

	// 잡힘 상태 소스 결정: ①GrabSystem 컴포넌트(TCFurnitureActor 계열) ②bIsGrabbed 리플렉션
	GrabSystem = Owner->FindComponentByClass<UFurnitureGrabSystem>();
	if (!GrabSystem)
	{
		GrabbedProp = CastField<FBoolProperty>(Owner->GetClass()->FindPropertyByName(TEXT("bIsGrabbed")));
	}
	if (!GrabSystem && !GrabbedProp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Feedback] %s: 잡힘 상태 소스 없음 — 관찰 비활성"), *GetNameSafe(Owner));
		SetComponentTickEnabled(false);
		return;
	}
	bLastGrabbed = ReadGrabbed();

	// 기본 에셋 로드 (인스턴스에서 지정했으면 유지)
	if (!PickupSound) { PickupSound = LoadObject<USoundBase>(nullptr, DefaultPickupSound); }
	if (!DropSound) { DropSound = LoadObject<USoundBase>(nullptr, DefaultDropSound); }
	if (!PickupFX) { PickupFX = LoadObject<UNiagaraSystem>(nullptr, DefaultPickupFX); }
	if (!BreakSound) { BreakSound = LoadObject<USoundBase>(nullptr, DefaultBreakSound); }
	if (!BreakFX) { BreakFX = LoadObject<UNiagaraSystem>(nullptr, DefaultBreakFX); }

	UE_LOG(LogTemp, Log, TEXT("[Feedback] %s 부착 완료 (sound: %s/%s, fx: %s)"), *GetNameSafe(Owner),
		PickupSound ? TEXT("O") : TEXT("X"), DropSound ? TEXT("O") : TEXT("X"), PickupFX ? TEXT("O") : TEXT("X"));
}

void UTCFeedbackComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner || (!GrabSystem && !GrabbedProp))
	{
		return;
	}

	const bool bGrabbed = ReadGrabbed();
	if (bGrabbed == bLastGrabbed)
	{
		return;
	}
	bLastGrabbed = bGrabbed;
	UE_LOG(LogTemp, Log, TEXT("[Feedback] %s 잡힘 전이: %s"), *GetNameSafe(Owner),
		bGrabbed ? TEXT("잡기") : TEXT("놓기"));

	// 소유자가 인터페이스로 거부하면 재생하지 않음
	if (Owner->Implements<UTCFeedbackOverride>() &&
		!ITCFeedbackOverride::Execute_ShouldAutoFeedback(Owner))
	{
		return;
	}

	const FVector Loc = GetFXLocation();
	if (bGrabbed)
	{
		if (PickupSound) { UGameplayStatics::PlaySoundAtLocation(this, PickupSound, Loc); }
		if (PickupFX)
		{
			// 들어올릴 때 바닥 먼지 — 액터 바운즈 밑면에서 스폰
			FVector Origin, Extent;
			Owner->GetActorBounds(false, Origin, Extent);
			const FVector Base(Origin.X, Origin.Y, Origin.Z - Extent.Z + 8.f);
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PickupFX, Base);
		}
	}
	else if (DropSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DropSound, Loc);
	}
}

void UTCFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 파괴 연출 — 액터가 게임 중 명시적으로 제거될 때 그 자리에서 재생.
	// 레벨 전환/PIE 종료는 제외하고, 트럭 근처 제거는 '적재'이므로 억제한다.
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (EndPlayReason == EEndPlayReason::Destroyed && Owner && World && World->IsGameWorld()
		&& GetNetMode() != NM_DedicatedServer)
	{
		bool bNearTruck = false;
		if (AActor* Truck = UGameplayStatics::GetActorOfClass(World, ATCMovingTruck::StaticClass()))
		{
			bNearTruck = FVector::Dist(Truck->GetActorLocation(), Owner->GetActorLocation()) < TruckSuppressRadius;
		}
		if (!bNearTruck)
		{
			const FVector Loc = Owner->GetActorLocation();
			if (BreakSound) { UGameplayStatics::PlaySoundAtLocation(World, BreakSound, Loc); }
			if (BreakFX) { UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, BreakFX, Loc); }
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool UTCFeedbackComponent::ReadGrabbed() const
{
	if (GrabSystem)
	{
		return GrabSystem->GetGrabbedPlayers().Num() > 0;
	}
	AActor* Owner = GetOwner();
	return (Owner && GrabbedProp) ? GrabbedProp->GetPropertyValue_InContainer(Owner) : false;
}

FVector UTCFeedbackComponent::GetFXLocation() const
{
	AActor* Owner = GetOwner();
	if (Owner && Owner->Implements<UTCFeedbackOverride>())
	{
		return ITCFeedbackOverride::Execute_GetFeedbackLocation(Owner);
	}
	return Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
}
