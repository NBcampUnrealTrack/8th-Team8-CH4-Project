// Fill out your copyright notice in the Description page of Project Settings.


#include "Level/Struct/DamageableWall.h"
#include "Components/StaticMeshComponent.h"

ADamageableWall::ADamageableWall() { MaxDamageLevel = 3; }

void ADamageableWall::OnDamageChanged(int32 NewLevel)
{
	if (NewLevel <= 0 || DamageMaterials.Num() == 0) return;

	const int32 Index = FMath::Clamp(NewLevel - 1, 0, DamageMaterials.Num() - 1);
	if (DamageMaterials[Index])
	{
		Mesh->SetMaterial(0, DamageMaterials[Index]);
	}
}
