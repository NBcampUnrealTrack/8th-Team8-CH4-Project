// Fill out your copyright notice in the Description page of Project Settings.





#include "GrabActorComponent.h"

#include "FuniturePawn.h"

#include "FurniturePawn2.h"

#include "FurniturePawn3.h"

#include "GameFramework/Character.h"

#include "Engine/World.h"

#include "GameFramework/CharacterMovementComponent.h"





#include "CatchCharacter/Furniture/FurnitureActor.h"
#include "CatchCharacter/Furniture/FurnitureGrabSystem.h"

UGrabActorComponent::UGrabActorComponent()

{

	PrimaryComponentTick.bCanEverTick = false;



	// 충돌 설정 (Trace에는 걸리지만 물리적 충돌은 하지 않음)

	SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InitSphereRadius(40.f);



	HeldFurniture = nullptr;

}



void UGrabActorComponent::ToggleGrab()

{

	// 1. 이미 잡고 있다면 해제 요청

	if (HeldFurniture)

	{

		UE_LOG(LogTemp, Log, TEXT("[GrabActor] 상호작용 종료 요청"));

		ServerRPCRelease();

		HeldFurniture = nullptr;

		return;

	}



	// 2. 주변 가구 탐색 (클라이언트/서버 공통)

	FVector StartLocation = GetComponentLocation();

	FVector EndLocation = StartLocation + (GetForwardVector() * 150.f);

	FCollisionShape TraceSphere = FCollisionShape::MakeSphere(40.f);



	FHitResult HitResult;

	FCollisionQueryParams TraceParams;

	TraceParams.AddIgnoredActor(GetOwner());



	bool bHit = GetWorld()->SweepSingleByChannel(

		HitResult,

		StartLocation,

		EndLocation,

		FQuat::Identity,

		ECC_Visibility,

		TraceSphere,

		TraceParams

	);



	if (bHit && HitResult.GetActor())

	{

		AActor* HitActor = HitResult.GetActor();

		

		// FurniturePawn 확인 (괄호 오류 수정)

		if (HitActor->IsA(AFuniturePawn::StaticClass()) || HitActor->IsA(AFurniturePawn2::StaticClass()) || HitActor->IsA(AFurniturePawn3::StaticClass()) || HitActor->IsA(AFurnitureActor::StaticClass()))

		{

			UE_LOG(LogTemp, Log, TEXT("[GrabActor] 가구 발견: %s"), *HitActor->GetName());

			ServerRPCGrab(HitActor);

			HeldFurniture = HitActor;

		}

	}

}



bool UGrabActorComponent::ServerRPCGrab_Validate(AActor* Furniture)

{

	return Furniture != nullptr;

}



void UGrabActorComponent::ServerRPCGrab_Implementation(AActor* Furniture)

{

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());

	if (!OwnerCharacter || !Furniture) return;



	// FurniturePawn 처리

	if (AFuniturePawn* FP1 = Cast<AFuniturePawn>(Furniture))

	{

		UE_LOG(LogTemp, Log, TEXT("[GrabActor] 서버: AFuniturePawn 잡기 실행"));

		FP1->Grab(OwnerCharacter, this);

		HeldFurniture = Furniture;

	}

	// FurniturePawn2 처리

	else if (AFurniturePawn2* FP2 = Cast<AFurniturePawn2>(Furniture))

	{

		UE_LOG(LogTemp, Log, TEXT("[GrabActor] 서버: AFurniturePawn2 잡기 실행"));

		FP2->Grab(OwnerCharacter, FVector(0,0,40.f), this);

		HeldFurniture = Furniture;

	}



	// FurniturePawn3 처리

	else if (AFurniturePawn3* FP3 = Cast<AFurniturePawn3>(Furniture))

	{

		UE_LOG(LogTemp, Log, TEXT("[GrabActor] 서버: AFurniturePawn3 잡기 실행"));

		FP3->Grab(OwnerCharacter, this);

		HeldFurniture = Furniture;

	}

	// FurnitureActor 처리 (Component System)

	else if (AFurnitureActor* FA = Cast<AFurnitureActor>(Furniture))

	{

		UE_LOG(LogTemp, Log, TEXT("[GrabActor] 서버: AFurnitureActor 잡기 실행"));

		if (UFurnitureGrabSystem* GS = FA->GetGrabSystem())

		{

			GS->Grab(OwnerCharacter, FVector(0,0,40.f), this);

		}

		HeldFurniture = Furniture;

	}

}



bool UGrabActorComponent::ServerRPCRelease_Validate()

{

	return true;

}



void UGrabActorComponent::ServerRPCRelease_Implementation()

{

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());

	if (!OwnerCharacter || !HeldFurniture) return;



	if (AFuniturePawn* FP1 = Cast<AFuniturePawn>(HeldFurniture))

	{

		UE_LOG(LogTemp, Log, TEXT("[GrabActor] 서버: AFuniturePawn 놓기 실행"));

		FP1->Release(OwnerCharacter);

	}

	else if (AFurniturePawn2* FP2 = Cast<AFurniturePawn2>(HeldFurniture))

	{

		UE_LOG(LogTemp, Log, TEXT("[GrabActor] 서버: AFurniturePawn2 놓기 실행"));

		FP2->Release(OwnerCharacter);

	}

	else if (AFurnitureActor* FA = Cast<AFurnitureActor>(HeldFurniture))

	{

		UE_LOG(LogTemp, Log, TEXT("[GrabActor] 서버: AFurnitureActor 놓기 실행"));

		if (UFurnitureGrabSystem* GS = FA->GetGrabSystem())

		{

			GS->Release(OwnerCharacter);

		}

	}



	HeldFurniture = nullptr;

}



void UGrabActorComponent::ClientRPC_ForcePosition_Implementation(FVector TargetLocation, FRotator TargetRotation)

{

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());

	// 자기가 조종하는 클라이언트 화면에서만 강제 업데이트 적용

	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled() && !GetOwner()->HasAuthority())

	{

		OwnerCharacter->SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);

	}

}


