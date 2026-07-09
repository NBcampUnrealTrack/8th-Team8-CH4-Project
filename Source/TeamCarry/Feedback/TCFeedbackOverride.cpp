// Fill out your copyright notice in the Description page of Project Settings.

#include "Feedback/TCFeedbackOverride.h"
#include "GameFramework/Actor.h"

FVector ITCFeedbackOverride::GetFeedbackLocation_Implementation()
{
	if (const AActor* AsActor = Cast<AActor>(this))
	{
		return AsActor->GetActorLocation();
	}
	return FVector::ZeroVector;
}
