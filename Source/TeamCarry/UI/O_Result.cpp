// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/O_Result.h"
#include "CommonButtonBase.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Input/CommonUIInputTypes.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Network/Session/TCSessionFlow.h"
#include "TeamCarry/Core/TeamCarryGameState.h"
#include "Engine/World.h"

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
			// 별 개수는 Img_Star0~2 이미지 교체로 표시하므로 텍스트는 계속 숨겨둔다.
			Txt_StarCount->SetVisibility(ESlateVisibility::Collapsed);
		}

		UpdateStarDisplay(StarCount);

		if (Bar_Progress)
		{
			// S_InGame의 PB_TeamMoney(4장-7)와 동일 기준(TotalScore/TotalLevelValue)을 그대로 재사용한다.
			// TotalLevelValue는 스테이지 중 불변이라 결과 화면 시점에도 GameState에 그대로 남아 있다.
			float LevelValuePercent = 0.0f;
			if (const UWorld* World = GetWorld())
			{
				if (const ATeamCarryGameState* GS = World->GetGameState<ATeamCarryGameState>())
				{
					LevelValuePercent = GS->TotalLevelValue > 0 ? (float)FinalScore / (float)GS->TotalLevelValue : 0.0f;
				}
			}
			TargetProgressPercent = FMath::Clamp(LevelValuePercent, 0.0f, 1.0f);
			// 0%에서 시작해 NativeTick의 보간으로 목표치까지 차오르게 한다(즉시 SetPercent 하지 않음).
			Bar_Progress->SetPercent(DisplayedProgressPercent);
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

	// 스테이지별 결과 배경: FStageInfo::ResultBackgroundImage 가 설정된 스테이지만 교체하고,
	// 미설정이면 WBP 디자이너에 지정된 기본 배경을 그대로 둔다.
	if (Background)
	{
		if (const UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr)
		{
			FStageInfo Info;
			if (Flow->FindStageInfo(Flow->GetSelectedStageId(), Info) && !Info.ResultBackgroundImage.IsNull())
			{
				if (UTexture2D* BackgroundTexture = Info.ResultBackgroundImage.LoadSynchronous())
				{
					Background->SetBrushFromTexture(BackgroundTexture);
				}
			}
		}
	}
}

void UO_Result::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Bar_Progress && !FMath::IsNearlyEqual(DisplayedProgressPercent, TargetProgressPercent, 0.001f))
	{
		DisplayedProgressPercent = FMath::FInterpTo(DisplayedProgressPercent, TargetProgressPercent, InDeltaTime, 4.0f);
		Bar_Progress->SetPercent(DisplayedProgressPercent);
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

void UO_Result::UpdateStarDisplay(int32 StarCount)
{
	const TObjectPtr<UImage> StarImages[] = { Img_Star0, Img_Star1, Img_Star2 };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(StarImages); ++Index)
	{
		if (UImage* StarImage = StarImages[Index])
		{
			UTexture2D* Texture = (Index < StarCount) ? StarTexture_Filled : StarTexture_Empty;
			if (Texture)
			{
				StarImage->SetBrushFromTexture(Texture);
			}
		}
	}
}
