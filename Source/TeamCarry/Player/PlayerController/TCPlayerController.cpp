// TCPlayerController.cpp

#include "Player/PlayerController/TCPlayerController.h"
#include "Player/PlayerState/TCPlayerState.h"
#include "Player/Character/TCPlayerCharacter.h"
#include "Network/Session/TCLobbyGameMode.h"
#include "Network/Session/TCLobbyGameState.h"
#include "Network/Session/TCSessionFlow.h"
#include "Kismet/GameplayStatics.h"
#include "Network/Net/TCNetStatics.h"
#include "Core/TeamCarryGameMode.h"
#include "Components/WidgetInteractionComponent.h"
#include "Level/Struct/TCStageSelectBoard.h"
#include "TeamCarry/UI/W_StageBoardScreen.h"
#include "Blueprint/UserWidget.h"

// --- UI 테스트용 MockUIController, GameInstance ---
#include "TeamCarry/UI/MockUIController.h"
#include "Engine/GameInstance.h"
#include "TeamCarry/UI/S_Loading.h"
#include "Input/CommonUIActionRouterBase.h"
#include "CommonInputModeTypes.h"

// --- 글로벌 UI 단축키(Enhanced Input) ---
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "TimerManager.h"

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

		// 게시판 클릭 모드용 빨간 점 커서 위젯을 미리 만들어 둔다(뷰포트 추가/제거는 게시판
		// 클릭 모드 진입/종료 시점에 한다). PlayerTick()이 매 프레임 마우스 위치로 옮긴다.
		if (BoardSelectCursorWidgetClass)
		{
			BoardSelectCursorWidgetInstance = CreateWidget<UUserWidget>(this, BoardSelectCursorWidgetClass);
			if (BoardSelectCursorWidgetInstance)
			{
				BoardSelectCursorWidgetInstance->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));

				// 커서는 순수 시각 표시일 뿐이어야 한다. 기본값(Visible)이면 마우스가 이 위젯 위에
				// 있을 때(=항상, 마우스를 따라다니므로) Slate 2D 히트 테스트가 이 위젯을 클릭 대상으로
				// 잡아버려, WidgetInteractionComponent가 노리는 게시판(월드 스페이스) 클릭이 새는
				// 원인이 됐다 — 클릭할 때마다 커서가 잠깐 (0,0) 쪽으로 튀고 클릭도 씹히던 버그.
				BoardSelectCursorWidgetInstance->SetVisibility(ESlateVisibility::HitTestInvisible);
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
				MockController->HideLoadingScreenNow();
				return;
			}
			// 2. 로비 맵 (캐릭터 조작 + 마우스 커서 필요 — 플레이어블 로비, 명세 4장-3)
			else if (Flow && CurrentMapPath.Equals(Flow->GetLobbyMapPath(), ESearchCase::IgnoreCase))
			{
				MockController->ReplaceState(EE_UIState::Lobby);
				UE_LOG(LogTCNet, Log, TEXT("[PlayerController] 로비 진입: S_Lobby 출력."));
				MockController->HideLoadingScreenNow();

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
				MockController->HideLoadingScreenNow();
			}
			// 4. 그 외 실제 인게임 맵 폴백(스테이지 등, 캐릭터 조작 필요)
			else
			{
				// 내 화면의 로딩이 끝나는 즉시 InGame 으로 전환한다 — ESC 등 글로벌 입력이
				// Input_ToggleESCUI() 의 CurrentState 게이트(InGame/Tutorial/Lobby)를 처음부터
				// 통과하도록 하기 위함(다른 클라이언트를 기다리는 동안 CurrentState 가 Loading에
				// 머물러 ESC 가 무반응으로 보이던 문제 수정). "전원 로딩 완료" 대기는 더 이상 UI
				// 상태와 묶지 않고 GameState::CurrentPhase(WaitingToStart -> Countdown, 서버 권위)로만
				// 게이팅한다 — ClientNotifyAllPlayersLoaded()는 그 신호 전달용으로 남는다.
				MockController->ReplaceState(EE_UIState::InGame);
				bShowMouseCursor = true;
				SetInputMode(FInputModeGameAndUI());

				// 이 시점이 곧 "내 맵 로딩 완료"라는 실제 사건이다 — 시간 기반 연출(0→90%)을
				// 끊고 게이지를 내 로딩 완료 구간(99%)으로 올린다. 화면은 여전히 지속형 로딩
				// 위젯이 덮고 있으며(ReplaceState(InGame) 는 이제 위젯을 내리지 않는다),
				// 하강은 전원 완료 후 FinishLoadingScreen() 이 단독으로 담당한다.
				if (US_Loading* LoadingWidget = MockController->GetActiveLoadingWidget())
				{
					LoadingWidget->NotifyLocalLoadComplete();
				}

				StartMapLoadedReportRetry();
				UE_LOG(LogTCNet, Log, TEXT("[PlayerController] 인게임 맵 진입: InGame 전환 완료, 로딩 완료 보고."));
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
		EIC->BindAction(IA_BoardListUp, ETriggerEvent::Started, this, &ThisClass::Input_BoardListUp);
		EIC->BindAction(IA_BoardListDown, ETriggerEvent::Started, this, &ThisClass::Input_BoardListDown);
		EIC->BindAction(IA_LobbyReady, ETriggerEvent::Started, this, &ThisClass::Input_LobbyReady);
		EIC->BindAction(IA_LobbyStart, ETriggerEvent::Started, this, &ThisClass::Input_LobbyStart);
		EIC->BindAction(IA_LobbyStageSelect, ETriggerEvent::Started, this, &ThisClass::Input_LobbyStageSelect);
		EIC->BindAction(IA_LobbyHelp, ETriggerEvent::Started, this, &ThisClass::Input_LobbyHelp);
	}
}

