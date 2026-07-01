// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/S_CharacterSelect.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Player/PlayerController/TCPlayerController.h"
#include "Player/PlayerState/TCPlayerState.h"
#include "Network/Session/TCLobbyGameState.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

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
		return;
	}

	// 클라이언트에서 로비 GameState 복제가 1틱 늦게 도착하는 경우를 위한 지연 재바인딩.
	// (싱글/순수 mock 환경이면 다음 틱에도 GameState 가 없으므로 그대로 mock 유지)
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

	if (Slot1_Name) Slot1_Name->SetText(FText::FromString(TEXT("Player_1 (You)")));
	if (Slot1_Status) Slot1_Status->SetText(FText::FromString(TEXT("NOT READY")));
	if (Slot2_Name) Slot2_Name->SetText(FText::FromString(TEXT("Player_2 (AI)")));
	if (Slot2_Status) Slot2_Status->SetText(FText::FromString(TEXT("READY")));
	if (Slot3_Name) Slot3_Name->SetText(FText::FromString(TEXT("Player_3 (AI)")));
	if (Slot3_Status) Slot3_Status->SetText(FText::FromString(TEXT("READY")));
	if (Slot4_Name) Slot4_Name->SetText(FText::FromString(TEXT("Player_4 (AI)")));
	if (Slot4_Status) Slot4_Status->SetText(FText::FromString(TEXT("READY")));
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
	case 0: OutName = Slot1_Name; OutStatus = Slot1_Status; break;
	case 1: OutName = Slot2_Name; OutStatus = Slot2_Status; break;
	case 2: OutName = Slot3_Name; OutStatus = Slot3_Status; break;
	case 3: OutName = Slot4_Name; OutStatus = Slot4_Status; break;
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

void US_CharacterSelect::HandleBackClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI CharacterSelect] Returning to Main Menu."));
		MockController->ReplaceState(EE_UIState::MainMenu);
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
		const int32 Slot = TCPS->GetLobbySlotIndex();
		if (Slot < 0 || Slot > 3)
		{
			continue;
		}
		UTextBlock* NameText = nullptr;
		UTextBlock* StatusText = nullptr;
		GetSlotTexts(Slot, NameText, StatusText);
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
