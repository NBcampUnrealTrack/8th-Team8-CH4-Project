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
class UTCCarrySpeedComponent;
class UWidgetInteractionComponent;

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

	// 월드 스페이스 위젯 상호작용 컴포넌트 가져오기(ATCPlayerController 등 외부에서 접근).
	FORCEINLINE UWidgetInteractionComponent* GetWidgetInteraction() const { return WidgetInteraction; }

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

	// 가구 운반 중 인원비례 속도 조절 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TCPlayerCharacter|Components")
	TObjectPtr<UTCCarrySpeedComponent> CarrySpeedComponent;

	// BP_StageSelectBoard 등 월드 스페이스 위젯(WidgetComponent)과의 마우스 클릭 상호작용을 담당한다
	// (명세 4장-5, 게시판 UI 개정). 마우스 커서 위치를 기준으로 트레이스하므로, 로컬 플레이어가
	// 커서를 사용할 수 있는 모드(ATCPlayerController::EnterBoardInteractionMode)일 때만 의미 있다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TCPlayerCharacter|Components")
	TObjectPtr<UWidgetInteractionComponent> WidgetInteraction;

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

	// 상하 시점 액션 (Aim Offset)
	UFUNCTION(BlueprintPure, Category = "TCPlayerCharacter|Animation")
	float GetAimPitch() const;

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

	// 시점 전환 액션 (Tab키)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputAction> ToggleViewAction;

	// 가구 회전(Z축) 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputAction> RotateZAction;

	// 가구 회전(Y축) 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputAction> RotateYAction;

	// 카메라 줌 액션 (마우스 휠)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputAction> ZoomAction;

	// 이모트(춤) 1~4 입력 처리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputAction> Emote1Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputAction> Emote2Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputAction> Emote3Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Input")
	TObjectPtr<UInputAction> Emote4Action;

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

	// 상호작용 입력(좌클릭) 뗄 때 처리 — 게시판 클릭 모드 중엔 WidgetInteraction 릴리즈로 전달
	void ReleaseInteract(const FInputActionValue& InValue);

	//  상호작용 - 던지기 입력 처리
	void Throw(const FInputActionValue& InValue);

	// 현재 1인칭 상태인지 확인하는 변수
	bool bIsFirstPerson = false;

	// 시점 전환 입력 처리
	void ToggleView(const FInputActionValue& InValue);

	// 점프 입력 처리
	void TryJump();

	// 가구 회전 입력 처리
	void RotateZ(const FInputActionValue& InValue);
	void RotateY(const FInputActionValue& InValue);

	// 마우스 휠 줌 처리
	void HandleZoomInput(const FInputActionValue& InValue);

	// 이모트(춤) 1~4 입력 처리 함수
	void Emote1(const FInputActionValue& InValue);
	void Emote2(const FInputActionValue& InValue);
	void Emote3(const FInputActionValue& InValue);
	void Emote4(const FInputActionValue& InValue);

	// 이모트(춤) 취소 입력 처리 함수
	void CancelEmote(const FInputActionValue& InValue);

#pragma endregion

#pragma region Animation

protected:
	// 잡기 애니메이션 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Animation")
	TObjectPtr<UAnimMontage> GrabMontage;

	// 던지기 애니메이션 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Animation")
	TObjectPtr<UAnimMontage> ThrowMontage;

	// 이모트 애니메니션 몽타주(1~4)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Animation")
	TObjectPtr<UAnimMontage> Emote1Montage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Animation")
	TObjectPtr<UAnimMontage> Emote2Montage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Animation")
	TObjectPtr<UAnimMontage> Emote3Montage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TCPlayerCharacter|Animation")
	TObjectPtr<UAnimMontage> Emote4Montage;

	// 서버에게 애니메이션 재생을 요청하는 RPC 함수
	UFUNCTION(Server, Reliable)
	void ServerPlayActionMontage(int32 ActionID);

	// 서버가 클라이언트에게 애니메이션을 재생하라고 방송하는 RPC 함수
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayActionMontage(int32 ActionID);

	// 서버에게 애니메이션 중지를 요청하는 RPC 함수
	UFUNCTION(Server, Reliable)
	void ServerStopActionMontage(int32 ActionID);

	// 서버가 클라이언트에게 애니메이션 중지를 방송하는 RPC 함수
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopActionMontage(int32 ActionID);

#pragma endregion
};
