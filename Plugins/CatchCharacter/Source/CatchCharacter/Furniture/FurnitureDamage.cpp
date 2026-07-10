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
	PreviousQuat     = GetOwner()->GetActorQuat();

	// 스폰 직후 낙하/배치 접촉으로 즉시 데미지 입는 것 방지 (서버 전용)
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SetInvincible(3.f);
	}
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

	// 완전무적상태 확인
	if (bIsSuperInvincible)
		return;

	// (무적 체크는 아래로 이동 — 강한 물리 충격은 무적 관통시키기 위해 ImpactSpeed 계산 후 판정)

	FString NetMode = Owner->HasAuthority() ? TEXT("Server") : TEXT("Client");

	// TODO : 플레이어나 특정 사물에는 부딪쳐도 데미지 안입으려면 조건논의 필요
	// 타겟이 아닌 대상 : 부딪친대상이 존재해야함, 자기자신
	if (OtherActor && OtherActor == this->GetOwner())
	{
		return;
	}

	// 이미 파괴된 가구(체력 0)와의 충돌은 무시 — 파괴 조각/잔해에 맞아 데미지 입는 것 방지
	if (OtherActor)
	{
		if (UFurnitureStat* OtherStat = OtherActor->FindComponentByClass<UFurnitureStat>())
		{
			if (OtherStat->GetCurrentHealth() <= 0.f)
			{
				return;
			}
		}
	}

	// 충돌 세기(cm/s). 질량은 데미지에 영향 없음 — 가구별 위력은 CollisionDamageMultiplier로 조절
	float ImpactSpeed = 0.f;
	const bool bPhysicsImpact = (HitComp && HitComp->IsSimulatingPhysics());   // 공중 낙하·던짐 여부

	if (HitComp && HitComp->IsSimulatingPhysics())
	{
		// 물리 충돌: 충격량 ÷ 질량 = 실제 접촉 속도 변화량 (질량 정규화 → 무게 무관 지표)
		const float Mass = HitComp->GetMass();
		ImpactSpeed = (Mass > KINDA_SMALL_NUMBER) ? NormalImpulse.Size() / Mass : 0.f;
	}
	else if (HitComp)
	{
		// 운반 중 스윕 충돌: 충돌 지점의 실제 속도 = 중심 속도 + 회전 접선 속도(ω × r)
		// 제자리 회전이라도 끝단은 ω·r 속도로 움직이므로 회전 충돌 데미지가 반영됨
		// ω는 3축 각속도 벡터라 Yaw 회전·Pitch 기울이기 모두 포함 (회전 없으면 ω=0 → 중심 속도만)
		const FVector R = Hit.ImpactPoint - Owner->GetActorLocation();
		const FVector PointVelocity = CurrentVelocity + FVector::CrossProduct(CurrentAngularVelocityRad, R);

		// 접점 속도의 벽 법선 방향 성분 = 실제 충돌 세기 (스치는 방향 성분은 제외)
		ImpactSpeed = FMath::Max(0.f, FVector::DotProduct(PointVelocity, -Hit.ImpactNormal));
	}

	// 무적 판정: 무적이어도 '강한 물리 충격'(던짐·큰 낙하)은 관통 → 던짐이 즉시 데미지 등록.
	// 그 외(살짝 놓기·잔접촉·운반 부딪침)는 무적으로 보호.
	if (bIsInvincible)
	{
		const bool bHardPhysicsHit = bPhysicsImpact && (ImpactSpeed >= InvincibilityBypassSpeed);
		if (!bHardPhysicsHit)
			return;
	}

	// 데미지 = 충돌 속도 × 가구의 데미지 배율
	float Damage = ImpactSpeed * FurnitureStat->GetCollisionDamageMultiplier();

	// 공중 낙하·던짐 충격만 강화 (운반 중 부딪침은 배율 1 유지)
	if (bPhysicsImpact)
		Damage *= PhysicsImpactDamageScale;

	if (MinImpactSpeedForDamage < ImpactSpeed)
	{
		Damage *= DamagePerImpactSpeed;

		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("충격 속도 : %f (최소 요구: %f)"), ImpactSpeed, MinImpactSpeedForDamage));
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("피해량 : %f"), Damage));
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("[%s] ApplyDamage 호출! 데미지: %f"), *NetMode, Damage));

		UGameplayStatics::ApplyDamage(
			Owner,							// 맞은 녀석 : 자기자신
			Damage,							// 데미지 수치 : 충격량
			nullptr,						// 때린 녀석의 뇌 : 없음
			OtherActor,                     // 때린 녀석의 몸통 : 보통은 벽일듯
			UDamageType::StaticClass()		// 데미지 속성 : 기본 데미지
		);

		// 피해를 입혔으므로 0.5초 동안 무적 (연쇄 충돌 완충)
		SetInvincible(0.5f);
	}
}

void UFurnitureDamage::SetInvincible(float Duration)
{
	if (GetOwner() && !GetOwner()->HasAuthority())
		return;

	if (Duration <= 0.f)
	{
		DisableInvincible();
		return;
	}

	bIsInvincible = true;

	if (GetWorld())
	{
		if (GetWorld()->GetTimerManager().IsTimerActive(InvincibilityTimerHandle))
		{
			GetWorld()->GetTimerManager().ClearTimer(InvincibilityTimerHandle);
		}

		GetWorld()->GetTimerManager().SetTimer(
			InvincibilityTimerHandle,
			this,
			&UFurnitureDamage::DisableInvincible,
			Duration,
			false
		);
	}
}

void UFurnitureDamage::SetSuperInvincible(bool Active)
{
	if (GetOwner() && !GetOwner()->HasAuthority())
		return;

	bIsSuperInvincible = Active;
}

void UFurnitureDamage::DisableInvincible()
{
	if (GetOwner() && !GetOwner()->HasAuthority())
		return;

	bIsInvincible = false;

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(InvincibilityTimerHandle);
	}
}

void UFurnitureDamage::CalculateVelocity(float DeltaTime)
{
	// 속도 = 이동량 / 시간(cm/s)
	CurrentVelocity = (GetOwner()->GetActorLocation() - PreviousLocation) / DeltaTime;
	PreviousLocation = GetOwner()->GetActorLocation();

	// 3축 각속도 = 회전 변화량(쿼터니언) / 시간. 스윕 분기의 접점 속도(ω × r) 계산에 사용
	// Yaw·Pitch·Roll 어떤 축의 회전이든 하나의 각속도 벡터로 잡힘
	const FQuat CurQuat = GetOwner()->GetActorQuat();
	FQuat DeltaQuat = CurQuat * PreviousQuat.Inverse();
	// 최단 경로 보정 (W<0이면 반대 방향 장회전으로 해석되는 것 방지)
	if (DeltaQuat.W < 0.f)
	{
		DeltaQuat = FQuat(-DeltaQuat.X, -DeltaQuat.Y, -DeltaQuat.Z, -DeltaQuat.W);
	}
	FVector Axis;
	float AngleRad;
	DeltaQuat.ToAxisAndAngle(Axis, AngleRad);
	CurrentAngularVelocityRad = Axis * (AngleRad / DeltaTime);
	PreviousQuat = CurQuat;
}
