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
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"

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
			MockController->OnStateChanged.AddUniqueDynamic(this, &ATCPlayerController::HandleUIStateChanged);

			// 맵 이름(GetWorld()->GetName())에 대한 문자열 하드코딩 대신, DefaultGame.ini 로
			// 덮어쓸 수 있는 UTCSessionFlow 의 실제 설정 경로와 직접 비교한다. 그래야 레벨을
			// 옮기거나 이름을 바꿔도(ini 값만 갱신하면) 화면 판별이 계속 맞아떨어진다.
			// GetOutermost()->GetName() 은 패키지 전체 경로("/Game/...")를 돌려주므로 ini 값과
			// 형식이 일치한다. PIE 는 패키지 이름에 "UEDPIE_n_" 접두어를 붙이므로 제거 후 비교한다.
			const UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>();
			const FString CurrentMapPath = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());

			// 1. 타이틀 맵
			if (Flow && CurrentMapPath.Equals(Flow->GetTitleMapPath(), ESearchCase::IgnoreCase))
			{
				MockController->ReplaceState(EE_UIState::MainMenu);
				UE_LOG(LogTCNet, Log, TEXT("[PlayerController] 타이틀 진입: MainMenu 출력."));
				// 메뉴이므로 마우스 커서를 켭니다.
				bShowMouseCursor = true;
				return;
			}
			// 2. 로비 맵 (캐릭터 조작 + 마우스 커서 필요 — 플레이어블 로비, 명세 4장-3)
			else if (Flow && CurrentMapPath.Equals(Flow->GetLobbyMapPath(), ESearchCase::IgnoreCase))
			{
				MockController->ReplaceState(EE_UIState::Lobby);
				UE_LOG(LogTCNet, Log, TEXT("[PlayerController] 로비 진입: S_Lobby 출력."));

				// 로비는 캐릭터 조작이 기본(명세 6장-2 예외 규정). US_Lobby::GetDesiredInputConfig() 가
				// 기본값(bCursorModeActive=false → 커서 숨김/게임 전용 입력)을 이미 선언하므로,
				// 여기서 별도로 입력 모드를 덮어쓸 필요가 없다 — CommonUI 라우터가 화면 활성화 시점에
				// 자동으로 적용한다. Alt(IA_ToggleLobbyCursor)를 눌러야만 커서가 나와 로비 버튼을
				// 조작할 수 있다.
			}
			// 3. 튜토리얼 맵 (캐릭터 조작 필요). 별도의 튜토리얼 맵이 실제로 존재할 때만 매치되며,
			// 스테이지 맵(DefaultStageMapPath 등)과는 ini 상에서 서로 다른 경로를 유지해야 한다.
			else if (Flow && CurrentMapPath.Equals(Flow->GetTutorialMapPath(), ESearchCase::IgnoreCase))
			{
				MockController->ReplaceState(EE_UIState::Tutorial);

				// [추가] 부모 클래스의 UI 모드를 덮어쓰고 캐릭터 조작 권한을 부여합니다.
				bShowMouseCursor = true;
				FInputModeGameAndUI GameAndUIMode;
				SetInputMode(GameAndUIMode);

				UE_LOG(LogTCNet, Log, TEXT("[PlayerController] 튜토리얼 진입: 조작 모드 활성화."));
			}
			// 4. 그 외 실제 인게임 맵 폴백(스테이지 등, 캐릭터 조작 필요)
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

void ATCPlayerController::HandleUIStateChanged(EE_UIState NewState)
{
	if (!IsLocalController())
	{
		return;
	}

	// InGame/Tutorial 로 돌아올 때마다(오버레이를 전부 닫고 게임 화면으로 복귀할 때 포함)
	// BeginPlay() 에서 이미 검증된 방식으로 입력 모드를 다시 확실히 적용한다.
	// CommonUI 라우터의 자동 복원(last-resort 포커스 처리)은 O_PauseMenu 위에서 O_Settings/
	// O_KeyGuide/O_SaveLoad 등을 한 번 더 열었다 닫는 것처럼 오버레이가 2단 이상 중첩되면
	// 신뢰할 수 없어지는 경우가 재현되어, 그 경로를 우회해 직접 복구한다.
	if (NewState == EE_UIState::InGame || NewState == EE_UIState::Tutorial)
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
		}
		bShowMouseCursor = true;
		SetInputMode(FInputModeGameAndUI());
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
