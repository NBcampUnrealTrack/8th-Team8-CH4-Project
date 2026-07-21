// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/ToonSkySyncActor.h"
#include "Engine/DirectionalLight.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/GameplayStatics.h"

ATCToonSkySync::ATCToonSkySync()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f; // 20Hz면 편집·연출용으로 충분
}

void ATCToonSkySync::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Sun)
	{
		Sun = Cast<ADirectionalLight>(
			UGameplayStatics::GetActorOfClass(GetWorld(), ADirectionalLight::StaticClass()));
	}
	if (Sun && SkyCollection)
	{
		// 태양을 '향하는' 방향 = 라이트 전방의 반대
		const FVector Dir = -Sun->GetActorForwardVector();
		UKismetMaterialLibrary::SetVectorParameterValue(
			this, SkyCollection, TEXT("SunDir"), FLinearColor(Dir.X, Dir.Y, Dir.Z, 0.f));
	}
}
