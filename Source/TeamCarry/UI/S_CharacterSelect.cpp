// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/S_CharacterSelect.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "TeamCarry/UI/MockUIController.h"
#include "TeamCarry/UI/O_Confirm.h"
#include "Player/PlayerController/TCPlayerController.h"
#include "Player/PlayerState/TCPlayerState.h"
#include "Network/Session/TCLobbyGameState.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "Network/Session/TCSessionFlow.h"

void US_CharacterSelect::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind buttons
	if (Btn_Ready)
	{
		Btn_Ready->OnClicked.AddUniqueDynamic(this, &US_CharacterSelect::HandleReadyClicked);
	}

	if (Btn_Start)
	{
		Btn_Start->OnClicked.AddUniqueDynamic(this, &US_CharacterSelect::HandleStartClicked);
		Btn_Start->SetIsEnabled(false); // Disabled initially until everyone is ready
	}

	if (Btn_Back)
	{
		Btn_Back->OnClicked.AddUniqueDynamic(this, &US_CharacterSelect::HandleBackClicked);
	}

	bLocalPlayerReady = false;
	SetIsFocusable(true);

	// ── 조기 종료(return)를 만나기 전에 방 코드를 가장 먼저 출력하도록 위로 끌어올림 ──
	if (Txt_Session_Code)
	{
		if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
		{
			FString RoomCodeString = Flow->GetRoomCode();
			if (!RoomCodeString.IsEmpty())
			{
				FString FormattedCode = FString::Printf(TEXT("방 코드: %s"), *RoomCodeString);
				Txt_Session_Code->SetText(FText::FromString(FormattedCode));
			}
		}
	}

	// ── 네트워크 로비가 있으면 복제 데이터 경로, 없으면 mock 경로 ──
	bNetworkedLobby = TryBindNetworkLobby();
	if (bNetworkedLobby)
	{
		// 클라이언트는 Start 를 누를 수 없다(호스트 독점).
		const bool bIsHost = GetWorld() && GetWorld()->GetNetMode() != NM_Client;
		if (Btn_Start)
		{
			Btn_Start->SetVisibility(bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		RefreshLobbyFromGameState();

		// 바로 이 return 때문에 밑에 있던 코드들이 씹혔습니다.
		return;
	}

	// 클라이언트에서 로비 GameState 복제가 1틱 늦게 도착하는 경우를 위한 지연 재바인딩.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (!bNetworkedLobby && TryBindNetworkLobby())
				{
					bNetworkedLobby = true;
					const bool bIsHost = GetWorld() && GetWorld()->GetNetMode() != NM_Client;
					if (Btn_Start)
					{
						Btn_Start->SetVisibility(bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
					}
					RefreshLobbyFromGameState();
				}
			}));
	}

	// ── mock 폴백(프로토타입 단일레벨) ──
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->OnLobbySlotUpdated.AddUniqueDynamic(this, &US_CharacterSelect::HandleLobbySlotUpdated);
	}

	if (Txt_Slot1_Name) Txt_Slot1_Name->SetText(FText::FromString(TEXT("Player_1 (You)")));
	if (Txt_Slot1_Status) Txt_Slot1_Status->SetText(FText::FromString(TEXT("NOT READY")));
	if (Txt_Slot2_Name) Txt_Slot2_Name->SetText(FText::FromString(TEXT("Player_2 (AI)")));
	if (Txt_Slot2_Status) Txt_Slot2_Status->SetText(FText::FromString(TEXT("READY")));
	if (Txt_Slot3_Name) Txt_Slot3_Name->SetText(FText::FromString(TEXT("Player_3 (AI)")));
	if (Txt_Slot3_Status) Txt_Slot3_Status->SetText(FText::FromString(TEXT("READY")));
	if (Txt_Slot4_Name) Txt_Slot4_Name->SetText(FText::FromString(TEXT("Player_4 (AI)")));
	if (Txt_Slot4_Status) Txt_Slot4_Status->SetText(FText::FromString(TEXT("READY")));
}

void US_CharacterSelect::NativeDestruct()
{
	// mock 경로 정리
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->OnLobbySlotUpdated.RemoveAll(this);
	}

	// 네트워크 경로 정리
	if (ATCLobbyGameState* LobbyGS = BoundLobbyState.Get())
	{
		LobbyGS->OnLobbyPlayersChanged.RemoveAll(this);
	}
	BoundLobbyState = nullptr;

	Super::NativeDestruct();
}

bool US_CharacterSelect::TryBindNetworkLobby()
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
	LobbyGS->OnLobbyPlayersChanged.AddUniqueDynamic(this, &US_CharacterSelect::HandleLobbyPlayersChanged);
	BoundLobbyState = LobbyGS;
	return true;
}

ATCPlayerController* US_CharacterSelect::GetTCPlayerController() const
{
	return Cast<ATCPlayerController>(GetOwningPlayer());
}

void US_CharacterSelect::GetSlotTexts(int32 SlotIndex, UTextBlock*& OutName, UTextBlock*& OutStatus) const
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

