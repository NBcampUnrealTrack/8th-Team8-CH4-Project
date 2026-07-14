// Fill out your copyright notice in the Description page of Project Settings.


#include "Furniture/TCFurnitureActor.h"
#include "CatchCharacter/Furniture/FurnitureGrabSystem.h"
#include "CatchCharacter/Furniture/FurnitureStat.h"
#include "Player/Character/TCPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Player/Component/GrabComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "EngineUtils.h"
#include "Core/TeamCarryGameMode.h"

ATCFurnitureActor::ATCFurnitureActor()
{
    // 파괴 후 콜리전 꺼짐을 감시하는 용도로만 틱 사용 (평소엔 꺼둠, 파괴 시 활성화)
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    // 물리 시뮬레이션중의 충돌도 OnComponentHit으로 전달.
    if (FurnitureMesh)
    {
        FurnitureMesh->SetNotifyRigidBodyCollision(true);

        // 물리 낙하/던짐 시 무게감: 미끄러짐·구름 억제
        FurnitureMesh->SetLinearDamping(0.8f);
        FurnitureMesh->SetAngularDamping(4.0f);
    }

    // 지오메트리 컬렉션 컴포넌트 생성
    GeometryCollectionComp = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollectionComp"));

    if (RootComponent)
    {
        GeometryCollectionComp->SetupAttachment(RootComponent);
    }

    // 초기에는 보이지 않고, 물리도 꺼진 상태로 설정
    GeometryCollectionComp->SetVisibility(false);
    // 생성자에서 SetSimulatePhysics 금지 — CDO 생성 중 물리 머티리얼 조회 에러로 쿠킹 실패.
    // 프로퍼티 직접 기록으로 대체 (파괴 시 SetSimulatePhysics(true) 런타임 호출은 그대로 유효).
    GeometryCollectionComp->BodyInstance.bSimulatePhysics = false;
    GeometryCollectionComp->SetCollisionProfileName(TEXT("NoCollision"));

    // 클러스터 레벨별 파괴 임계값
    GeometryCollectionComp->bUseSizeSpecificDamageThreshold = false;
    GeometryCollectionComp->DamageThreshold = { 100.0f, 10.0f, 10.0f };

    // 조각의 위치까지 동기화x 어차피 플레이어랑 상호작용안될거.
    GeometryCollectionComp->SetIsReplicated(false);

    // [금 표시 = 겹침 메쉬 방식 (최종 채택)]
    // 원본과 같은 스태틱메쉬를 살짝 키워 겹치고 금 머티리얼만 입힘. 원본 무수정.
    //
    // ── 왜 SetOverlayMaterial(엔진 오버레이)을 안 쓰나 (시도 후 롤백한 이력) ──
    //  1) 오버레이 패스는 'Nanite 메쉬'에서 렌더되지 않음 (실측: Nanite 끄면 나오고 켜면 안 나옴).
    //     가구 메쉬 다수가 Nanite 활성(렌더링 담당 세팅)이라 오버레이는 구조적으로 불가.
    //  2) 오버레이 패스는 Masked 머티리얼도 렌더 안 함 (Translucent만 가능 — 큐브 실측).
    //  겹침 메쉬는 독립 컴포넌트로 일반 렌더 경로를 타므로 원본의 Nanite 여부와 무관하게 항상 그려짐.
    //  금 머티리얼은 Masked/Translucent 둘 다 가능 (현재 Translucent 사용 — 가장자리 부드러움).
    CrackMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrackMeshComp"));
    if (RootComponent)
        CrackMeshComp->SetupAttachment(RootComponent);   // RootComponent = FurnitureMesh (부모 생성자에서 설정)
    CrackMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CrackMeshComp->SetCollisionProfileName(TEXT("NoCollision"));
    CrackMeshComp->SetCastShadow(false);
    CrackMeshComp->SetVisibility(false);                 // 평소엔 숨김, 금 단계에서만 표시
    CrackMeshComp->SetIsReplicated(false);               // 시각 전용 (각 머신에서 로컬 처리)

    // 금(크랙) 오버레이 머티리얼 기본값 자동 로드 → 모든 가구가 별도 지정 없이 공통 사용.
    // (특정 가구만 다른 금을 쓰려면 그 가구 디테일의 Furniture|Crack 슬롯에서 덮어쓰면 됨.
    //  경로에 에셋이 없으면 Succeeded()=false로 그냥 비어 있게 두므로 크래시 없음.)
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> CrackMat1(
        TEXT("/Game/Furniture/Material/M_Damage_level1.M_Damage_level1"));
    if (CrackMat1.Succeeded())
        CrackOverlayStage1 = CrackMat1.Object;

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> CrackMat2(
        TEXT("/Game/Furniture/Material/M_Damage_level2.M_Damage_level2"));
    if (CrackMat2.Succeeded())
        CrackOverlayStage2 = CrackMat2.Object;
}

