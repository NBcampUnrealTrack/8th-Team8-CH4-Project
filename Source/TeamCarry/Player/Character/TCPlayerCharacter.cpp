// TCPlayerCharacter.cpp

#include "Player/Character/TCPlayerCharacter.h"
#include "Player/Component/GrabComponent.h"
#include "Player/Component/TCCarrySpeedComponent.h"
#include "Furniture/TCFurnitureActor.h"
#include "CatchCharacter/Furniture/FurnitureGrabSystem.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"
#include "Components/WidgetInteractionComponent.h"
#include "Player/PlayerController/TCPlayerController.h"

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
	
	// 카메라가 벽에 부딪히도록 충돌 활설화
	SpringArm->bDoCollisionTest = true;
	
	// 카메라가 몸통을 뚫지 않도록 스프링 암의 기준점을 펭귄 머리 높이로 올림
	SpringArm->TargetOffset = FVector(0.f, 0.f, 80.f); 

	// 카메라 충돌 구체의 크기를 키워 벽에 너무 깊이 파고들기 전에 미리 멈추게 방어
	SpringArm->ProbeSize = 24.f;

	// 카메라가 부드럽게 지연되며 따라오게 수정
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 15.0f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 10.0f;
	
	SpringArm->SetupAttachment(GetRootComponent());

	// 카메라 컴포넌트 생성 및 스프링 암에 부착
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->bUsePawnControlRotation = false;
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	// 기본 걷기 속도 = 250으로 설정
	GetCharacterMovement()->MaxWalkSpeed = 250.f;

	// 가구 컴포넌트 연결
	GrabComponent = CreateDefaultSubobject<UGrabComponent>(TEXT("GrabComponent"));

	// 운반 인원비례 속도 조절 컴포넌트
	CarrySpeedComponent = CreateDefaultSubobject<UTCCarrySpeedComponent>(TEXT("CarrySpeedComponent"));

	// 월드 스페이스 위젯(BP_StageSelectBoard 의 BoardScreen 등) 클릭 상호작용용(명세 4장-5, 게시판 UI 개정).
	// 마우스 커서 위치를 기준으로 트레이스한다 — ATCPlayerController::EnterBoardInteractionMode() 가
	// 커서를 노출하는 입력 모드로 전환해 줄 때만 실질적으로 클릭이 가능하다.
	WidgetInteraction = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteraction"));
	WidgetInteraction->SetupAttachment(Camera);
	WidgetInteraction->InteractionSource = EWidgetInteractionSource::Mouse;
	// 게시판 클릭 모드가 아닐 때는 비활성 — 평상시 마우스 커서가 없는 상태에서 엉뚱한 월드 위젯을
	// 건드리지 않도록 한다. ATCPlayerController::ClientEnterBoardInteractionMode()가 활성화한다.
	WidgetInteraction->SetActive(false);
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
	EIC->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ThisClass::TryJump);
	EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	EIC->BindAction(RunAction, ETriggerEvent::Started, this, &ThisClass::StartRun);
	EIC->BindAction(RunAction, ETriggerEvent::Completed, this, &ThisClass::StopRun);
	EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::Interact);
	EIC->BindAction(InteractAction, ETriggerEvent::Completed, this, &ThisClass::ReleaseInteract);
	EIC->BindAction(ThrowAction, ETriggerEvent::Started, this, &ThisClass::Throw);
	EIC->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &ThisClass::ToggleView);
	EIC->BindAction(RotateZAction, ETriggerEvent::Triggered, this, &ThisClass::RotateZ);
	EIC->BindAction(RotateYAction, ETriggerEvent::Triggered, this, &ThisClass::RotateY);
	EIC->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ThisClass::HandleZoomInput);
	EIC->BindAction(Emote1Action, ETriggerEvent::Started, this, &ThisClass::Emote1);
	EIC->BindAction(Emote2Action, ETriggerEvent::Started, this, &ThisClass::Emote2);
	EIC->BindAction(Emote3Action, ETriggerEvent::Started, this, &ThisClass::Emote3);
	EIC->BindAction(Emote4Action, ETriggerEvent::Started, this, &ThisClass::Emote4);
	EIC->BindAction(MoveAction, ETriggerEvent::Started, this, &ThisClass::CancelEmote);
	EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ThisClass::CancelEmote);
	EIC->BindAction(RunAction, ETriggerEvent::Started, this, &ThisClass::CancelEmote);

}

