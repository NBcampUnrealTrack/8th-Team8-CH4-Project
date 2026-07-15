// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CatchCharacter/Furniture/FurnitureActor.h"
#include "Player/Interface/TCInteractable.h"
#include "TCFurnitureActor.generated.h"

class UMaterialInterface;
class UUserWidget;

/**
 * 
 */
UCLASS()
class TEAMCARRY_API ATCFurnitureActor : public AFurnitureActor, public ITCInteractable
{
    GENERATED_BODY()

public:
    ATCFurnitureActor();

    virtual bool CanInteract_Implementation(ATCPlayerCharacter* Player) override;
    virtual void OnInteract_Implementation(ATCPlayerCharacter* Player) override;
    virtual void OnFocus_Implementation() override;
    virtual void OnUnfocus_Implementation() override;

protected:
    virtual void BeginPlay() override;

    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION()
    void DestroyFurniture();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_DestroyFurniture();

    // 가구 파괴용 메쉬
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Destruction")
    TObjectPtr<UGeometryCollectionComponent> GeometryCollectionComp;

    // 파괴 사운드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
    TObjectPtr<USoundBase> BreakSound;

    //// 가구 전용 피지컬 머티리얼 (튕김 제거 + 미끄러짐 방지, 코드에서 생성)
    //UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physics")
    //TObjectPtr<class UPhysicalMaterial> FurniturePhysMaterial;

    // 가구가 파괴되었는지 여부를 저장하는 플래그
    //UPROPERTY(Replicated) 서버에서만 처리하면되니 필요없을거라 판단.
    bool bIsFurnitureDestroyed = false;

    // =====================================================================
    // 금(크랙) 표시 — 체력 비율이 임계값 이하로 떨어지면 메쉬 위에 오버레이 머티리얼로 '금'을 덧씌움.
    // 원본 머티리얼은 무수정. 반투명 '금' 머티리얼을 에디터에서 슬롯에 지정(비워두면 안 뜸, 안전).
    // 체력 감지는 UFurnitureStat::OnFurnitureDamage(서버·클라 공통 브로드캐스트)로 처리 → 전 클라 동기화.
    // =====================================================================

    // 금 전용 겹침 메쉬 — 원본과 같은 스태틱메쉬를 살짝 키워 겹치고, 이 메쉬에만 금 머티리얼을 입힘.
    // 엔진 SetOverlayMaterial을 못 쓰는 이유(실측): 오버레이 패스가 Nanite 메쉬에서 렌더되지 않음
    // (가구 다수가 Nanite 활성) + Masked 머티리얼도 오버레이 패스 미지원. 겹침 메쉬는 독립 컴포넌트라
    // 일반 렌더 경로로 항상 그려짐. 상세 이력은 cpp 생성자·UpdateCrackVisual 주석 참조.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture|Crack")
    TObjectPtr<UStaticMeshComponent> CrackMeshComp;

    // 1단계 금 (CrackStage1Ratio 이하). 예: 60%
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Crack")
    TObjectPtr<UMaterialInterface> CrackOverlayStage1;

    // 2단계 금 (CrackStage2Ratio 이하). 예: 30%
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Crack")
    TObjectPtr<UMaterialInterface> CrackOverlayStage2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Crack", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CrackStage1Ratio = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Crack", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CrackStage2Ratio = 0.3f;

    // =====================================================================
    // 데미지 숫자 UI — 피해를 입을 때마다 가구 위에 피해량 위젯을 띄움 (전 클라, 각자 로컬 표시).
    // 체력 감지는 금 표시와 같은 OnFurnitureDamage(서버 TakeDamage + 클라 OnRep) 경로 재사용.
    // 위젯 BP 계약: 'SetDamage(float)' 함수를 구현하면 피해량이 전달됨 (없어도 크래시 없음).
    // =====================================================================

    // 피해량 숫자 표시 여부. false면 위젯을 아예 스폰하지 않음 (가구별 에디터 설정·런타임 BP 토글 모두 가능)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|DamageUI")
    bool bShowDamageNumbers = false;

    // 피해량 표시 위젯 클래스. 비워두면 표시 안 함 (C++ 기본 경로 자동 로드 시도).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|DamageUI")
    TSubclassOf<UUserWidget> DamageNumberWidgetClass;

    // 위젯 자동 제거까지의 시간(초). 위젯 애니메이션 길이와 맞출 것.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|DamageUI")
    float DamageNumberLifetime = 1.2f;

    // 피해량 위젯 스폰 (스크린 스페이스 위젯 컴포넌트, Lifetime 후 자동 제거)
    void SpawnDamageNumber(float Damage);

    // 체력 변화 콜백 (서버·클라 공통). 금 단계 갱신 + 데미지 숫자 표시.
    UFUNCTION()
    void OnFurnitureDamaged(float MaxHealth, float OldHealth, float NewHealth);

    // 체력 비율(0~1)로 금 단계를 계산해 오버레이 적용. 단계가 바뀔 때만 실제 교체.
    void UpdateCrackVisual(float HealthRatio);

    // 현재 적용된 금 단계 (0=없음, 1, 2) — 중복 적용 방지
    int32 CurrentCrackStage = 0;

    // =====================================================================
    // 무게 기반 중력 — 무거운 가구일수록 빨리 떨어짐.
    // 기본 물리는 질량과 무관하게 같은 가속도로 낙하하므로, 스탯 무게(Mass)에 비례한
    // 추가 중력 가속을 Tick에서 더해준다. GravityMassReference(100)가 1배 기준.
    //   예) Mass 100 = 1배(추가 없음), Mass 200 = 2배, Mass 50 = 0.5배(더 천천히)
    // =====================================================================

    // 중력 1배 기준 무게. 이 값 대비 비율만큼 중력 가속이 스케일됨. 0 이하면 기능 끔.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture|Physics")
    float GravityMassReference = 100.f;

};
