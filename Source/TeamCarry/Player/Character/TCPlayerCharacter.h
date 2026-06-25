// TCPlayerCharacter.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "TCPlayerCharacter.generated.h"

// 전방 선언
class UCameraComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;
class UGrabComponent;

UCLASS()
class TEAMCARRY_API ATCPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

#pragma region ACharacter Override

public:
	// 생성자
	ATCPlayerCharacter();

	// 입력 바인딩
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// BeginPlay
	virtual void BeginPlay() override;

#pragma endregion

#pragma region TCPlayerCharacter Components

public:
	// 스프링암 컴포넌트 가져오기
	FORCEINLINE USpringArmComponent* GetSpringArm() const { return SpringArm; }

	// 카메라 컴포넌트 가져오기
	FORCEINLINE UCameraComponent* GetCamera() const { return Camera; }

protected:
	// 카메라를 캐릭터에서 일정 거리 떨어지게 유지하는 스프링암 컴포넌트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	// 플레이어의 화면을 비추는 카메라 컴포넌트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Components")
	TObjectPtr<UCameraComponent> Camera;

	// 상호작용 및 가구 잡기를 담당하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TCPlayerCharacter|Components")
	TObjectPtr<UGrabComponent> GrabComponent;

#pragma endregion

#pragma region Input

private:
	// 이동 처리: W, A, S, D
	void HandleMoveInput(const FInputActionValue& InValue);

	// 시점 처리: 마우스
	void HandleLookInput(const FInputActionValue& InValue);

protected:
	// 입력 컨텍스트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;

	// 이동 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputAction> MoveAction;

	// 시점 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputAction> LookAction;

	// 점프 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputAction> JumpAction;

	// 달리기 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputAction> RunAction;

	// 상호작용(E) - 잡기 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputAction> InteractAction;

	// 상호작용(F) - 던지기 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputAction> ThrowAction;

private:
	// 달리기 시작/종료 처리
	void StartRun(const FInputActionValue& InValue);
	void StopRun(const FInputActionValue& InValue);

	// 상대한테도 보이기 위해 서버로 달리기 상태를 전송하는 RPC 함수
	UFUNCTION(Server, Reliable)
	void ServerStartRun();

	// 상대한테도 보이기 위해 서버로 달리기 상태를 전송하는 RPC 함수
	UFUNCTION(Server, Reliable)
	void ServerStopRun();

	// 상호작용 - 잡기 입력 처리
	void Interact(const FInputActionValue& InValue);

	//  상호작용 - 던지기 입력 처리
	void Throw(const FInputActionValue& InValue);
#pragma endregion

#pragma region Animation

protected:
	// 잡기 애니메이션 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Animation")
	TObjectPtr<UAnimMontage> GrabMontage;

	// 던지기 애니메이션 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Animation")
	TObjectPtr<UAnimMontage> ThrowMontage;

	// 서버에게 애니메이션 재생을 요청하는 RPC 함수
	UFUNCTION(Server, Reliable)
	void ServerPlayActionMontage(int32 ActionID);

	// 서버가 클라이언트에게 애니메이션을 재생하라고 방송하는 RPC 함수
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayActionMontage(int32 ActionID);


#pragma endregion
};
