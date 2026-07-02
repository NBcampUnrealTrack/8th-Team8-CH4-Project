// Fill out your copyright notice in the Description page of Project Settings.


#include "Furniture/TCFurnitureActor.h"
#include "CatchCharacter/Furniture/FurnitureGrabSystem.h"
#include "CatchCharacter/Furniture/FurnitureStat.h"
#include "Player/Character/TCPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Player/Component/GrabComponent.h"
#include "EngineUtils.h"

ATCFurnitureActor::ATCFurnitureActor()
{
    // 지오메트리 컬렉션 컴포넌트 생성
    GeometryCollectionComp = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollectionComp"));

    if (RootComponent)
    {
        GeometryCollectionComp->SetupAttachment(RootComponent);
    }

    // 초기에는 보이지 않고, 물리도 꺼진 상태로 설정
    GeometryCollectionComp->SetVisibility(false);
    GeometryCollectionComp->SetSimulatePhysics(false);
    GeometryCollectionComp->SetCollisionProfileName(TEXT("NoCollision"));
}

void ATCFurnitureActor::BeginPlay()
{
    Super::BeginPlay();

    // 파괴됨을 감지 (서버에서만 바인딩)
    if (HasAuthority() && GetFurnitureStat())
    {
        GetFurnitureStat()->OnFurnitureDestroy.AddDynamic(this, &ATCFurnitureActor::DestroyFurniture);
    }
}

void ATCFurnitureActor::DestroyFurniture()
{
    // 서버에서만 처리
    if (HasAuthority())
    {
        bIsFurnitureDestroyed = true;

        // 가구 잡기 해제
        if (UFurnitureGrabSystem* FGS = GetGrabSystem())
        {
            // 플레이어가 직접 키를 눌러 내려놓은 것처럼 UGrabComponent의 TryInteract호출
            // 이렇게 하면 정상적으로 놓음처리됨
            for (TActorIterator<ATCPlayerCharacter> It(GetWorld()); It; ++It)
            {
                ATCPlayerCharacter* Player = *It;
                if (FGS->IsGrabbedBy(Player))
                {
                    if (UActorComponent* Comp = Player->GetComponentByClass(UGrabComponent::StaticClass()))
                    {
                        UGrabComponent* GrabComp = Cast<UGrabComponent>(Comp);
                        if (GrabComp)
                        {
                            GrabComp->TryInteract();
                        }
                    }
                }
            }
            
            // 만약 놓쳐진 경우가 있다면 대비하여 확실히 전체 해제
            FGS->AllRelease();
        }

        // 파괴 효과
        Multicast_DestroyFurniture();

        // 파괴 메쉬가 없다면 액터를 완전히 삭제 예약 (다른작업의 처리 시간 확보를 위해 0.1초 지연)
        if (GeometryCollectionComp && GeometryCollectionComp->GetRestCollection())
        {
            // 조각나고나서 5초후 삭제
            SetLifeSpan(5.f);
        }
        else
        {
            SetLifeSpan(0.1f);
        }
    }
}

void ATCFurnitureActor::Multicast_DestroyFurniture_Implementation()
{
    // 파괴음 재생
    if (BreakSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, BreakSound, GetActorLocation());
    }

    if (FurnitureMesh)
    {
        // 기존 멀쩡한 메쉬는 숨기고 물리/충돌을 해제
        FurnitureMesh->SetVisibility(false);
        FurnitureMesh->SetSimulatePhysics(false);
        FurnitureMesh->SetCollisionProfileName(TEXT("NoCollision"));
    }

    if (GeometryCollectionComp && GeometryCollectionComp->GetRestCollection())
    {
        // 지오메트리 컬렉션과 물리를 킨다
        GeometryCollectionComp->SetVisibility(true);
        GeometryCollectionComp->SetSimulatePhysics(true);

        // 파괴용 콜리전 프로파일을 사용
        GeometryCollectionComp->SetCollisionProfileName(TEXT("Destructible"));
    }
}

bool ATCFurnitureActor::CanInteract_Implementation(ATCPlayerCharacter* Player)
{
    // 이미 파괴된 가구라면 상호작용 불가능
    if (bIsFurnitureDestroyed)
        return false;

    // 가구 잡기 시스템이 존재해야함
    UFurnitureGrabSystem* FGS = GetGrabSystem();
    if (!FGS || !Player)
        return false;

    // 내가 이미 잡고 있으면 OR 빈자리 있으면 true
    return FGS->IsGrabbedBy(Player) || FGS->CanAcceptGrab();
}

void ATCFurnitureActor::OnInteract_Implementation(ATCPlayerCharacter* Player)
{
    // 가구 잡기 시스템이 존재해야함
    UFurnitureGrabSystem* FGS = GetGrabSystem();
    if (!FGS || !Player)
        return;

    if (FGS->IsGrabbedBy(Player))
        FGS->Release(Player);
    else                         
        FGS->Grab(Player, GrabLiftHeight);
}

void ATCFurnitureActor::OnFocus_Implementation() 
{ 
    SetHighlight(true); 
}
void ATCFurnitureActor::OnUnfocus_Implementation() 
{ 
    SetHighlight(false); 
}