#include "FurnitureStat.h"
#include "Net/UnrealNetwork.h"

UFurnitureStat::UFurnitureStat()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	CurrentHealth = 100.f;
	RequiredPlayer = 1;
	BaseSpeed = 100.f;
	CollisionDamageMultiplier = 10.f;
	CurrentGrabbedPlayer = 0;
	Mass = 200.f;
	Friction = 4.f;
}

void UFurnitureStat::BeginPlay()
{
	Super::BeginPlay();
}

void UFurnitureStat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UFurnitureStat, CurrentHealth);
	DOREPLIFETIME(UFurnitureStat, RequiredPlayer);
	DOREPLIFETIME(UFurnitureStat, BaseSpeed);
	DOREPLIFETIME(UFurnitureStat, CollisionDamageMultiplier);
	DOREPLIFETIME(UFurnitureStat, CurrentGrabbedPlayer);
	DOREPLIFETIME(UFurnitureStat, Mass);
	DOREPLIFETIME(UFurnitureStat, Friction);
}

void UFurnitureStat::InitializeStats(const FFurnitureData& Data)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) return;

	DefaultStats = Data;
	
	CurrentHealth = Data.MaxHealth;
	RequiredPlayer = Data.RequiredPlayer;
	BaseSpeed = Data.BaseSpeed;
	CollisionDamageMultiplier = Data.CollisionDamageMultiplier;
	Mass = Data.Mass;
	Friction = Data.Friction;
}

void UFurnitureStat::UpdateGrabbedPlayers(int32 Count)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) return;

	CurrentGrabbedPlayer = Count;
}
