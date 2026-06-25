// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TCGoalZone.generated.h"

class UBoxComponent;
class AStageManager;

UCLASS()
class TEAMCARRY_API ATCGoalZone : public AActor
{
	GENERATED_BODY()
	
public:
	ATCGoalZone();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Goal")
	TObjectPtr<UBoxComponent> Trigger;

	// 도착(놓임) 확인 주기
	UPROPERTY(EditAnywhere, Category = "Goal")
	float CheckInterval = 0.25f;

	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	void CheckArrivals(); // 타이머 콜백 놓인 가구 카운트

	UPROPERTY() 
	TObjectPtr<AStageManager> StageManager;
	
	UPROPERTY() 
	TSet<TObjectPtr<AActor>> InZone;     // 현재 구역 안 가구
	
	UPROPERTY() 
	TSet<TObjectPtr<AActor>> Delivered;  // 이미 카운트됨
	
	FTimerHandle CheckTimer;

public:
	void OnFurnitureStolen(AActor* Furniture);

	UFUNCTION(BlueprintCallable, Category = "Goal")
	AActor* GetAnyDeliveredFurniture() const;
};