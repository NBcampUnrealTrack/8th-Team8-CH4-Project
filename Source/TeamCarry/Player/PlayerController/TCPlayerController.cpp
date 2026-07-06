// TCPlayerController.cpp

#include "Player/PlayerController/TCPlayerController.h"
#include "Player/PlayerState/TCPlayerState.h"
#include "Network/Session/TCLobbyGameMode.h"
#include "Network/Session/TCSessionFlow.h"
#include "Network/Net/TCNetStatics.h"

// --- UI 테스트용 MockUIController, GameInstance ---
#include "TeamCarry/UI/MockUIController.h"
#include "Engine/GameInstance.h"

// --- 글로벌 UI 단축키(Enhanced Input) ---
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

void ATCPlayerController::RequestSetReady(bool bInReady)
{
	ServerSetReady(bInReady);
}

void ATCPlayerController::RequestStartGame()
{
	ServerRequestStartGame();
}

void ATCPlayerController::RequestSetCharacterIndex(int32 InCharacterIndex)
{
	ServerSetCharacterIndex(InCharacterIndex);
}

// --- UI 테스트용 BeginPlay() ---
void ATCPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 서버 뒷단이 아닌, 실제 모니터 화면을 보고 있는 '로컬 플레이어'일 때만 UI를 띄웁니다.
	if (IsLocalPlayerController())
	{
		// 글로벌 UI 단축키는 폰 스폰 여부·HUD 포커스 상태와 무관하게 항상 동작해야 하므로,
		// 캐릭터 이동 IMC(우선순위 0, ATCPlayerCharacter::BeginPlay)보다 높은 우선순위로 추가한다.
		if (UEnhancedInputLocalPlayerSubsystem* EILPS = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (IMC_GlobalUI)
			{
				EILPS->AddMappingContext(IMC_GlobalUI, 1);
			}
		}

		if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
		{
			FString CurrentMapName = GetWorld()->GetName();

			// 1. 타이틀 맵
			if (CurrentMapName.Contains(TEXT("L_Title")))
			{
				MockController->ReplaceState(EE_UIState::MainMenu);
				UE_LOG(LogTCNet, Log, TEXT("[PlayerController] 타이틀 진입: MainMenu 출력."));
				// 메뉴이므로 마우스 커서를 켭니다.
				bShowMouseCursor = true;
				return;
			}
			// 2. 로비 맵 (캐릭터 조작 + 마우스 커서 필요 — 플레이어블 로비, 명세 4장-3)
			else if (CurrentMapName.Contains(TEXT("L_Lobby")))
			{
				MockController->ReplaceState(EE_UIState::Lobby);
				UE_LOG(LogTCNet, Log, TEXT("[PlayerController] 로비 진입: S_Lobby 출력."));

				// 로비는 캐릭터 조작이 기본(명세 6장-2 예외 규정). US_Lobby::GetDesiredInputConfig() 가
				// 기본값(bCursorModeActive=false → 커서 숨김/게임 전용 입력)을 이미 선언하므로,
				// 여기서 별도로 입력 모드를 덮어쓸 필요가 없다 — CommonUI 라우터가 화면 활성화 시점에
				// 자동으로 적용한다. Alt(IA_ToggleLobbyCursor)를 눌러야만 커서가 나와 로비 버튼을
				// 조작할 수 있다.
			}
			// 3. 튜토리얼 및 프로토타입 맵 (캐릭터 조작 필요)
			else if (CurrentMapName.Contains(TEXT("L_Tutorial")) || CurrentMapName.Contains(TEXT("L_FurnitureProto")))
			{
				MockController->ReplaceState(EE_UIState::Tutorial);

				// [추가] 부모 클래스의 UI 모드를 덮어쓰고 캐릭터 조작 권한을 부여합니다.
				bShowMouseCursor = true;
				FInputModeGameAndUI GameAndUIMode;
				SetInputMode(GameAndUIMode);

				UE_LOG(LogTCNet, Log, TEXT("[PlayerController] 튜토리얼/프로토타입 진입: 조작 모드 활성화."));
			}
			// 4. 그 외 실제 인게임 맵 폴백 (캐릭터 조작 필요)
			else
			{
				MockController->ReplaceState(EE_UIState::InGame);

				// [추가] 인게임 역시 캐릭터 조작이 필요하므로 입력 모드를 덮어씁니다.
				bShowMouseCursor = true;
				FInputModeGameAndUI GameAndUIMode;
				SetInputMode(GameAndUIMode);

				UE_LOG(LogTCNet, Log, TEXT("[PlayerController] 인게임 맵 진입: 조작 모드 활성화."));
			}
		}
	}
}

void ATCPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(IA_ToggleESCUI, ETriggerEvent::Started, this, &ThisClass::Input_ToggleESCUI);
		EIC->BindAction(IA_SkipTutorial, ETriggerEvent::Started, this, &ThisClass::Input_SkipTutorial);
		EIC->BindAction(IA_ToggleLobbyCursor, ETriggerEvent::Started, this, &ThisClass::Input_ToggleLobbyCursor);
	}
}

void ATCPlayerController::Input_ToggleESCUI()
{
	UMockUIController* MockController = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr;
	if (!MockController)
	{
		return;
	}

	// InGame/Tutorial 화면일 때만 일시정지 오버레이를 연다.
	// 닫기(뒤로가기)는 O_PauseMenu 위젯의 NativeOnHandleBackAction(CommonUI 표준)이 자체 처리한다.
	const EE_UIState CurrentState = MockController->GetCurrentState();
	if (CurrentState == EE_UIState::InGame || CurrentState == EE_UIState::Tutorial)
	{
		MockController->PushOverlay(TEXT("O_PauseMenu"));
	}
}

void ATCPlayerController::SetLobbyCursorActive(bool bInActive)
{
	// 로비 밖(메인메뉴/인게임 등)에서 잘못 호출돼도 다른 화면의 입력 모드를 건드리지 않도록 가드한다.
	const UMockUIController* MockController = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr;
	if (!MockController || MockController->GetCurrentState() != EE_UIState::Lobby)
	{
		return;
	}

	bLobbyCursorActive = bInActive;
	bShowMouseCursor = bInActive;

	if (bInActive)
	{
		// 커서를 꺼내 로비 인라인 버튼(Btn_CharacterSelect 등)을 클릭할 수 있게 한다.
		SetInputMode(FInputModeGameAndUI());
	}
	else
	{
		// 캐릭터 조작 모드로 복귀. 커서를 숨기고 게임 전용 입력으로 전환한다.
		SetInputMode(FInputModeGameOnly());
	}
}

void ATCPlayerController::Input_ToggleLobbyCursor()
{
	UMockUIController* MockController = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr;
	if (!MockController || MockController->GetCurrentState() != EE_UIState::Lobby)
	{
		return;
	}

	// O_CharacterSelect/O_StageSelect 등 오버레이가 열려 있으면 그쪽 GetDesiredInputConfig 가 이미
	// 입력을 UI 전용으로 강제하고 있으므로 끼어들지 않는다.
	if (MockController->IsAnyOverlayActive())
	{
		return;
	}

	SetLobbyCursorActive(!bLobbyCursorActive);
}

void ATCPlayerController::Input_SkipTutorial()
{
	UMockUIController* MockController = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr;
	if (!MockController)
	{
		return;
	}

	// Tutorial 상태일 때만 스킵이 유효하다(다른 화면에서 실수로 눌려도 무시).
	// 명세 4장-6: 마지막 Step 완료와 동일하게 HostReturnToLobby() 로 S_Lobby 복귀(L_StageSelect 직행 폐기).
	if (MockController->GetCurrentState() == EE_UIState::Tutorial)
	{
		if (UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr)
		{
			Flow->CompleteTutorial();
		}
	}
}

void ATCPlayerController::ServerSetReady_Implementation(bool bInReady)
{
	if (ATCPlayerState* PS = GetPlayerState<ATCPlayerState>())
	{
		PS->SetReadyAuthoritative(bInReady);
	}
}

void ATCPlayerController::ServerSetCharacterIndex_Implementation(int32 InCharacterIndex)
{
	if (ATCPlayerState* PS = GetPlayerState<ATCPlayerState>())
	{
		PS->SetCharacterIndexAuthoritative(InCharacterIndex);
	}
}

void ATCPlayerController::ServerRequestStartGame_Implementation()
{
	// 시작 권한·전원 준비 검증은 GameMode 에 위임(서버 권위).
	if (ATCLobbyGameMode* LobbyGM = GetWorld() ? GetWorld()->GetAuthGameMode<ATCLobbyGameMode>() : nullptr)
	{
		LobbyGM->StartGameFromLobby(this);
	}
	else
	{
		UE_LOG(LogTCNet, Warning, TEXT("ServerRequestStartGame: ATCLobbyGameMode 없음 — 무시"));
	}
}