void ATCPlayerController::Input_ToggleESCUI()
{
	// 로딩 화면이 아직 덮여 있으면(전원 로딩 대기 중) ESC 를 무시한다. 이 구간의 CurrentState 는
	// 이미 InGame 이라 아래 게이트를 통과해 O_PauseMenu 가 열리지만, 그 오버레이는 RootLayout
	// 소속이라 지속형 로딩 위젯(ZOrder 1000) 아래에 깔려 보이지 않는다 — 눌러도 아무 일 없어
	// 보이면서 스택만 쌓이는 상태가 된다. 이 구간의 탈출 경로는 로딩 화면의 Btn_LeaveSession 이다.
	if (UMockUIController* Mock = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr)
	{
		if (Mock->GetActiveLoadingWidget())
		{
			return;
		}
	}

	// 게시판 클릭 모드 중이면 ESC는 그 모드를 먼저 닫는다(일시정지 메뉴보다 우선, 명세 4장-5).
	if (bBoardInteractionModeActive)
	{
		ExitBoardInteractionMode();
		return;
	}

	UMockUIController* MockController = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr;
	if (!MockController)
	{
		return;
	}

	// InGame/Tutorial/Lobby 화면일 때만 일시정지 오버레이를 연다(v3 내부 개정 — Lobby 추가).
	// S_Lobby도 S_InGame과 동일하게 GetDesiredInputConfig가 순수 Game 모드라서, CommonUI의
	// NativeOnHandleBackAction(Back 액션)은 키보드로 호출되지 않는다 — 여기(Enhanced Input)가
	// 유일하게 동작하는 ESC 경로다.
	// 닫기(뒤로가기)는 O_PauseMenu 위젯의 NativeOnHandleBackAction(CommonUI 표준)이 자체 처리한다.
	const EE_UIState CurrentState = MockController->GetCurrentState();
	if (CurrentState == EE_UIState::InGame || CurrentState == EE_UIState::Tutorial || CurrentState == EE_UIState::Lobby)
	{
		MockController->PushOverlay(TEXT("O_PauseMenu"));
	}
}

void ATCPlayerController::ClientShowLoadingScreen_Implementation()
{
	if (UMockUIController* MockController = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr)
	{
		MockController->ShowLoadingScreenNow();
	}
}

void ATCPlayerController::ServerReportMapLoaded_Implementation()
{
	if (ATeamCarryGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATeamCarryGameMode>() : nullptr)
	{
		GM->NotifyPlayerFinishedLoading(this);
	}
	else
	{
		// 스테이지 GameMode 가 아니거나 캐스팅 실패 — 무한 대기를 막기 위해 안전하게 즉시 진입.
		UE_LOG(LogTCNet, Warning, TEXT("ServerReportMapLoaded: ATeamCarryGameMode 없음 — 즉시 진입으로 폴백"));
		ClientNotifyAllPlayersLoaded();
	}
}

void ATCPlayerController::StartMapLoadedReportRetry()
{
	ServerReportMapLoaded();

	// 서버 응답(ClientNotifyAllPlayersLoaded)이 올 때까지 1초 간격 재전송.
	// 호스트(리슨서버)는 RPC 가 로컬 직접 호출이라 첫 발에서 바로 응답이 와 타이머가 곧 정리된다.
	GetWorldTimerManager().SetTimer(MapLoadedReportRetryHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			UE_LOG(LogTCNet, Warning, TEXT("[PlayerController] 로딩 완료 보고 응답 없음 — 재전송"));
			ServerReportMapLoaded();
		}), 1.0f, true);
}

