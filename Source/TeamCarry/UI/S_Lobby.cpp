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

	Super::NativeDestruct();
}

bool US_Lobby::NativeOnHandleBackAction()
{
	// ESC = 뒤로 가기 버튼과 동일 처리(명세 5-1). O_Confirm 모달을 거쳐 로비를 나간다.
	HandleBackClicked();
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
		if (NameText) NameText->SetText(FText::FromString(TEXT("---")));
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("EMPTY")));
	}

	// 복제된 PlayerState 로 슬롯 채우기.
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
		if (NameText)
		{
			NameText->SetText(FText::FromString(TCPS->GetPlayerName()));
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
	// O_StageSelect 는 5단계에서 생성 예정 — 호출부만 배선.
	// 노출 분기(IsHost)는 편의일 뿐 권위가 아니므로, 실제 확정은 서버 측에서 재검증한다(명세 6장-8).
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->PushOverlay(TEXT("O_StageSelect"));
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

void US_Lobby::OnConfirmLeaveLobby()
{
	// TCSessionFlow::LeaveToTitle 이 세션 파기와 타이틀 레벨 이동을 함께 처리한다.
	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		Flow->LeaveToTitle();
	}
}