void US_CharacterSelect::HandleReadyClicked()
{
	bLocalPlayerReady = !bLocalPlayerReady;
	UE_LOG(LogTemp, Log, TEXT("[UI CharacterSelect] Btn_Ready Clicked. Ready State: %s"), bLocalPlayerReady ? TEXT("TRUE") : TEXT("FALSE"));

	if (bNetworkedLobby)
	{
		// 서버 권위에 위임. 시작버튼 활성/슬롯표시는 복제 갱신(HandleLobbyPlayersChanged)에서 처리.
		if (ATCPlayerController* PC = GetTCPlayerController())
		{
			PC->RequestSetReady(bLocalPlayerReady);
		}
		return;
	}

	// ── mock 폴백 ──
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->SetLobbySlotReady(0, bLocalPlayerReady);
		if (Btn_Start)
		{
			Btn_Start->SetIsEnabled(bLocalPlayerReady);
		}
	}
}

void US_CharacterSelect::HandleStartClicked()
{
	if (bNetworkedLobby)
	{
		// 호스트 시작 요청 → 서버가 전원 준비 검증 후 ServerTravel.
		if (ATCPlayerController* PC = GetTCPlayerController())
		{
			UE_LOG(LogTemp, Warning, TEXT("[UI CharacterSelect] Host requested start (networked)."));
			PC->RequestStartGame();
		}
		return;
	}

	// ── mock 폴백 ──
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI CharacterSelect] Host started the game (mock). Transitioning to S_Tutorial..."));
		MockController->ReplaceState(EE_UIState::Tutorial);
	}
}

bool US_CharacterSelect::NativeOnHandleBackAction()
{
	// ESC = 뒤로 가기 버튼과 동일 처리(명세 5-1). O_Confirm 모달을 거쳐 로비를 나간다.
	HandleBackClicked();
	return true;
}

void US_CharacterSelect::HandleBackClicked()
{
	// 명세: 캐릭터 선택/로비 ──뒤로──▶ (O_Confirm: 방 종료) ──▶ [S_MainMenu].
	// 파괴적 액션(방 나가기)이므로 즉시 전환하지 않고 확인 모달을 먼저 띄운다.
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI CharacterSelect] Back clicked. Pushing O_Confirm overlay."));

		UCommonActivatableWidget* OverlayWidget = MockController->PushOverlay(TEXT("O_Confirm"));

		if (UO_Confirm* ConfirmUI = Cast<UO_Confirm>(OverlayWidget))
		{
			FOnConfirmYesAction YesAction;
			YesAction.BindDynamic(this, &US_CharacterSelect::OnConfirmLeaveLobby);

			ConfirmUI->SetupConfirm(
				FText::FromString(TEXT("로비 나가기")),
				FText::FromString(TEXT("정말로 로비를 나가시겠습니까?")),
				YesAction
			);
		}
	}
}

void US_CharacterSelect::OnConfirmLeaveLobby()
{
	// TCSessionFlow::LeaveToTitle 이 세션 파기와 타이틀 레벨(L_Title) 이동을 함께 처리한다.
	// ReplaceState(MainMenu)만 호출하면 로비 레벨/세션이 그대로 남아, 이후 캐릭터 선택 화면에
	// 재입장할 때 세션·슬롯 상태가 꼬여 다시 들어갈 수 없게 된다.
	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI CharacterSelect] Leave confirmed. Requesting Leave To Title."));
		Flow->LeaveToTitle();
	}
}

void US_CharacterSelect::HandleLobbyPlayersChanged()
{
	RefreshLobbyFromGameState();
}

void US_CharacterSelect::RefreshLobbyFromGameState()
{
	ATCLobbyGameState* LobbyGS = BoundLobbyState.Get();
	if (!LobbyGS)
	{
		return;
	}

	// 방 코드 갱신: 클라이언트는 복제(RoomCode)가 위젯 생성 이후에 도착할 수 있어
	// NativeConstruct 1회 표시만으론 "오프라인"에 머문다 → 로비 변경 통지마다 재확인.
	// (빈 값이면 덮어쓰지 않아 진짜 오프라인 표기는 유지)
	if (Txt_Session_Code)
	{
		if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
		{
			const FString RoomCodeString = Flow->GetRoomCode();
			if (!RoomCodeString.IsEmpty())
			{
				Txt_Session_Code->SetText(FText::FromString(FString::Printf(TEXT("방 코드: %s"), *RoomCodeString)));
			}
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

	// 호스트만: 전원 준비 시 시작버튼 활성.
	const bool bIsHost = GetWorld() && GetWorld()->GetNetMode() != NM_Client;
	if (Btn_Start && bIsHost)
	{
		Btn_Start->SetIsEnabled(LobbyGS->AreAllPlayersReady());
	}
}

void US_CharacterSelect::HandleLobbySlotUpdated(int32 SlotIndex, FPlayerInfo PlayerInfo)
{
	// mock 경로 전용.
	FText StatusText = PlayerInfo.bIsReady ? FText::FromString(TEXT("READY")) : FText::FromString(TEXT("NOT READY"));

	UTextBlock* NameText = nullptr;
	UTextBlock* SlotStatus = nullptr;
	GetSlotTexts(SlotIndex, NameText, SlotStatus);
	if (SlotStatus)
	{
		SlotStatus->SetText(StatusText);
	}
}
