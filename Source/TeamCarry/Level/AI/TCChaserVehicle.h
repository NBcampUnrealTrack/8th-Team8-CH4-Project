#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TCChaserVehicle.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

UCLASS()
class TEAMCARRY_API ATCChaserVehicle : public APawn
{
	GENERATED_BODY()

public:
	ATCChaserVehicle();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// AI(컨트롤러/BT)가 호출할 조종 입력 — 매 틱 목표 방향 세팅
	// Throttle: -1~1 (전후진), Steer: -1~1 (조향)
	void SetDriveInput(float InThrottle, float InSteer);

	// 무력화 (앞유리 깨지면 호출 — 다음 단계에서 연결)
	UFUNCTION(BlueprintCallable, Category = "Chaser")
	void Disable();

	UFUNCTION(BlueprintPure, Category = "Chaser")
	bool IsDisabled() const { return bDisabled; }

	UFUNCTION(BlueprintPure, Category = "Chaser")
	float GetCurrentSpeed() const { return CurrentSpeed; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Chaser")
	TObjectPtr<UStaticMeshComponent> Body;

	// 플레이어 트럭과 충돌 감지용
	UPROPERTY(VisibleAnywhere, Category = "Chaser")
	TObjectPtr<UBoxComponent> HitBox;

	// 이동 파라미터
	UPROPERTY(EditAnywhere, Category = "Chaser|Movement")
	float MaxSpeed = 1400.f;        // 플레이어 트럭보다 약간 빠르게(추격)
	UPROPERTY(EditAnywhere, Category = "Chaser|Movement")
	float Acceleration = 1800.f;
	UPROPERTY(EditAnywhere, Category = "Chaser|Movement")
	float TurnRateAtSpeed = 70.f;
	UPROPERTY(EditAnywhere, Category = "Chaser|Movement")
	float BrakeDecel = 2000.f;

	// 무력화 시 꺾이는 방향/세기
	UPROPERTY(EditAnywhere, Category = "Chaser|Disable")
	float SwerveYawRate = 90.f;     // 무력화 후 초당 회전(도)

	UPROPERTY(ReplicatedUsing = OnRep_Disabled)
	bool bDisabled = false;

	UFUNCTION() void OnRep_Disabled();

private:
	float CurrentThrottle = 0.f;
	float CurrentSteer = 0.f;
	float CurrentSpeed = 0.f;
	float SwerveDir = 0.f;          // 무력화 시 좌(-1)/우(+1)
};