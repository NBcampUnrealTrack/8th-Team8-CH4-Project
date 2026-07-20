// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/S_Lobby.h"
#include "CommonButtonBase.h"
#include "Components/TextBlock.h"
#include "Input/CommonUIInputTypes.h"
#include "TeamCarry/UI/MockUIController.h"
#include "TeamCarry/UI/O_Confirm.h"
#include "Player/PlayerController/TCPlayerController.h"
#include "Player/PlayerState/TCPlayerState.h"
#include "Network/Session/TCLobbyGameState.h"
#include "Network/Session/TCSessionFlow.h"
#include "Level/Struct/TCStageSelectBoard.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

void US_Lobby::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_CharacterSelect)
	{
		Btn_CharacterSelect->OnClicked().AddUObject(this, &US_Lobby::HandleCharacterSelectClicked);
	}
	if (Btn_StageSelect)
	{
		Btn_StageSelect->OnClicked().AddUObject(this, &US_Lobby::HandleStageSelectClicked);
	}
	if (Btn_Ready)
	{
		Btn_Ready->OnClicked().AddUObject(this, &US_Lobby::HandleReadyClicked);
	}
	if (Btn_Start)
	{
		Btn_Start->OnClicked().AddUObject(this, &US_Lobby::HandleStartClicked);
		Btn_Start->SetIsEnabled(false); // 전원 준비 완료 전까지 비활성.
	}
	if (Btn_Back)
	{
		Btn_Back->OnClicked().AddUObject(this, &US_Lobby::HandleBackClicked);
	}
	if (Btn_KeyGuide)
	{
		Btn_KeyGuide->OnClicked().AddUObject(this, &US_Lobby::HandleKeyGuideClicked);
	}

	// BP_StageSelectBoard 근접 프롬프트 구독(명세 4장-5).
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->OnInteractTargetChanged.AddUniqueDynamic(this, &US_Lobby::HandleInteractTargetChanged);
	}
	if (Txt_InteractPrompt)
	{
		Txt_InteractPrompt->SetVisibility(ESlateVisibility::Collapsed);
	}

	bLocalPlayerReady = false;
	// SetIsFocusable(true) 를 두면 오버레이(O_Confirm 등)가 닫혀 이 위젯이 leaf-most 로
	// 복귀할 때, 포커스 대상이 없어 라우터가 이 위젯 자체에 키보드 포커스를 last-resort로
	// 박아버려 캐릭터 조작이 막힌다(S_InGame 과 동일 버그, 상세 사유는 S_InGame.cpp 참고).

	if (!TryBindNetworkLobby())
	{
		// 클라이언트는 로비 GameState 복제가 1틱 늦게 도착할 수 있다(S_CharacterSelect 승계).
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					TryBindNetworkLobby();
				}));
		}
	}
}

void US_Lobby::NativeDestruct()
{
	if (ATCLobbyGameState* LobbyGS = BoundLobbyState.Get())
	{
		LobbyGS->OnLobbyPlayersChanged.RemoveAll(this);
	}
	BoundLobbyState = nullptr;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMockUIController* MockController = GI->GetSubsystem<UMockUIController>())
		{
			MockController->OnInteractTargetChanged.RemoveAll(this);
		}
	}

	Super::NativeDestruct();
}

bool US_Lobby::NativeOnHandleBackAction()
{
	// (v3 내부 개정) ESC → O_PauseMenu(Lobby 컨텍스트)를 연다. 기존에는 Btn_Back과 동일하게
	// O_Confirm(방 나가기)로 직행해 로비에서 설정(O_Settings)에 접근할 경로가 없었다.
	// Btn_Back 클릭은 기존처럼 HandleBackClicked()의 O_Confirm 직행 단축 경로를 그대로 유지한다.
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->PushOverlay(TEXT("O_PauseMenu"));
	}
	return true;
}

TOptional<FUIInputConfig> US_Lobby::GetDesiredInputConfig() const
{
	// 명세 6장-2: 로비는 캐릭터 조작이 기본값 — 커서를 숨기고 게임 전용 입력을 받는다(S_InGame과 동일 패턴).
	// CommonUI 라우터는 화면 전환처럼 위젯 트리가 바뀌는 시점에만 이 값을 다시 조회하므로, 이 값을
	// 항상 "Game"으로 고정해 두면 라우터가 임의로 Menu 기본값으로 되돌리는 일이 없다. Alt 로 커서를
	// 꺼내는 것은 ATCPlayerController::SetLobbyCursorActive() 가 SetInputMode 를 직접 호출해 처리하며,
	// 여기서 트리 변경이 함께 일어나지 않는 한(오버레이 Push/Pop 등) 그 값이 그대로 유지된다.
	return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently);
}

