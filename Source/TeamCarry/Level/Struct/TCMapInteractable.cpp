// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/Struct/TCMapInteractable.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TC_MapUIHandler.h"
#include "Player/Character/TCPlayerCharacter.h"
#include "GameFramework/PlayerController.h"

ATCMapInteractable::ATCMapInteractable()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAll")); 
}

bool ATCMapInteractable::CanInteract_Implementation(ATCPlayerCharacter*)
{
	return true;
}

void ATCMapInteractable::OnFocus_Implementation() { Mesh->SetRenderCustomDepth(true); }
void ATCMapInteractable::OnUnfocus_Implementation() { Mesh->SetRenderCustomDepth(false); }

void ATCMapInteractable::OnInteract_Implementation(ATCPlayerCharacter* Player)
{

	if (!Player) return;

	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC) return;

	// 임시 확인 로그 (UI 연결 전 통보 발동 검증용)
	UE_LOG(LogTemp, Warning, TEXT("[MapInteract] OpenStageMap 통보 → %s"), *GetNameSafe(PC));

	if (PC->Implements<UTC_MapUIHandler>())
	{
		ITC_MapUIHandler::Execute_OpenStageMap(PC);
	}
}
