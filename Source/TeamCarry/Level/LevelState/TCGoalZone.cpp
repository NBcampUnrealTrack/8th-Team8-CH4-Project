// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/LevelState/TCGoalZone.h"
#include "Components/BoxComponent.h"
#include "Core/MovableFurniture.h"  
#include "Level/StageManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ATCGoalZone::ATCGoalZone()
{
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(250.f, 250.f, 200.f));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
}

void ATCGoalZone::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		StageManager = Cast<AStageManager>(
			UGameplayStatics::GetActorOfClass(this, AStageManager::StaticClass()));

		Trigger->OnComponentBeginOverlap.AddDynamic(this, &ATCGoalZone::OnBeginOverlap);
		Trigger->OnComponentEndOverlap.AddDynamic(this, &ATCGoalZone::OnEndOverlap);

		GetWorldTimerManager().SetTimer(CheckTimer, this, &ATCGoalZone::CheckArrivals, CheckInterval, true);
	}
}

void ATCGoalZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CheckTimer);
	Super::EndPlay(EndPlayReason);
}

void ATCGoalZone::OnBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!HasAuthority() || !OtherActor) return;
	if (!OtherActor->Implements<UMovableFurniture>()) return;
	InZone.Add(OtherActor);
}

void ATCGoalZone::OnEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	if (!HasAuthority() || !OtherActor) return;
	InZone.Remove(OtherActor);
}

void ATCGoalZone::CheckArrivals()
{
	if (!HasAuthority() || !StageManager) return;

	for (auto It = InZone.CreateIterator(); It; ++It)
	{
		AActor* Furniture = It->Get();
		if (!Furniture) { It.RemoveCurrent(); continue; }   // 파괴된 액터 정리
		if (Delivered.Contains(Furniture))  continue; 

		// 놓였을 때만 도착 인정
		if (IMovableFurniture::Execute_IsGrabbed(Furniture)) continue; 

		Delivered.Add(Furniture);
		StageManager->RegisterDelivery(Furniture);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
				FString::Printf(TEXT("도착: %s (%d/%d)"),
					*GetNameSafe(Furniture),
					StageManager->GetDeliveredCount(),
					StageManager->GetTargetCount()));
		}
	}
}

void ATCGoalZone::OnFurnitureStolen(AActor* Furniture)
{
	if (!HasAuthority()) return;
	Delivered.Remove(Furniture);   
	InZone.Remove(Furniture);
	if (StageManager) { StageManager->UnregisterDelivery(Furniture); }
}

AActor* ATCGoalZone::GetAnyDeliveredFurniture() const
{
	for (const TObjectPtr<AActor>& Furniture : Delivered)
	{
		if (Furniture) { return Furniture; }
	}
	return nullptr;
}