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
    // 파괴 후 콜리전 꺼짐을 감시하는 용도로만 틱 사용 (평소엔 꺼둠, 파괴 시 활성화)
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

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

    // 조각의 위치까지 동기화x 어차피 플레이어랑 상호작용안될거.
    GeometryCollectionComp->SetIsReplicated(false);
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

        // AllRelease() → Release() 내부에서 SetSimulatePhysics(true)와 SetReplicateMovement(true)가 복원됨.
        // FurnitureMesh가 RootComponent이므로 물리가 활성화되면 액터 전체가 중력으로 낙하하고
        // 거기에 붙은 GC 컴포넌트도 통째로 따라 이동해 흔들려 보임 → 즉시 다시 꺼서 차단.
        if (FurnitureMesh)
        {
            FurnitureMesh->SetSimulatePhysics(false);
        }
        SetReplicateMovement(false);

        // 파괴 효과
        Multicast_DestroyFurniture();

        // GC 컴포넌트가 없거나, 있어도 파괴 메쉬(RestCollection)가 등록되지 않았다면
        // 조각날 것이 없으므로 액터를 즉시 삭제 예약 (다른작업의 처리 시간 확보를 위해 0.1초 지연)
        if (!GeometryCollectionComp || !GeometryCollectionComp->GetRestCollection())
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
    // 클라이언트에서도 파괴 상태를 기록하고 감시 틱 시작.
    // OnRep_GrabbedPlayers가 이 RPC보다 늦게 도착하면 FurnitureMesh의
    // 물리/콜리전을 되살려 투명 벽이 생기므로, Tick에서 즉시 다시 꺼서 방어.
    bIsFurnitureDestroyed = true;
    SetActorTickEnabled(true);

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

    // 파괴 메쉬(RestCollection)가 실제로 등록된 경우에만 조각내기 시뮬레이션 실행.
    // 컴포넌트만 있고 메쉬가 비어 있으면 조각날 것이 없으므로 건너뜀.
    if (GeometryCollectionComp && GeometryCollectionComp->GetRestCollection())
    {
        GeometryCollectionComp->SetVisibility(true);

        // 바닥·벽과 충돌하는 기본 프로파일을 먼저 적용한 뒤 Pawn 채널만 무시
        GeometryCollectionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
        GeometryCollectionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

        // 루트(StaticMesh)에서 분리: 붙어 있으면 어태치먼트 구속(부모 위치 유지)과
        // 물리 시뮬레이션(자유 낙하)이 매 프레임 상충해 조각 전체가 흔들림 → 분리해 독립 시뮬레이션.
        GeometryCollectionComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

        // TODO : GC 에셋에서 Enable Clustering=false 또는 Damage Threshold≈0 설정 필요
        GeometryCollectionComp->SetSimulatePhysics(true);

        GeometryCollectionComp->AddRadialImpulse(
            GetActorLocation(), 40.0f, 80.0f, RIF_Linear, true);
    }
}

void ATCFurnitureActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 파괴 후 OnRep_GrabbedPlayers가 뒤늦게 도착해 콜리전을 되살리는 경우 대비:
    // 살아나는 즉시 다음 틱에 다시 꺼서 파괴된 가구 자리의 투명 벽 방지
    if (bIsFurnitureDestroyed && FurnitureMesh &&
        FurnitureMesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
    {
        FurnitureMesh->SetSimulatePhysics(false);
        FurnitureMesh->SetCollisionProfileName(TEXT("NoCollision"));
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