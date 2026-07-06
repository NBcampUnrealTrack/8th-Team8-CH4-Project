// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/O_CharacterSelect.h"
#include "CommonButtonBase.h"
#include "Components/TextBlock.h"
#include "Input/CommonUIInputTypes.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Player/PlayerController/TCPlayerController.h"
#include "Player/PlayerState/TCPlayerState.h"
#include "Network/Session/TCLobbyGameState.h"
#include "Engine/World.h"

void UO_CharacterSelect::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Option0)
	{
		Btn_Option0->OnClicked().AddUObject(this, &UO_CharacterSelect::HandleOption0Clicked);
	}
	if (Btn_Option1)
	{
		Btn_Option1->OnClicked().AddUObject(this, &UO_CharacterSelect::HandleOption1Clicked);
	}
	if (Btn_Option2)
	{
		Btn_Option2->OnClicked().AddUObject(this, &UO_CharacterSelect::HandleOption2Clicked);
	}
	if (Btn_Close)
	{
		Btn_Close->OnClicked().AddUObject(this, &UO_CharacterSelect::HandleCloseClicked);
	}

	SetIsFocusable(true);

	if (UWorld* World = GetWorld())
	{
		if (ATCLobbyGameState* LobbyGS = World->GetGameState<ATCLobbyGameState>())
		{
			LobbyGS->OnLobbyPlayersChanged.AddUniqueDynamic(this, &UO_CharacterSelect::HandleLobbyPlayersChanged);
			BoundLobbyState = LobbyGS;
		}
	}

	RefreshPreviewFromPlayerState();
}

void UO_CharacterSelect::NativeDestruct()
{
	if (ATCLobbyGameState* LobbyGS = BoundLobbyState.Get())
	{
		LobbyGS->OnLobbyPlayersChanged.RemoveAll(this);
	}
	BoundLobbyState = nullptr;

	Super::NativeDestruct();
}

TOptional<FUIInputConfig> UO_CharacterSelect::GetDesiredInputConfig() const
{
	// 명세 4장-4/6장-2: 활성화 중 게임 입력을 차단하고 UI 전용(Menu) 입력으로 전환한다.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

bool UO_CharacterSelect::NativeOnHandleBackAction()
{
	HandleCloseClicked();
	return true;
}

ATCPlayerController* UO_CharacterSelect::GetTCPlayerController() const
{
	return Cast<ATCPlayerController>(GetOwningPlayer());
}

void UO_CharacterSelect::RequestSelectOption(int32 InCharacterIndex)
{
	if (ATCPlayerController* PC = GetTCPlayerController())
	{
		PC->RequestSetCharacterIndex(InCharacterIndex);
	}

	// 낙관적 갱신: 서버 왕복 전에도 미리보기 텍스트를 즉시 반영한다(즉시 적용 방식, 확인 버튼 없음).
	if (Txt_Preview)
	{
		Txt_Preview->SetText(FText::Format(NSLOCTEXT("O_CharacterSelect", "PreviewFormat", "선택된 외형: {0}"), FText::AsNumber(InCharacterIndex)));
	}
}

void UO_CharacterSelect::RefreshPreviewFromPlayerState()
{
	if (!Txt_Preview)
	{
		return;
	}

	if (const ATCPlayerState* LocalTCPS = GetOwningPlayerState<ATCPlayerState>())
	{
		Txt_Preview->SetText(FText::Format(NSLOCTEXT("O_CharacterSelect", "PreviewFormat", "선택된 외형: {0}"), FText::AsNumber(LocalTCPS->GetCharacterIndex())));
	}
}

void UO_CharacterSelect::HandleLobbyPlayersChanged()
{
	RefreshPreviewFromPlayerState();
}

void UO_CharacterSelect::HandleOption0Clicked()
{
	RequestSelectOption(0);
}

void UO_CharacterSelect::HandleOption1Clicked()
{
	RequestSelectOption(1);
}

void UO_CharacterSelect::HandleOption2Clicked()
{
	RequestSelectOption(2);
}

void UO_CharacterSelect::HandleCloseClicked()
{
	// 명세 4장-4: 닫기(Btn_Close/ESC) → PopCurrentOverlay() 로 S_Lobby 복귀.
	// (S_Lobby 는 자신의 커서 모드 상태를 스스로 기억하므로 여기서 별도로 복원할 필요가 없다.)
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->PopCurrentOverlay();
	}
}