void ATCPlayerController::ClientNotifyAllPlayersLoaded_Implementation()
{
	// 서버가 보고를 접수했다 — 재전송 중단.
	GetWorldTimerManager().ClearTimer(MapLoadedReportRetryHandle);

	UMockUIController* MockController = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr;

	// BeginPlay() 의 스테이지 맵 분기가 이미 InGame 전환 + 입력 모드를 적용해 두었으므로 정상
	// 경로에서 아래 두 호출은 idempotent no-op 이다. 재접속/후발 합류 경로의 안전망으로 유지한다.
	if (MockController)
	{
		MockController->ReplaceState(EE_UIState::InGame);
	}
	bShowMouseCursor = true;
	SetInputMode(FInputModeGameAndUI());

	// MoviePlayer 는 hard travel(타이틀 복귀/최초 조인) 경로 전용이다. 스테이지 진입은
	// ATeamCarryGameMode 의 bUseSeamlessTravel=true 이후 UEngine::LoadMap 을 타지 않아
	// FCoreUObjectDelegates::PreLoadMap 이 발화하지 않으므로 이 호출은 사실상 no-op 이지만,
	// 다른 경로를 위해 그대로 둔다. (2026-07-19: seamless travel에서도 MoviePlayer를 걸어보는
	// 시도를 했다가 게임 스레드 행이 발생해 되돌렸다 — TCSessionFlow::HostServerTravel 주석 참고.)
	if (UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr)
	{
		Flow->StopMovieLoadingScreen();
	}

	// 지속형 로딩 위젯(ZOrder 1000)에 90→100% 마무리 연출을 재생시키고, 그게 끝난 뒤에 내린다.
	// 여기서 바로 내리면 100% 가 한 프레임도 보이지 않는다.
	US_Loading* LoadingWidget = MockController ? MockController->GetActiveLoadingWidget() : nullptr;
	if (!LoadingWidget)
	{
		FinishLoadingScreen();   // 로딩 화면이 없는 경로(재접속 등) — 즉시 진행
		return;
	}

	TWeakObjectPtr<ATCPlayerController> WeakThis = this;
	LoadingWidget->OnFinishAnimationCompleted.AddWeakLambda(this, [WeakThis]()
		{
			if (!WeakThis.IsValid()) return;
			WeakThis->GetWorldTimerManager().ClearTimer(WeakThis->FinishAnimSafetyHandle);
			WeakThis->FinishLoadingScreen();
		});
	LoadingWidget->PlayFinishAnimation();

	// 세이프티: 연출 도중 위젯이 파괴되면 델리게이트가 영영 오지 않아, 이 클라이언트만 로딩
	// 화면이 영구히 덮인 채 남는다(서버는 이미 게이트를 통과해 카운트다운 중이라 구제해주지 않는다).
	GetWorldTimerManager().SetTimer(FinishAnimSafetyHandle, [WeakThis]()
		{
			if (WeakThis.IsValid()) { WeakThis->FinishLoadingScreen(); }
		}, LoadingWidget->FinishDuration + 0.5f, false);

	UE_LOG(LogTCNet, Log, TEXT("[PlayerController] 전원 로딩 완료 확인 — 마무리 연출 시작."));
}

void ATCPlayerController::ClientNotifyLoadingStalled_Implementation()
{
	if (UMockUIController* MockController = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr)
	{
		if (US_Loading* LoadingWidget = MockController->GetActiveLoadingWidget())
		{
			LoadingWidget->NotifyLoadingStalled();
		}
	}
	UE_LOG(LogTCNet, Warning, TEXT("[PlayerController] 로딩 게이트 지연 안내 수신."));
}

