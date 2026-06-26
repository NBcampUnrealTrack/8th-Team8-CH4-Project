// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/Interface/TCInteractable.h"
#include "Core/TC_UsableItem.h"
#include "TCBatItem.generated.h"

UCLASS()
class TEAMCARRY_API ATCBatItem : public AActor, public ITCInteractable, public ITC_UsableItem
{
	GENERATED_BODY()
	
public:
	ATCBatItem();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ITCInteractable
	virtual bool CanInteract_Implementation(ATCPlayerCharacter* Player) override;
	virtual void OnFocus_Implementation() override;
	virtual void OnUnfocus_Implementation() override;
	virtual void OnInteract_Implementation(ATCPlayerCharacter* Player) override;

	// 휘두르기 (GrabComponent나 플레이어가 호출 — 서버 전제)
	UFUNCTION(BlueprintCallable, Category = "Bat")
	void Swing(AActor* User);

	virtual void OnUse_Implementation(AActor* User) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Bat")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 휘두를 때 닿는 범위
	UPROPERTY(EditAnywhere, Category = "Bat")
	float SwingRange = 150.f;
	UPROPERTY(EditAnywhere, Category = "Bat")
	float SwingRadius = 80.f;

	// 기절 시간
	UPROPERTY(EditAnywhere, Category = "Bat")
	float StunDuration = 3.f;

	// 들고 있는 주인 (복제 — 어태치 상태 공유)
	UPROPERTY(ReplicatedUsing = OnRep_Holder)
	TObjectPtr<AActor> Holder = nullptr;

	UFUNCTION()
	void OnRep_Holder();

private:
	void AttachToHolder(AActor* NewHolder);
};