bool US_Lobby::TryBindNetworkLobby()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	ATCLobbyGameState* LobbyGS = World->GetGameState<ATCLobbyGameState>();
	if (!LobbyGS)
	{
		return false;
	}
	LobbyGS->OnLobbyPlayersChanged.AddUniqueDynamic(this, &US_Lobby::HandleLobbyPlayersChanged);
	BoundLobbyState = LobbyGS;
	RefreshLobbyFromGameState();
	return true;
}

ATCPlayerController* US_Lobby::GetTCPlayerController() const
{
	return Cast<ATCPlayerController>(GetOwningPlayer());
}

void US_Lobby::GetSlotTexts(int32 SlotIndex, UTextBlock*& OutName, UTextBlock*& OutStatus) const
{
	OutName = nullptr;
	OutStatus = nullptr;
	switch (SlotIndex)
	{
	case 0: OutName = Txt_Slot1_Name; OutStatus = Txt_Slot1_Status; break;
	case 1: OutName = Txt_Slot2_Name; OutStatus = Txt_Slot2_Status; break;
	case 2: OutName = Txt_Slot3_Name; OutStatus = Txt_Slot3_Status; break;
	case 3: OutName = Txt_Slot4_Name; OutStatus = Txt_Slot4_Status; break;
	default: break;
	}
}

void US_Lobby::RefreshHostOnlyVisibility()
{
	// 명세 6장-8: 방장 전용 버튼 분기는 IsHost() 로 판정한다(별도 bIsHost 필드 신설 금지).
	const UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr;
	const bool bIsHost = Flow && Flow->IsHost();

	if (Btn_StageSelect)
	{
		Btn_StageSelect->SetVisibility(bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Btn_Start)
	{
		Btn_Start->SetVisibility(bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void US_Lobby::RefreshLobbyFromGameState()
{
	ATCLobbyGameState* LobbyGS = BoundLobbyState.Get();
	if (!LobbyGS)
	{
		return;
	}

	RefreshHostOnlyVisibility();

	// 방 코드: 빈 값이면 덮어쓰지 않는다(진짜 오프라인 표기는 유지 — S_CharacterSelect 승계).
	if (Txt_RoomCode)
	{
		const FString RoomCodeString = LobbyGS->GetRoomCode();
		if (!RoomCodeString.IsEmpty())
		{
			Txt_RoomCode->SetText(FText::FromString(FString::Printf(TEXT("방 코드: %s"), *RoomCodeString)));
		}
	}

	// 먼저 모든 슬롯을 빈 상태로.
	for (int32 i = 0; i < 4; ++i)
	{
		UTextBlock* NameText = nullptr;
		UTextBlock* StatusText = nullptr;
		GetSlotTexts(i, NameText, StatusText);
		if (NameText)
		{
			NameText->SetText(FText::FromString(TEXT("---")));
			NameText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		}
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("EMPTY")));
	}

	// 복제된 PlayerState 로 슬롯 채우기.
	const APlayerState* LocalPS = GetOwningPlayerState();
	for (APlayerState* PS : LobbyGS->PlayerArray)
	{
		const ATCPlayerState* TCPS = Cast<ATCPlayerState>(PS);
		if (!TCPS)
		{
			continue;
		}
		const int32 SlotIdx = TCPS->GetLobbySlotIndex();
		if (SlotIdx < 0 || SlotIdx > 3)
		{
			continue;
		}
		UTextBlock* NameText = nullptr;
		UTextBlock* StatusText = nullptr;
		GetSlotTexts(SlotIdx, NameText, StatusText);

		// 규칙: 1P 호스트 고정(명세 4장-3) — 슬롯 0이 곧 방장이므로 별도 판정 없이 슬롯 인덱스로 표시한다.
		const bool bIsHostSlot = (SlotIdx == 0);
		const bool bIsSelf = (PS == LocalPS);

		if (NameText)
		{
			const FString Prefix = bIsHostSlot ? TEXT("[방장] ") : TEXT("");
			const FString Suffix = bIsSelf ? TEXT(" (나)") : TEXT("");
			NameText->SetText(FText::FromString(Prefix + TCPS->GetPlayerName() + Suffix));

			// 로비 리스트는 입장 순서대로 빨강-파랑-노랑-초록 순으로 표시한다.
			// 주의: 캐릭터 밑 링 데칼(BP_PlayerCharacter::DecalColor)은 ColorIndex 0=파랑,
			// 1=빨강으로 구현되어 있어 이 순서와 어긋난다(0/1 반전). 데칼 쪽도 맞추려면
			// BP_PlayerCharacter::DecalColor 매핑을 0=빨강/1=파랑으로 함께 바꿔야 한다.
			// 2/3(노랑/초록) 데칼은 아직 미구현이라 여기 값은 잠정값이다.
			FLinearColor SlotColor = FLinearColor::White;
			switch (TCPS->GetColorIndex())
			{
			case 0: SlotColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f); break; // 빨강
			case 1: SlotColor = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f); break; // 파랑
			case 2: SlotColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f); break; // 노랑(잠정)
			case 3: SlotColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f); break; // 초록(잠정)
			default: break; // ColorIndex 미배정(-1) 등 — 흰색 유지
			}
			NameText->SetColorAndOpacity(FSlateColor(SlotColor));

			// 가시성 강화: 글자색보다 어두운 톤으로 아웃라인을 둘러 배경(밝은 크림색 패널) 위에서도
			// 잘 읽히게 한다.
			FSlateFontInfo NameFont = NameText->GetFont();
			NameText->SetFont(NameFont);
		}
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(TCPS->IsReady() ? TEXT("READY") : TEXT("NOT READY")));
		}
	}

	// 로컬 플레이어의 실제 복제 Ready 값으로 토글 기준을 동기화(다인원 환경에서 desync 방지).
	if (const ATCPlayerState* LocalTCPS = GetOwningPlayerState<ATCPlayerState>())
	{
		bLocalPlayerReady = LocalTCPS->IsReady();
	}

	// 방장만: 전원 준비 시 시작 버튼 활성.
	if (Btn_Start)
	{
		const UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr;
		if (Flow && Flow->IsHost())
		{
			Btn_Start->SetIsEnabled(LobbyGS->AreAllPlayersReady());
		}
	}
}