void ATCPlayerController::FinishLoadingScreen()
{
	if (bLoadingScreenFinished)
	{
		return;
	}
	bLoadingScreenFinished = true;

	if (UMockUIController* MockController = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr)
	{
		MockController->HideLoadingScreenNow();
	}

	// (2026-07-19) RefreshUIInputConfig()(라우터가 위젯 트리를 다시 스캔해 leaf-most를 스스로
	// 재계산)을 두 번 시도했으나(동일 프레임 / 한 틱 지연) 두 번 다 오히려 완전히 안 먹히는
	// 쪽으로 악화됐다 — "재계산을 요청"하는 방식 자체가 이 프로젝트 환경에서 신뢰할 수 없다고
	// 판단, 대신 S_InGame::GetDesiredInputConfig()와 동일한 값을 라우터에 직접 지정한다
	// (SetActiveUIInputConfig — 트리를 스캔하지 않고 그냥 그 값을 쓰므로 "무엇을 leaf-most로
	// 착각하는가"와 무관하다). 로딩 위젯이 사라진 직후 수 초간 마우스 캡처가 CaptureDuringMouseDown
	// 에 멈춰 카메라 조작이 안 되고, 그 상태에서 눌린 키의 Release 이벤트가 씹혀 캐릭터가 계속
	// 움직이는(예: W 한 번만 눌러도 계속 전진) 문제가 전부 이 공백 구간 때문이었다(실측 확인).
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UCommonUIActionRouterBase* ActionRouter = LP->GetSubsystem<UCommonUIActionRouterBase>())
		{
			ActionRouter->SetActiveUIInputConfig(FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently, true));
		}
	}
}
void ATCPlayerController::ClientEnterBoardInteractionMode_Implementation(ATCStageSelectBoard* Board)
{
	UE_LOG(LogTemp, Warning, TEXT("[PlayerController] ClientEnterBoardInteractionMode 진입"));
	if (bBoardInteractionModeActive)
	{
		return;
	}
	bBoardInteractionModeActive = true;
	ActiveBoard = Board;

	// 마우스 커서를 노출해 WidgetInteractionComponent 가 BoardScreen 을 클릭할 수 있게 한다
	// (S_Lobby 의 Alt 커서 토글, SetLobbyCursorActive 와 동일한 입력 모드 패턴).
	bShowMouseCursor = true;
	SetInputMode(FInputModeGameAndUI());

	// 게시판 클릭(선택) 모드임을 한눈에 알 수 있도록 빨간 점 위젯을 뷰포트에 띄운다.
	// PlayerTick()이 매 프레임 위치를 마우스로 옮긴다.
	if (BoardSelectCursorWidgetInstance && !BoardSelectCursorWidgetInstance->IsInViewport())
	{
		BoardSelectCursorWidgetInstance->AddToViewport(9999);
	}

	if (const ATCPlayerCharacter* Char = Cast<ATCPlayerCharacter>(GetPawn()))
	{
		if (UWidgetInteractionComponent* Interaction = Char->GetWidgetInteraction())
		{
			Interaction->SetActive(true);
		}
	}
}

void ATCPlayerController::ExitBoardInteractionMode()
{
	if (!bBoardInteractionModeActive)
	{
		return;
	}
	bBoardInteractionModeActive = false;

	// SetInputMode/커서 전환을 지금 이 자리(확인·취소 버튼의 OnClicked 콜백 스택, 즉
	// WidgetInteraction::ReleasePointerKey 처리 도중)에서 바로 적용하면, Slate가 마우스 캡처를
	// 게임 뷰포트로 즉시 돌려주면서 그 클릭을 캐릭터의 좌클릭(Interact) 입력으로 다시 잡아버려
	// 확인/취소를 누르자마자 게시판 모드가 즉시 재진입되는 무한 루프가 발생했다(현재 클릭 이벤트
	// 처리가 끝나기 전에 입력 모드를 바꾼 것이 원인). 다음 틱으로 미뤄 우회한다.
	TWeakObjectPtr<ATCPlayerController> WeakThis(this);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
		{
			ATCPlayerController* StrongThis = WeakThis.Get();
			if (!StrongThis)
			{
				return;
			}

			if (const ATCPlayerCharacter* Char = Cast<ATCPlayerCharacter>(StrongThis->GetPawn()))
			{
				if (UWidgetInteractionComponent* Interaction = Char->GetWidgetInteraction())
				{
					Interaction->SetActive(false);
				}
			}

			// 캐릭터 조작 모드로 복귀. 커서를 숨기고 게임 전용 입력으로 전환한다.
			StrongThis->bShowMouseCursor = false;
			StrongThis->SetInputMode(FInputModeGameOnly());
			StrongThis->ActiveBoard = nullptr;

			if (StrongThis->BoardSelectCursorWidgetInstance && StrongThis->BoardSelectCursorWidgetInstance->IsInViewport())
			{
				StrongThis->BoardSelectCursorWidgetInstance->RemoveFromParent();
			}
		}));
	}
}

void ATCPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (bBoardInteractionModeActive && BoardSelectCursorWidgetInstance && BoardSelectCursorWidgetInstance->IsInViewport())
	{
		float MouseX = 0.f;
		float MouseY = 0.f;
		if (GetMousePosition(MouseX, MouseY))
		{
			BoardSelectCursorWidgetInstance->SetPositionInViewport(FVector2D(MouseX, MouseY), true);
		}
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
	// 게시판 클릭 모드는 별도로 커서/입력 모드를 소유한다(ClientEnterBoardInteractionMode/
	// ExitBoardInteractionMode). 여기서 끼어들면 두 상태가 서로 모르는 채 bShowMouseCursor와
	// SetInputMode를 번갈아 덮어써 커서가 꼬인다 — ESC(Input_ToggleESCUI)로 먼저 닫아야 한다.
	if (bBoardInteractionModeActive)
	{
		return;
	}

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

void ATCPlayerController::Input_BoardListUp()
{
	if (!bBoardInteractionModeActive)
	{
		return;
	}
	if (ATCStageSelectBoard* Board = ActiveBoard.Get())
	{
		if (UW_StageBoardScreen* BoardScreen = Board->GetBoardScreenWidget())
		{
			BoardScreen->NavigateStageSelection(-1);
		}
	}
}

void ATCPlayerController::Input_BoardListDown()
{
	if (!bBoardInteractionModeActive)
	{
		return;
	}
	if (ATCStageSelectBoard* Board = ActiveBoard.Get())
	{
		if (UW_StageBoardScreen* BoardScreen = Board->GetBoardScreenWidget())
		{
			BoardScreen->NavigateStageSelection(1);
		}
	}
}

bool ATCPlayerController::CanHandleLobbyShortcut() const
{
	if (bBoardInteractionModeActive)
	{
		return false;
	}

	const UMockUIController* MockController = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr;
	if (!MockController || MockController->GetCurrentState() != EE_UIState::Lobby)
	{
		return false;
	}

	// O_PauseMenu/O_Confirm 등 오버레이가 열려 있으면 그쪽이 입력을 소유 중이므로 끼어들지 않는다
	// (Input_ToggleLobbyCursor 와 동일 가드).
	if (MockController->IsAnyOverlayActive())
	{
		return false;
	}

	return true;
}

void ATCPlayerController::Input_LobbyReady()
{
	if (!CanHandleLobbyShortcut())
	{
		return;
	}

	const ATCPlayerState* LocalTCPS = GetPlayerState<ATCPlayerState>();
	const bool bCurrentlyReady = LocalTCPS && LocalTCPS->IsReady();
	RequestSetReady(!bCurrentlyReady);
}

void ATCPlayerController::Input_LobbyStart()
{
	if (!CanHandleLobbyShortcut())
	{
		return;
	}

	// Btn_Start 와 동일한 노출/활성 조건(방장 + 전원 준비)을 확인한다(명세 6장-8, IsHost() 로만 판정).
	const UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr;
	if (!Flow || !Flow->IsHost())
	{
		return;
	}

	const ATCLobbyGameState* LobbyGS = GetWorld() ? GetWorld()->GetGameState<ATCLobbyGameState>() : nullptr;
	if (!LobbyGS || !LobbyGS->AreAllPlayersReady())
	{
		return;
	}

	RequestStartGame();
}

void ATCPlayerController::Input_LobbyStageSelect()
{
	if (!CanHandleLobbyShortcut())
	{
		return;
	}

	// Btn_StageSelect 와 동일하게 방장 전용이다(명세 4장-3).
	const UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr;
	if (!Flow || !Flow->IsHost())
	{
		return;
	}

	// S_Lobby::HandleStageSelectClicked 와 동일한 로직 — 씬에 유일한 BP_StageSelectBoard 앞으로 텔레포트한다.
	TArray<AActor*> Boards;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATCStageSelectBoard::StaticClass(), Boards);
	if (Boards.Num() == 0)
	{
		UE_LOG(LogTCNet, Warning, TEXT("[PlayerController] Input_LobbyStageSelect: BP_StageSelectBoard 를 찾지 못함"));
		return;
	}

	const ATCStageSelectBoard* Board = Cast<ATCStageSelectBoard>(Boards[0]);
	if (!Board)
	{
		return;
	}

	if (APawn* thisPawn = GetPawn())
	{
		thisPawn->TeleportTo(Board->GetTeleportLocation(), Board->GetTeleportRotation(), false, true);
	}
}

void ATCPlayerController::Input_LobbyHelp()
{
	if (!CanHandleLobbyShortcut())
	{
		return;
	}

	// Btn_KeyGuide 와 동일하게 조작법 팝업을 연다(전원 사용 가능).
	if (UMockUIController* MockController = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr)
	{
		MockController->PushOverlay(TEXT("O_KeyGuide"));
	}
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
