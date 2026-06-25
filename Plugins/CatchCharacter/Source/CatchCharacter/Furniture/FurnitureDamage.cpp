// Fill out your copyright notice in the Description page of Project Settings.

#include "FurnitureDamage.h"

UFurnitureDamage::UFurnitureDamage()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFurnitureDamage::BeginPlay()
{
	Super::BeginPlay();
}

void UFurnitureDamage::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
