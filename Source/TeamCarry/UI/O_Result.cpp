// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/O_Result.h"
#include "CommonButtonBase.h"
#include "Components/TextBlock.h"
#include "Input/CommonUIInputTypes.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Network/Session/TCSessionFlow.h"

void UO_Result::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_ToLobby)
	{
		Btn_ToLobby->OnClicked().AddUObject(this, &UO_Result::HandleToLobbyClicked);
	}
	if (Btn_ToTitle)
	{
		Btn_ToTitle->OnClicked().AddUObject(this, &UO_Result::HandleToTitleClicked);
	}

	SetIsFocusable(true);

	// 로비 복귀 트래블은 호스트 전용 — 프로토타입은 호스트 전용 활성화를 기본으로 한다(명세 4장-8).
	if (Btn_ToLobby)
	{
		const UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr;
		const bool bIsHost = Flow && Flow->IsHost();
		Btn_ToLobby->SetVisibility(bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// O_Result 는 PushOverlay() 로 동기 생성되므로, TriggerGameResult() 가 캐시해 둔 값을 바로 읽는다.
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		const int32 FinalScore = MockController->GetLastFinalScore();
		const int32 StarCount = MockController->GetLastStarCount();
		const float ElapsedTime = MockController->GetLastElapsedTime();

		if (Txt_Score)
		{
			Txt_Score->SetText(FText::Format(NSLOCTEXT("O_Result", "ScoreFormat", "${0}"), FText::AsNumber(FinalScore)));
		}

		if (Txt_StarCount)
		{
			Txt_StarCount->SetText(FText::AsNumber(StarCount));
		}

		if (Txt_ElapsedTime)
		{
			const int32 TotalSeconds = FMath::Max(0, FMath::FloorToInt(ElapsedTime));
			const int32 Minutes = TotalSeconds / 60;
			const int32 Seconds = TotalSeconds % 60;

			Txt_ElapsedTime->SetText(FText::FromString(FString::Printf(TEXT("클리어 시간: %02d 분 %02d 초"), Minutes, Seconds)));
		}

		UE_LOG(LogTemp, Log, TEXT("[UI Result] Result Display Updated: Score=%d, Star=%d, Time=%.1fs"), FinalScore, StarCount, ElapsedTime);
	}
}

TOptional<FUIInputConfig> UO_Result::GetDesiredInputConfig() const
{
	// 명세 4장-8: 활성화 중 게임 입력을 완전히 차단하고 UI 전용(Menu) 입력으로 전환한다.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

bool UO_Result::NativeOnHandleBackAction()
{
	// 강제 모달: ESC 로 닫을 수 없다. Back 전파만 차단하고 아무 동작도 하지 않는다.
	return true;
}

void UO_Result::HandleToLobbyClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->PopCurrentOverlay();
	}

	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		Flow->HostReturnToLobby();
	}
}

void UO_Result::HandleToTitleClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->PopCurrentOverlay();
	}

	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		Flow->LeaveToTitle();
	}
}
