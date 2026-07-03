// Fill out your copyright notice in the Description page of Project Settings.

#include "FurnitureDamage.h"
#include "FurnitureStat.h"
#include "Kismet/GameplayStatics.h"

UFurnitureDamage::UFurnitureDamage()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFurnitureDamage::BeginPlay()
{
	Super::BeginPlay();

	PreviousLocation = GetOwner()->GetActorLocation();
}

void UFurnitureDamage::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 서버에서만 처리함
	if (GetOwner()->HasAuthority())
	{
		CalculateVelocity(DeltaTime);
	}
}

void UFurnitureDamage::Setup(UFurnitureStat* InStat)
{
	FurnitureStat = InStat;
}

void UFurnitureDamage::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,FVector NormalImpulse, const FHitResult& Hit)
{
	// 충돌데미지 처리는 서버에서! 그리고 스텟도 등록되어있어야함
	AActor* Owner = GetOwner();
	if (!Owner->HasAuthority() || !FurnitureStat)
		return;

	FString NetMode = Owner->HasAuthority() ? TEXT("Server") : TEXT("Client");

	// TODO : 플레이어나 특정 사물에는 부딪쳐도 데미지 안입으려면 조건논의 필요
	// 타겟이 아닌 대상 : 부딪친대상이 존재해야함, 자기자신
	if (OtherActor && OtherActor == this->GetOwner())
	{
		return;
	}

	float ImpactSpeed = 0.f;

	if (HitComp && HitComp->IsSimulatingPhysics())
	{
		// 물리 충돌: 충격량 ÷ 질량 = 실제 접촉 속도 변화량.
		const float Mass = HitComp->GetMass();
		ImpactSpeed = (Mass > KINDA_SMALL_NUMBER) ? NormalImpulse.Size() / Mass : 0.f;
	}
	else
	{
		// 운반 중 스윕 충돌:
		// 벽에 박힐당시 충격량 = 내적을 통해 연산.a와 b의 내적 = 충격자의 의한 벽에 수직인 벡터
		// 노말 벡터 -한이유 = 그냥 하면 둔각이라서 -값나옴
		ImpactSpeed = FMath::Max(0.f, FVector::DotProduct(CurrentVelocity, -Hit.ImpactNormal));
	}

	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("충격 속도 : %f (최소 요구: %f)"), ImpactSpeed, MinImpactSpeedForDamage));
	// 데미지 배율 = 충격량 * 가구의 데미지 배율
	float Damage = ImpactSpeed * FurnitureStat->GetCollisionDamageMultiplier();

	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("피해량 : %f"), Damage));

	if (MinImpactSpeedForDamage < ImpactSpeed)
	{
		Damage *= DamagePerImpactSpeed;

		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("[%s] ApplyDamage 호출! 데미지: %f"), *NetMode, Damage));

		UGameplayStatics::ApplyDamage(
			Owner,							// 맞은 녀석 : 자기자신
			Damage,							// 데미지 수치 : 충격량
			nullptr,						// 때린 녀석의 뇌 : 없음
			OtherActor,                     // 때린 녀석의 몸통 : 보통은 벽일듯
			UDamageType::StaticClass()		// 데미지 속성 : 기본 데미지
		);
	}
}

void UFurnitureDamage::CalculateVelocity(float DeltaTime)
{
	// 속도 = 이동량 / 시간(m/s)
	CurrentVelocity = (GetOwner()->GetActorLocation() - PreviousLocation) / DeltaTime;
	PreviousLocation = GetOwner()->GetActorLocation();
}
