// Fill out your copyright notice in the Description page of Project Settings.


#include "Furniture/TCFurnitureActor.h"
#include "CatchCharacter/Furniture/FurnitureGrabSystem.h"
#include "CatchCharacter/Furniture/FurnitureStat.h"
#include "Player/Character/TCPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"


#include "Player/Component/GrabComponent.h"
#include "EngineUtils.h"

void ATCFurnitureActor::BeginPlay()
{
    Super::BeginPlay();

    // 파괴됨을 감지 (서버에서만 바인딩)
    if (HasAuthority() && GetFurnitureStat())
    {
        GetFurnitureStat()->OnFurnitureDestroy.AddDynamic(this, &ATCFurnitureActor::DestroyFuniture);
    }
}

void ATCFurnitureActor::DestroyFuniture()
{
    // 서버에서만 처리
    if (HasAuthority())
    {
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

        // 파괴 메쉬가 없다면 액터를 완전히 삭제 예약 (멀티캐스트 전송 시간 확보를 위해 0.1초 지연)
        if (!BrokenMesh)
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
        // 파괴 메쉬가 있으면 교체, 없으면 숨김
        if (BrokenMesh)
        {
            FurnitureMesh->SetStaticMesh(BrokenMesh);
        }
        else
        {
            FurnitureMesh->SetVisibility(false);
        }

        // 물리 시뮬레이션 및 모든 충돌 완전히 끄기 (잔해로만 남음)
        FurnitureMesh->SetSimulatePhysics(false);
        FurnitureMesh->SetCollisionProfileName(TEXT("NoCollision"));
    }
}
bool ATCFurnitureActor::CanInteract_Implementation(ATCPlayerCharacter* Player)
{
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