// 게임 시작 시 수행
void ATCPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 마우스 조작감 — 카메라 회전 랙을 줄여 시선이 '확확' 따라오게 한다.
	// (랙 10.0은 카메라가 마우스를 눈에 띄게 뒤따라와 조작이 둔하게 느껴지던 원인)
	// 생성자 기본값 대신 여기서 덮어쓰는 것은 Live Coding 호환용 런타임 튜닝 지점 —
	// 값 확정 후 생성자 기본값으로 옮겨도 된다.
	if (SpringArm)
	{
		SpringArm->CameraRotationLagSpeed = 25.f;
		SpringArm->CameraLagSpeed = 20.f;
	}

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

	// 이모트(춤)이 재생 중일 때는 이동 입력을 무시하여 미끄러짐 방지
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && (
		AnimInstance->Montage_IsPlaying(Emote1Montage) ||
		AnimInstance->Montage_IsPlaying(Emote2Montage) ||
		AnimInstance->Montage_IsPlaying(Emote3Montage) ||
		AnimInstance->Montage_IsPlaying(Emote4Montage)))
	{
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

// 상하 시점 (Aim Offset) 각도 반환
float ATCPlayerCharacter::GetAimPitch() const
{
	// Aim Offset 설정에 맞게 180도 형식으로 정규화(Normalize)해서 반환
	return FRotator::NormalizeAxis(GetBaseAimRotation().Pitch);
}

// 플레이어 달리기 시작
void ATCPlayerCharacter::StartRun(const FInputActionValue& InValue)
{
	// 운반 중에는 인원수와 무관하게 달리기가 MaxWalkSpeed를 건드리지 않는다.
	// 운반 속도는 GrabSystem(ComputeCarrySpeed)이 서버·클라 양쪽에서 관리하는데,
	// 여기서 500으로 덮어쓰면 서버 운반 속도와 어긋나 클라 예측 이동이 매 move마다
	// 보정(ClientAdjustPosition)되며 러버밴딩이 남 — 1인 운반도 동일.
	if (GrabComponent && GrabComponent->GetGrabbedActor())
	{
		return;
	}

	// 달리기 최대 속도 500
	GetCharacterMovement()->MaxWalkSpeed = 500.f;

	// 서버한테 알리기
	ServerStartRun();
}

// 플레이어 달리기 종료 -> 걷기
void ATCPlayerCharacter::StopRun(const FInputActionValue& InValue)
{
	// 운반 중이면 속도 복원도 건너뜀 — 250 하드코딩이 운반 속도를 덮어쓰는 것 방지 (StartRun과 대칭)
	if (GrabComponent && GrabComponent->GetGrabbedActor())
	{
		return;
	}

	// 걷기 속도 250
	GetCharacterMovement()->MaxWalkSpeed = 250.f;

	// 서버한테 알리기
	ServerStopRun();
}

// 상호작용(E키) - 잡기
void ATCPlayerCharacter::Interact(const FInputActionValue& InValue)
{
	// 게시판 클릭 모드 중엔 좌클릭을 가구 잡기(GrabComponent)가 아니라 WidgetInteraction의
	// 클릭으로 넘긴다 — WidgetInteractionComponent는 매 틱 트레이스로 호버 대상만 갱신할 뿐,
	// PressPointerKey를 직접 호출해야만 UMG에 실제 클릭 이벤트가 전달된다.
	if (const ATCPlayerController* PC = Cast<ATCPlayerController>(GetController()))
	{
		if (PC->IsBoardInteractionModeActive())
		{
			if (WidgetInteraction)
			{
				WidgetInteraction->PressPointerKey(EKeys::LeftMouseButton);
			}
			return;
		}
	}

	// 유효성 검사
	if (GrabComponent)
	{
		// 상호작용 - 잡기 실행 명령을 먼저 호출하고 성공 여부를 반환받음
		bool bIsGrabSuccess = GrabComponent->TryInteract();

		// 가구 잡기에 성공(true)했을 경우에만 애니메이션 재생
		if (bIsGrabSuccess && GrabMontage)
		{
			// 로컬 애니메이션 재생
			PlayAnimMontage(GrabMontage);

			// 서버에 애니메이션 전송
			ServerPlayActionMontage(0);
		}

		// 상호작용-잡기 실행 명령
		// GrabComponent->TryInteract();
	}
}

// 상호작용 입력(좌클릭) 뗄 때 - 게시판 클릭 모드 중엔 WidgetInteraction 릴리즈로 전달
void ATCPlayerCharacter::ReleaseInteract(const FInputActionValue& InValue)
{
	if (const ATCPlayerController* PC = Cast<ATCPlayerController>(GetController()))
	{
		if (PC->IsBoardInteractionModeActive() && WidgetInteraction)
		{
			WidgetInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
		}
	}
}

// 상호작용(F키) - 던지기
void ATCPlayerCharacter::Throw(const FInputActionValue& InValue)
{
	// 유효성 검사
	if (GrabComponent)
	{
		// 현재 들고 있는 가구가 있는지 확인
		if (AActor* GrabbedActor = GrabComponent->GetGrabbedActor())
		{
			// 가구 시스템을 가져와서 현재 잡고 있는 인원수 확인
			UFurnitureGrabSystem* FGS = GrabbedActor->FindComponentByClass<UFurnitureGrabSystem>();
			
			// 2명 이상이면 애니메이션 X 던지기 요청 중단
			if (FGS && FGS->GetGrabbedPlayers().Num() >= 2)
			{
				return;
			}
		}

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

// 카메라 시점 변환 함수
void ATCPlayerCharacter::ToggleView(const FInputActionValue& InValue)
{
	if (!SpringArm) return;

	// 상태 반전 (true -> false, false -> true)
	bIsFirstPerson = !bIsFirstPerson;

	if (bIsFirstPerson)
	{
		// 1인칭: 스프링암 길이를 0으로 만들고 높이를 캐릭터 눈높이(약 65)로 올림
		SpringArm->TargetArmLength = 0.f;
		SpringArm->SocketOffset = FVector(30.f, 0.f, 65.f);
	}
	else
	{
		// 3인칭: 원래 길이와 위치로 복구
		SpringArm->TargetArmLength = 400.f;
		SpringArm->SocketOffset = FVector::ZeroVector;
	}
}

// 점프 함수
void ATCPlayerCharacter::TryJump()
{
	// 가구를 들고 있다면 점프 불가 처리 후 함수 종료
	if (GrabComponent && GrabComponent->GetGrabbedActor())
	{
		return;
	}

	Super::Jump();
}

// 가구 z축 회전 함수
void ATCPlayerCharacter::RotateZ(const FInputActionValue& InValue)
{
	if (GrabComponent)
	{
		// FRotator(Y축, Z축, X축)
		// 초당 135도의 속도로 회전 (조절 가능)
		float DeltaTime = GetWorld()->GetDeltaSeconds();
		GrabComponent->TryRotateFurniture(FRotator(0.0f, 135.0f * DeltaTime, 0.0f));
	}
}

// 가구 y축 회전 함수
void ATCPlayerCharacter::RotateY(const FInputActionValue& InValue)
{
	if (GrabComponent)
	{
		// FRotator(Y축, Z축, X축)
		// 초당 135도의 속도로 회전 (조절 가능)
		float DeltaTime = GetWorld()->GetDeltaSeconds();
		GrabComponent->TryRotateFurniture(FRotator(135.0f * DeltaTime, 0.0f, 0.0f));
	}
}

// 카메라 마우스 휠 줌 처리 함수
void ATCPlayerCharacter::HandleZoomInput(const FInputActionValue& InValue)
{
	if (!SpringArm) return;

	// 1인칭 상태일 때는 줌 조절을 제한
	if (bIsFirstPerson) return;

	// 마우스 휠 입력값 가져오기 (보통 위로 굴리면 1, 아래로 굴리면 -1)
	const float ZoomValue = InValue.Get<float>();

	// 한 번 휠을 굴릴 때 변하는 거리 (수치를 올려 속도 조절 가능)
	const float ZoomStep = 40.0f;

	// 마우스 휠을 위로 굴릴 때 줌 인(가까워짐) 마이너스
	float NewArmLength = SpringArm->TargetArmLength - (ZoomValue * ZoomStep);

	// 카메라가 캐릭터 내부로 파고들거나 너무 멀어지지 않게 최소/최대치 설정
	SpringArm->TargetArmLength = FMath::Clamp(NewArmLength, 150.0f, 1000.0f);
}

// 이모트(춤) 처리 함수
void ATCPlayerCharacter::Emote1(const FInputActionValue& InValue)
{
	if (Emote1Montage)
	{
		// 춤 시작 시 캐릭터의 현재 이동 속도를 강제로 0(즉시 정지) 설정
		GetCharacterMovement()->StopMovementImmediately();

		// 로컬 화면에서 먼저 춤 재생
		PlayAnimMontage(Emote1Montage);

		// 서버에 춤 재생 요청 (ActionID 2번)
		ServerPlayActionMontage(2);
	}
}

// 이모트(춤) 처리 함수
void ATCPlayerCharacter::Emote2(const FInputActionValue& InValue)
{
	if (Emote2Montage)
	{
		GetCharacterMovement()->StopMovementImmediately();
		PlayAnimMontage(Emote2Montage);
		ServerPlayActionMontage(3);
	}
}

// 이모트(춤) 처리 함수
void ATCPlayerCharacter::Emote3(const FInputActionValue& InValue)
{
	if (Emote3Montage)
	{
		GetCharacterMovement()->StopMovementImmediately();
		PlayAnimMontage(Emote3Montage);
		ServerPlayActionMontage(4);
	}
}

// 이모트(춤) 처리 함수
void ATCPlayerCharacter::Emote4(const FInputActionValue& InValue)
{
	if (Emote4Montage)
	{
		GetCharacterMovement()->StopMovementImmediately();
		PlayAnimMontage(Emote4Montage);
		ServerPlayActionMontage(5);
	}
}

// 이모트(춤) 취소 판정
void ATCPlayerCharacter::CancelEmote(const FInputActionValue& InValue)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	if (!AnimInstance) return;

	// 현재 이모트 몽타주가 재생 중이라면
	if (AnimInstance && AnimInstance->Montage_IsPlaying(Emote1Montage))
	{
		// 로컬 화면에서 몽타주 재생 중지
		StopAnimMontage(Emote1Montage);

		// 다른 플레이어들 화면에서도 중지되도록 서버에 요청
		ServerStopActionMontage(2);
	}
	else if (Emote2Montage && AnimInstance->Montage_IsPlaying(Emote2Montage))
	{
		StopAnimMontage(Emote2Montage);
		ServerStopActionMontage(3);
	}
	else if (Emote3Montage && AnimInstance->Montage_IsPlaying(Emote3Montage))
	{
		StopAnimMontage(Emote3Montage);
		ServerStopActionMontage(4);
	}
	else if (Emote4Montage && AnimInstance->Montage_IsPlaying(Emote4Montage))
	{
		StopAnimMontage(Emote4Montage);
		ServerStopActionMontage(5);
	}
}

// Multicast - 애니메이션 중지 전체 클라이언트 동기화
void ATCPlayerCharacter::MulticastStopActionMontage_Implementation(int32 ActionID)
{
	// 내 캐릭터가 아닌 다른 플레이어의 캐릭터일 때만 애니메이션 강제 중지
	if (!IsLocallyControlled())
	{
		if (ActionID == 2 && Emote1Montage) StopAnimMontage(Emote1Montage);
		else if (ActionID == 3 && Emote2Montage) StopAnimMontage(Emote2Montage);
		else if (ActionID == 4 && Emote3Montage) StopAnimMontage(Emote3Montage);
		else if (ActionID == 5 && Emote4Montage) StopAnimMontage(Emote4Montage);
	}
}

// Server - 애니메이션 중지 요청 수신
void ATCPlayerCharacter::ServerStopActionMontage_Implementation(int32 ActionID)
{
	MulticastStopActionMontage(ActionID);
}

// 애니메이션 전체 클라이언트 동기화
void ATCPlayerCharacter::MulticastPlayActionMontage_Implementation(int32 ActionID)
{
	// 중복 재생 방지
	// 내가 조종 중인 캐릭터가 아닐 때(남의 화면에서 볼 때)만 애니메이션을 덮어씌워 재생
	if (!IsLocallyControlled())
	{
		// 전달받은 ID에 따라 각자의 PC에 세팅된 몽타주를 안전하게 재생
		if (ActionID == 0 && GrabMontage) PlayAnimMontage(GrabMontage);
		else if (ActionID == 1 && ThrowMontage) PlayAnimMontage(ThrowMontage);
		else if (ActionID == 2 && Emote1Montage) PlayAnimMontage(Emote1Montage);
		else if (ActionID == 3 && Emote2Montage) PlayAnimMontage(Emote2Montage);
		else if (ActionID == 4 && Emote3Montage) PlayAnimMontage(Emote3Montage);
		else if (ActionID == 5 && Emote4Montage) PlayAnimMontage(Emote4Montage);
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
	// 운반 중 도착한 낡은 RPC(그랩 직전 발사) 무시 — 서버 운반 속도 덮어쓰기 방지
	if (GrabComponent && GrabComponent->GetGrabbedActor())
	{
		return;
	}
	GetCharacterMovement()->MaxWalkSpeed = 250.f;
}

// Server - 달리기 시작
void ATCPlayerCharacter::ServerStartRun_Implementation()
{
	if (GrabComponent && GrabComponent->GetGrabbedActor())
	{
		return;
	}
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
}