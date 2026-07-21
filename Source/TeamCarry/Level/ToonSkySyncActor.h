// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ToonSkySyncActor.generated.h"

class ADirectionalLight;
class UMaterialParameterCollection;

/**
 * 툰 스카이 태양 동기화 액터.
 * DirectionalLight 방향을 매 틱 MPC_ToonSky의 SunDir에 써서,
 * 라이트를 돌리면 툰 스카이의 태양 글로우·노을이 즉시 따라오게 한다.
 * 에디터 뷰포트에서도 틱하므로 (ShouldTickIfViewportsOnly) 편집 중에도 실시간 반영.
 */
UCLASS()
class TEAMCARRY_API ATCToonSkySync : public AActor
{
	GENERATED_BODY()

public:
	ATCToonSkySync();

	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; } // PIE 밖 에디터 뷰포트에서도 틱

protected:
	// 따라갈 태양. 미지정이면 레벨의 첫 DirectionalLight를 자동 탐색
	UPROPERTY(EditAnywhere, Category = "ToonSky")
	TObjectPtr<ADirectionalLight> Sun;

	// SunDir 벡터를 기록할 머티리얼 파라미터 컬렉션 (MPC_ToonSky)
	UPROPERTY(EditAnywhere, Category = "ToonSky")
	TObjectPtr<UMaterialParameterCollection> SkyCollection;
};