void ATCFurnitureActor::BeginPlay()
{
    Super::BeginPlay();

    // 파괴됨을 감지 (서버에서만 바인딩)
    if (HasAuthority() && GetFurnitureStat())
    {
        GetFurnitureStat()->OnFurnitureDestroy.AddDynamic(this, &ATCFurnitureActor::DestroyFurniture);
    }

    // (겹침 금 메쉬의 스태틱메쉬 동기화는 UpdateCrackVisual에서 실제 시각 메쉬를 찾아 처리)

    // 금 표시: 체력 변화 감지는 서버·클라 공통으로 바인딩 (OnFurnitureDamage는 서버 TakeDamage와
    // 클라 OnRep_CurrentHealth 양쪽에서 브로드캐스트되므로 전 머신에서 금이 동기화됨).
    if (GetFurnitureStat())
    {
        GetFurnitureStat()->OnFurnitureDamage.AddDynamic(this, &ATCFurnitureActor::OnFurnitureDamaged);

        // 초기 상태 반영 (이미 손상된 가구에 늦게 접속한 클라 대비)
        const float MaxHP = GetFurnitureStat()->GetMaxHealth();
        if (MaxHP > 0.f)
            UpdateCrackVisual(GetFurnitureStat()->GetCurrentHealth() / MaxHP);
    }
}

void ATCFurnitureActor::OnFurnitureDamaged(float MaxHealth, float OldHealth, float NewHealth)
{
    if (MaxHealth <= 0.f)
        return;

    UpdateCrackVisual(NewHealth / MaxHealth);
}

