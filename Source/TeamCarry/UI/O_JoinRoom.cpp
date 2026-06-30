// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/O_JoinRoom.h"
#include "Components/EditableTextBox.h"
#include "TeamCarry/UI/MockUIController.h"
#include "CommonButtonBase.h"
#include "Network/Session/TCGameInstance.h"
#include "Input/CommonUIInputTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UO_JoinRoom::UO_JoinRoom()
{
	// 강제 모달: Back(ESC)을 이 위젯이 직접 처리하도록 잡아둔다.
	bIsBackHandler = true;
	// 활성화 시 포커스를 가져와 하위 위젯의 입력 포커스를 차단한다.
	bSupportsActivationFocus = true;
}

void UO_JoinRoom::NativeConstruct()
{
	Super::NativeConstruct();

	// 모드 선택 버튼 바인딩
	if (Btn_CreateRoom)
	{
		Btn_CreateRoom->OnClicked().AddUObject(this, &UO_JoinRoom::HandleCreateRoomClicked);
	}

	if (Btn_JoinRoom)
	{
		Btn_JoinRoom->OnClicked().AddUObject(this, &UO_JoinRoom::HandleCreateRoomClicked);
	}

	// 코드 입력 변경 감지(방 참가 시 확인 버튼 활성/비활성 판단)
	if (Txt_Code)
	{
		Txt_Code->OnTextChanged.AddUniqueDynamic(this, &UO_JoinRoom::HandleCodeTextChanged);
	}

	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked().AddUObject(this, &UO_JoinRoom::HandleCreateRoomClicked);
	}

	// 초기 상태: 방 코드가 입력되지 않았으므로 방 참가 버튼 비활성화
	if (Btn_JoinRoom)		
	{
		Btn_JoinRoom->SetIsEnabled(false);
	}
}

TOptional<FUIInputConfig> UO_JoinRoom::GetDesiredInputConfig() const
{
	// Menu 모드 = UI Only. 모달이 떠 있는 동안 하위 위젯/게임으로 입력이 가지 않는다.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

UWidget* UO_JoinRoom::NativeGetDesiredFocusTarget() const
{
	// 기본 포커스는 '방 만들기'에 둔다.
	if (Btn_CreateRoom)
	{
		return Btn_CreateRoom;
	}

	return Super::NativeGetDesiredFocusTarget();
}

bool UO_JoinRoom::NativeOnHandleBackAction()
{
	// ESC = 취소와 동일. 라우터를 통해 닫아 상태/스택 동기화를 유지한다(명세 5-1).
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] Back(ESC) handled as Cancel. Popping via MockUIController."));
		MockController->PopCurrentOverlay();
	}
	// 처리했음을 알려 상위 스택으로 Back 전파를 막는다.
	return true;
}

void UO_JoinRoom::HandleCreateRoomClicked()
{
	UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>();
	if (!MockController) return;

	// '방 만들기' 클릭 시 O_JoinRoom 팝업을 닫고 세이브 슬롯 선택 화면으로 교체
	UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] Hosting room. Replacing state to SlotSelect..."));
	MockController->PopCurrentOverlay();
	MockController->ReplaceState(EE_UIState::SlotSelect);
}

void UO_JoinRoom::HandleJoinRoomClicked()
{
	UTCGameInstance* TCGI = Cast<UTCGameInstance>(GetGameInstance());
	if (!TCGI || !Txt_Code) return;

	FString TargetRoomCode = Txt_Code->GetText().ToString();
	if (TargetRoomCode.IsEmpty()) return;

	UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] Joining room. Searching for sessions with code: %s"), *TargetRoomCode);

	// 다중 검색 요청을 막기 위해 버튼 임시 비활성화
	if (Btn_JoinRoom) Btn_JoinRoom->SetIsEnabled(false);
	if (Btn_CreateRoom) Btn_CreateRoom->SetIsEnabled(false);

	TCGI->OnFindSessionsComplete.AddUniqueDynamic(this, &UO_JoinRoom::OnFindSessionsComplete);
	TCGI->FindSteamSessions(false, 20);
}

void UO_JoinRoom::HandleCancelClicked()
{
	// 취소: 팝업만 닫는다(하위 풀스크린은 이미 S_MainMenu 이므로 복귀됨, 명세 3-8).
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] Cancel clicked. Closing popup, returning to MainMenu."));
		MockController->PopCurrentOverlay();
	}
}

void UO_JoinRoom::HandleCodeTextChanged(const FText& Text)
{
	// 코드 입력란에 1글자 이상 존재하는 경우에만 참가 버튼 활성화
	if (Btn_JoinRoom)
	{
		Btn_JoinRoom->SetIsEnabled(!Text.IsEmpty());
	}
}

void UO_JoinRoom::OnFindSessionsComplete(bool bWasSuccessful, int32 NumResults)
{
	UTCGameInstance* TCGI = Cast<UTCGameInstance>(GetGameInstance());
	if (!TCGI) return;

	TCGI->OnFindSessionsComplete.RemoveDynamic(this, &UO_JoinRoom::OnFindSessionsComplete);

	if (bWasSuccessful && NumResults > 0)
	{
		FString TargetRoomCode = Txt_Code->GetText().ToString();
		int32 TargetSessionIndex = -1;

		for (int32 i = 0; i < NumResults; ++i)
		{
			if (TCGI->GetFoundSessionName(i) == TargetRoomCode)
			{
				TargetSessionIndex = i;
				break;
			}
		}

		if (TargetSessionIndex != -1)
		{
			UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] Match found. Executing JoinFoundSession..."));
			TCGI->OnJoinSessionComplete.AddUniqueDynamic(this, &UO_JoinRoom::OnJoinSessionComplete);
			TCGI->JoinFoundSession(TargetSessionIndex);
			return;
		}
	}

	// 실패 처리: 매칭되는 세션이 없는 경우
	UE_LOG(LogTemp, Warning, TEXT("[UI JoinRoom] Session search failed or room code not found."));

	if (Btn_JoinRoom) Btn_JoinRoom->SetIsEnabled(true);
	if (Btn_CreateRoom) Btn_CreateRoom->SetIsEnabled(true);

	// 에러 팝업 호출
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->PushOverlay(TEXT("O_Error"));
	}
}

void UO_JoinRoom::OnJoinSessionComplete(bool bWasSuccessful)
{
	UTCGameInstance* TCGI = Cast<UTCGameInstance>(GetGameInstance());
	if (TCGI)
	{
		TCGI->OnJoinSessionComplete.RemoveDynamic(this, &UO_JoinRoom::OnJoinSessionComplete);
	}

	if (bWasSuccessful)
	{
		// 조인에 성공하면 TCGameInstance에서 ClientTravel을 수행하므로 UI 코드는 대기합니다.
		UE_LOG(LogTemp, Log, TEXT("[UI JoinRoom] Successfully joined session. Waiting for ClientTravel."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[UI JoinRoom] Join session failed."));

		if (Btn_JoinRoom) Btn_JoinRoom->SetIsEnabled(true);
		if (Btn_CreateRoom) Btn_CreateRoom->SetIsEnabled(true);

		// 에러 팝업 호출
		if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
		{
			MockController->PushOverlay(TEXT("O_Error"));
		}
	}
}