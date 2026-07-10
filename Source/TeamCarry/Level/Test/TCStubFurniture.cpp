// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/Test/TCStubFurniture.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ATCStubFurniture::ATCStubFurniture()
{
	bReplicates = true;
	SetReplicateMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	// 생성자(CDO 포함)에서 SetSimulatePhysics(true)를 부르면 GEngine 미초기화 상태에서
	// 물리 머티리얼을 조회해 에러가 찍히고, 쿠커가 이를 실패로 집계해 패키징이 깨진다.
	// 리플렉션 프로퍼티에 직접 기록하면 동일한 기본값이 CDO/인스턴스에 전파된다.
	Mesh->BodyInstance.bSimulatePhysics = true;
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) { Mesh->SetStaticMesh(Cube.Object); Mesh->SetWorldScale3D(FVector(0.5f)); }
}