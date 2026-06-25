// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/Struct/BreakableWindow.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

ABreakableWindow::ABreakableWindow() { MaxDamageLevel = 1; }

void ABreakableWindow::OnDamageChanged(int32 NewLevel)
{
	if (NewLevel < 1) return;

	if (BrokenMesh) { Mesh->SetStaticMesh(BrokenMesh); }
	else { Mesh->SetVisibility(false); }

	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	if (BreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BreakSound, GetActorLocation());
	}
}