void ATCFurnitureActor::UpdateCrackVisual(float HealthRatio)
{
    // 체력 비율 → 금 단계 (낮은 임계값부터 검사)
    int32 Stage = 0;
    if (HealthRatio <= CrackStage2Ratio)      Stage = 2;
    else if (HealthRatio <= CrackStage1Ratio) Stage = 1;

    if (Stage == CurrentCrackStage)
        return;   // 단계 변화 없으면 재적용 안 함
    CurrentCrackStage = Stage;

    // ==================================================================
    // [시도했다가 롤백: 엔진 오버레이(SetOverlayMaterial) 방식]
    // 한 줄로 끝나 단순하지만, 오버레이 패스가 'Nanite 메쉬'에서 렌더되지 않아 롤백 (실측:
    // Nanite 켜진 가구만 금이 안 나옴. 추가로 Masked 머티리얼도 오버레이 패스에서 렌더 안 됨).
    // 가구 Nanite가 전부 해제되는 날이 오면 아래 두 줄로 교체 가능 (머티리얼은 Translucent 필수).
    // ==================================================================
    //UMaterialInterface* Overlay =
    //    (Stage == 2) ? CrackOverlayStage2 :
    //    (Stage == 1) ? CrackOverlayStage1 : nullptr;
    //FurnitureMesh->SetOverlayMaterial(Overlay);

    // [겹침 메쉬 방식 (최종 채택)] — 이유는 생성자 CrackMeshComp 주석 참조
    if (!CrackMeshComp)
        return;

    // 정상(금 없음) → 숨김
    if (Stage == 0)
    {
        CrackMeshComp->SetVisibility(false);
        return;
    }

    // 실제 '보이는' 스태틱메쉬 컴포넌트 찾기 = 보이면서 바운드가 가장 큰 SMC.
    // (FurnitureMesh가 '안 보이는 콜리전용 메쉬'인 가구가 있어, 그걸 복제하면 아무것도 안 보임 →
    //  콜리전 메쉬가 아닌 '진짜 시각 메쉬'를 크기로 판별해 고른다.)
    UStaticMeshComponent* Src = nullptr;
    {
        float BestSize = -1.f;
        TArray<UStaticMeshComponent*> Comps;
        GetComponents<UStaticMeshComponent>(Comps);
        for (UStaticMeshComponent* C : Comps)
        {
            if (!C || C == CrackMeshComp || !C->GetStaticMesh())
                continue;
            if (!C->IsVisible())          // 숨은 콜리전 메쉬 제외
                continue;
            const float Size = C->Bounds.SphereRadius;
            if (Size > BestSize)
            {
                BestSize = Size;
                Src = C;
            }
        }
        // 폴백: 보이는 게 하나도 없으면 FurnitureMesh라도
        if (!Src && FurnitureMesh && FurnitureMesh->GetStaticMesh())
            Src = FurnitureMesh;
    }

    if (!Src || !Src->GetStaticMesh())
    {
        // 원본 메쉬를 못 찾음 → 이 가구는 금 표시 불가
        return;
    }

    // 겹침 메쉬를 '실제 원본 컴포넌트'에 붙이고 같은 메쉬로 동기화
    if (CrackMeshComp->GetAttachParent() != Src)
        CrackMeshComp->AttachToComponent(Src, FAttachmentTransformRules::SnapToTargetIncludingScale);
    if (CrackMeshComp->GetStaticMesh() != Src->GetStaticMesh())
        CrackMeshComp->SetStaticMesh(Src->GetStaticMesh());

    // z-파이팅 방지: '메쉬 바운드 중심' 기준으로 균일 확대 (피벗이 어디 있든 항상 표면 바깥으로 나감).
    // 컴포넌트 스케일은 피벗 기준이라, 바운드 중심 C가 고정되도록 위치를 C*(1-S)로 보정 → 균일 쉘.
    const float   S           = 1.01f;
    const FVector LocalCenter = Src->GetStaticMesh()->GetBounds().Origin;
    CrackMeshComp->SetRelativeScale3D(FVector(S));
    CrackMeshComp->SetRelativeLocation(LocalCenter * (1.f - S));

    // 단계에 맞는 금 머티리얼을 모든 슬롯에 적용 → 금 선만 원본 위에 뜸
    UMaterialInterface* Mat = (Stage == 2) ? CrackOverlayStage2 : CrackOverlayStage1;
    const int32 NumMats = CrackMeshComp->GetNumMaterials();
    for (int32 i = 0; i < NumMats; ++i)
        CrackMeshComp->SetMaterial(i, Mat);

    CrackMeshComp->SetVisibility(true);
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

        // GM에 가구 파괴를 알림
        if (ATeamCarryGameMode* GM = Cast<ATeamCarryGameMode>(GetWorld()->GetAuthGameMode()))
        {
            GM->OnFurnitureDestroyed();
        }

        // GC 컴포넌트가 없거나, 있어도 파괴 메쉬(RestCollection)가 등록되지 않았다면
        // 조각날 것이 없으므로 액터를 즉시 삭제 예약 (다른작업의 처리 시간 확보를 위해 0.1초 지연)
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

    // 파괴 시 겹침 금 메쉬도 숨김 (원본이 사라지는데 금만 떠 있으면 안 됨)
    if (CrackMeshComp)
    {
        CrackMeshComp->SetVisibility(false);
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
            GetActorLocation(), 100.0f, 200.0f, RIF_Linear, true);
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