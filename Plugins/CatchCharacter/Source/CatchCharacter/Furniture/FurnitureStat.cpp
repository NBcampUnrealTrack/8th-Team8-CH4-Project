#include "FurnitureStat.h"
#include "Net/UnrealNetwork.h"

UFurnitureStat::UFurnitureStat()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	CurrentHealth = 100.f;
	MaxHealth = 100.f;
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

	// 서버에서만 데미지를 처리하도록 바인딩
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UFurnitureStat::TakeDamage);
	}
}

void UFurnitureStat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UFurnitureStat, CurrentHealth);
	DOREPLIFETIME(UFurnitureStat, MaxHealth);
	DOREPLIFETIME(UFurnitureStat, RowName);
	DOREPLIFETIME(UFurnitureStat, RequiredPlayer);
	DOREPLIFETIME(UFurnitureStat, BaseSpeed);
	DOREPLIFETIME(UFurnitureStat, CollisionDamageMultiplier);
	DOREPLIFETIME(UFurnitureStat, CurrentGrabbedPlayer);
	DOREPLIFETIME(UFurnitureStat, Mass);
	DOREPLIFETIME(UFurnitureStat, Friction);
	DOREPLIFETIME(UFurnitureStat, Price);
}

void UFurnitureStat::InitializeStats(const FFurnitureData& Data, FName InRowName)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) return;

	DefaultStats = Data;
	RowName = InRowName;

	CurrentHealth = Data.MaxHealth;
	MaxHealth = Data.MaxHealth;
	RequiredPlayer = Data.RequiredPlayer;
	BaseSpeed = Data.BaseSpeed;
	CollisionDamageMultiplier = Data.CollisionDamageMultiplier;
	Mass = Data.Mass;
	Friction = Data.Friction;
	Price = Data.Price;
}

void UFurnitureStat::UpdateGrabbedPlayers(int32 Count)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) return;

	CurrentGrabbedPlayer = Count;
}

void UFurnitureStat::TakeDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* Instigator, AActor* Causer)
{
	// 서버에서만 처리해줘야함
	if (GetOwner() && !GetOwner()->HasAuthority())
		return;

	// 체력이 이미 0이하거나 데미지가 없다면 처리x
	// (충돌 무적 판정은 UFurnitureDamage::OnHit에서 처리)
	if (Damage <= 0.f || CurrentHealth <= 0.f)
		return;

	float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Max(0.f, CurrentHealth - Damage);


	// 서버에서 즉시 브로드캐스트 (OnRep은 서버에서 안 돌므로 서버 시각 처리는 여기서)
	OnFurnitureDamage.Broadcast(MaxHealth, PreviousHealth, CurrentHealth);
	if (CurrentHealth <= 0.f)
	{
		OnFurnitureDestroy.Broadcast();
	}
}

void UFurnitureStat::OnRep_CurrentHealth(float OldHealth)
{
	// 클라에서 체력 복제 시 호출. 서버와 동일하게 OnFurnitureDamage를 브로드캐스트해
	// 액터의 금 표시 등 시각 처리를 모든 머신에서 통일. (파괴는 서버 Multicast가 별도 처리)
	OnFurnitureDamage.Broadcast(MaxHealth, OldHealth, CurrentHealth);
}