void US_Lobby::HandleLobbyPlayersChanged()
{
	RefreshLobbyFromGameState();
}

void US_Lobby::HandleCharacterSelectClicked()
{
	// O_CharacterSelect 는 3단계에서 생성 예정 — 아직 매핑이 없으면 PushOverlay 가 nullptr 을
	// 반환하고 라우터가 상태를 롤백한다(호출부만 배선).
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->PushOverlay(TEXT("O_CharacterSelect"));
	}
}

void US_Lobby::HandleStageSelectClicked()
{
	// (v3 내부 개정, 명세 4장-3·4장-5) O_StageSelect 를 직접 열지 않는다 — 방장 캐릭터를
	// BP_StageSelectBoard(ATCStageSelectBoard) 앞으로 순간이동시키는 편의 기능으로 축소되었다.
	// 실제 오버레이는 텔레포트 후 보드와의 기존 Interact 상호작용(ATCStageSelectBoard::OnInteract)으로 연다.
	// 씬에 보드가 유일하다고 가정한다(명세 4장-5).
	TArray<AActor*> Boards;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATCStageSelectBoard::StaticClass(), Boards);
	if (Boards.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI S_Lobby] StageSelect: BP_StageSelectBoard 를 찾지 못함"));
		return;
	}

	const ATCStageSelectBoard* Board = Cast<ATCStageSelectBoard>(Boards[0]);
	if (!Board)
	{
		return;
	}

	if (APawn* Pawn = GetOwningPlayerPawn())
	{
		// 리슨 서버 구조상 방장 자신의 위젯 호출이므로, 이 폰은 서버 시점에서도 로컬 권위를 갖는다.
		Pawn->TeleportTo(Board->GetTeleportLocation(), Board->GetTeleportRotation(), false, true);
	}
}

void US_Lobby::HandleReadyClicked()
{
	bLocalPlayerReady = !bLocalPlayerReady;
	if (ATCPlayerController* PC = GetTCPlayerController())
	{
		PC->RequestSetReady(bLocalPlayerReady);
	}
}

void US_Lobby::HandleStartClicked()
{
	if (ATCPlayerController* PC = GetTCPlayerController())
	{
		PC->RequestStartGame();
	}
}

void US_Lobby::HandleBackClicked()
{
	// 명세: 로비 ──뒤로──▶ (O_Confirm: 방 나가기) ──▶ [S_MainMenu].
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UCommonActivatableWidget* OverlayWidget = MockController->PushOverlay(TEXT("O_Confirm"));

		if (UO_Confirm* ConfirmUI = Cast<UO_Confirm>(OverlayWidget))
		{
			FOnConfirmYesAction YesAction;
			YesAction.BindDynamic(this, &US_Lobby::OnConfirmLeaveLobby);

			ConfirmUI->SetupConfirm(
				FText::FromString(TEXT("로비 나가기")),
				FText::FromString(TEXT("정말로 로비를 나가시겠습니까?")),
				YesAction
			);
		}
	}
}

void US_Lobby::HandleInteractTargetChanged(AActor* Target, FString Key)
{
	if (!Txt_InteractPrompt)
	{
		return;
	}
	if (Target)
	{
		Txt_InteractPrompt->SetText(FText::FromString(Key));
		Txt_InteractPrompt->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		Txt_InteractPrompt->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void US_Lobby::HandleKeyGuideClicked()
{
	// 조작법 팝업을 연다.
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI PauseMenu] KeyGuide clicked. Pushing O_KeyGuide overlay."));
		MockController->PushOverlay(TEXT("O_KeyGuide"));
	}
}

void US_Lobby::OnConfirmLeaveLobby()
{
	// TCSessionFlow::LeaveToTitle 이 세션 파기와 타이틀 레벨 이동을 함께 처리한다.
	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		Flow->LeaveToTitle();
	}
}
