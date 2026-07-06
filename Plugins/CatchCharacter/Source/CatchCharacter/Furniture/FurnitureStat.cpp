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

	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
		//FString::Printf(TEXT("가구 최대 체력 : %f / 남은 체력 : %f"), DefaultStats.MaxHealth, CurrentHealth));

	OnFurnitureDamage.Broadcast(DefaultStats.MaxHealth, PreviousHealth, CurrentHealth);
	if (CurrentHealth <= 0.f)
	{
		OnFurnitureDestroy.Broadcast();
	}
}