// TCPlayerCharacter.cpp


#include "Player/Character/TCPlayerCharacter.h"

#include "Player/Component/GrabComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"

// 기본 컴포넌트 + 이동 속성 초기화
ATCPlayerCharacter::ATCPlayerCharacter()
{
	// 틱 호출 활성화 여부
	PrimaryActorTick.bCanEverTick = false;

	// 컨트롤러 회전에 캐릭터가 따라 돌지 않도록 설정
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 캐릭터 이동 방향을 향해 자동으로 회전하도록 설정
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);

	// 스프링 암 컴포넌트 생성 및 초기 설정
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->TargetArmLength = 400.f;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->SetupAttachment(GetRootComponent());

	// 카메라 컴포넌트 생성 및 스프링 암에 부착
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->bUsePawnControlRotation = false;
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	// 기본 걷기 속도 = 250으로 설정
	GetCharacterMovement()->MaxWalkSpeed = 250.f;

	// 가구 컴포넌트 연결
	GrabComponent = CreateDefaultSubobject<UGrabComponent>(TEXT("GrabComponent"));

}

// 플레이어 키보드와 마우스 입력을 캐릭터 동작 함수에 연결
void ATCPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 향상된 입력 컴포넌트로 캐스팅
	UEnhancedInputComponent* EIC = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	// 각 입력 액션 바인딩
	EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::HandleMoveInput);
	EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::HandleLookInput);
	EIC->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
	EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	EIC->BindAction(RunAction, ETriggerEvent::Started, this, &ThisClass::StartRun);
	EIC->BindAction(RunAction, ETriggerEvent::Completed, this, &ThisClass::StopRun);
	EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::Interact);
	EIC->BindAction(ThrowAction, ETriggerEvent::Started, this, &ThisClass::Throw);

}

// 게임 시작 시 수행
void ATCPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	// 로컬 플레이어가 조종하는 캐릭터인지 확인
	if (IsLocallyControlled() == true)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		checkf(IsValid(PC) == true, TEXT("PlayerController is invalid."));

		// 향상된 입력 시스템 가져오기
		UEnhancedInputLocalPlayerSubsystem* EILPS = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
		checkf(IsValid(EILPS) == true, TEXT("EnhancedInputLocalPlayerSubsystem is invalid."));

		// 입력 매핑 컨텍스트 추가
		EILPS->AddMappingContext(InputMappingContext, 0);
	}
}

// 플레이어 이동 입력 처리
void ATCPlayerCharacter::HandleMoveInput(const FInputActionValue& InValue)
{
	// 컨트롤러 유효성 검사
	if (IsValid(Controller) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("Controller is invalid."));
		return;
	}

	// 이동 입력 값 가져오기
	const FVector2D InMovementVector = InValue.Get<FVector2D>();

	// 컨트롤러의 시선 방향 계산
	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator ControlYawRotation(0.0f, ControlRotation.Yaw, 0.0f);

	// 전방 및 우측 방향 벡터 추출
	const FVector ForwardDirection = FRotationMatrix(ControlYawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(ControlYawRotation).GetUnitAxis(EAxis::Y);

	// 캐릭터 이동 적용
	AddMovementInput(ForwardDirection, InMovementVector.X);
	AddMovementInput(RightDirection, InMovementVector.Y);
}

// 플레이어 카메라 시점 회전(마우스) 처리
void ATCPlayerCharacter::HandleLookInput(const FInputActionValue& InValue)
{
	// 컨트롤러 유효성 검사
	if (IsValid(Controller) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("Controller is invalid."));
		return;
	}

	// 마우스 이동 값 가져오기
	const FVector2D InLookVector = InValue.Get<FVector2D>();

	// 카메라 회전 적용
	AddControllerYawInput(InLookVector.X);
	AddControllerPitchInput(InLookVector.Y);
}

// 플레이어 달리기 시작
void ATCPlayerCharacter::StartRun(const FInputActionValue& InValue)
{
	// 달리기 최대 속도 500
	GetCharacterMovement()->MaxWalkSpeed = 500.f;

	// 서버한테 알리기
	ServerStartRun();

}

// 플레이어 달리기 종료 -> 걷기
void ATCPlayerCharacter::StopRun(const FInputActionValue& InValue)
{
	// 걷기 속도 250
	GetCharacterMovement()->MaxWalkSpeed = 250.f;

	// 서버한테 알리기
	ServerStopRun();

}

// 상호작용(E키) - 잡기
void ATCPlayerCharacter::Interact(const FInputActionValue& InValue)
{
	// 유효성 검사
	if (GrabComponent)
	{
		// 뷰포트에 로그 출력
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("E키 입력 : 가구 잡기 시도"));
		}

		// 출력 로그
		UE_LOG(LogTemp, Warning, TEXT("E키 입력 : 가구 잡기 시도"));

		// 애니메이션 재생
		if (GrabMontage)
		{
			// 로컬 애니메이션 재생
			PlayAnimMontage(GrabMontage);
			
			// 서버에 애니메이션 전송
			ServerPlayActionMontage(0);
		}

		// 상호작용-잡기 실행 명령
		GrabComponent->TryInteract();
	}
}

// 상호작용(F키) - 던지기
void ATCPlayerCharacter::Throw(const FInputActionValue& InValue)
{
	// 유효성 검사
	if (GrabComponent)
	{
		// 뷰포트에 로그 출력
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("F키 입력 : 가구 던지기 시도"));
		}

		// 출력 로그
		UE_LOG(LogTemp, Warning, TEXT("F키 입력 : 가구 던지기 시도"));

		// 애니메이션 재생
		if (ThrowMontage)
		{
			// 로컬 애니메이션 재생
			PlayAnimMontage(ThrowMontage);

			// 서버에 애니메이션 전송
			ServerPlayActionMontage(1);
		}

		// 상호작용-던지기 실행 명령
		GrabComponent->TryThrow();
	}	
}

// 애니메이션 전체 클라이언트 동기화
void ATCPlayerCharacter::MulticastPlayActionMontage_Implementation(int32 ActionID)
{
	// 중복 재생 방지
	// 내가 조종 중인 캐릭터가 아닐 때(남의 화면에서 볼 때)만 애니메이션을 덮어씌워 재생
	if (!IsLocallyControlled())
	{
		// 전달받은 ID에 따라 각자의 PC에 세팅된 몽타주를 안전하게 재생
		if (ActionID == 0 && GrabMontage)
		{
			PlayAnimMontage(GrabMontage);
		}
		else if (ActionID == 1 && ThrowMontage)
		{
			PlayAnimMontage(ThrowMontage);
		}
	}
}

// Server - 애니메이션 재생 요청 수신 및 전파
void ATCPlayerCharacter::ServerPlayActionMontage_Implementation(int32 ActionID)
{
	// 서버가 요청 받는 즉시 모든 사람(Multicast)에게 전파
	MulticastPlayActionMontage(ActionID);
}

// Server - 달리기 종료
void ATCPlayerCharacter::ServerStopRun_Implementation()
{
	GetCharacterMovement()->MaxWalkSpeed = 250.f;
}

// Server - 달리기 시작
void ATCPlayerCharacter::ServerStartRun_Implementation()
{
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
}


