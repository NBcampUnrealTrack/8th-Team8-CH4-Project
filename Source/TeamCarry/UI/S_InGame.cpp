// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/S_InGame.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "TeamCarry/UI/MockUIController.h"
#include "TeamCarry/Core/TeamCarryGameState.h"
#include "Engine/Texture2D.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Engine/World.h"

void US_InGame::NativeConstruct()
{
	Super::NativeConstruct();

	// SetIsFocusable(true) 를 두면 오버레이가 모두 닫혀 이 위젯이 leaf-most 로 복귀할 때,
	// 포커스 대상(NativeGetDesiredFocusTarget)이 없으므로 라우터가 "이 위젯 자체"에 키보드
	// 포커스를 last-resort로 박아버린다. 그러면 WASD 가 뷰포트(→ 캐릭터)로 가지 못하고 이
	// 빈 HUD 위젯에서 막혀 조작 불능이 된다. Focusable 을 끄면 라우터가 게임 뷰포트로
	// 포커스를 넘긴다(O_PauseMenu 등 다른 위젯이 이미 그렇게 동작하는 것과 동일한 경로).

	// Setup initial placeholder values
	if (PB_TeamMoney)
	{
		PB_TeamMoney->SetPercent(0.0f);
	}
	if (TextBlock_Score)
	{
		TextBlock_Score->SetText(FText::FromString(TEXT("0 / 0")));
	}
	if (TextBlock_Timer)
	{
		TextBlock_Timer->SetText(FText::FromString(TEXT("00 : 00")));
	}
	if (TextBlock_InteractPrompt)
	{
		TextBlock_InteractPrompt->SetText(FText::GetEmpty());
	}
	if (TextBlock_WarnPlayers)
	{
		// 경고 기능 미구현 — WBP 기본값 "Text Block" 노출 방지
		TextBlock_WarnPlayers->SetText(FText::GetEmpty());
		TextBlock_WarnPlayers->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Image_Map)
	{
		if (MapTexture)
		{
			// 위젯에 텍스처를 브러시로 설정
			Image_Map->SetBrushFromTexture(MapTexture);
			// 위젯 자체를 표시
			Image_Map->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			// 텍스처가 할당되지 않았다면 위젯 숨김
			Image_Map->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (Btn_Menu)
	{
		Btn_Menu->OnClicked.AddUniqueDynamic(this, &US_InGame::HandleMenuClicked);
	}


	// Setup initial placeholder value
	if (UWorld* World = GetWorld())
	{
		if (ATeamCarryGameState* GS = World->GetGameState<ATeamCarryGameState>())
		{
			// 팀 값어치 게이지의 Max 값은 스테이지 중 불변이므로 여기서 1회만 캐시한다.
			CachedTotalLevelValue = GS->TotalLevelValue;
			// Txt_FurnitureCount 분모(전체 상자 개수, 파괴된 것 포함)도 스테이지 중 불변이므로 1회만 캐시한다.
			CachedTotalFurnitureCount = GS->TotalFurnitureCount;

			// 게임이 시작될 때 GameState에 이미 들어있는 돈과 가구 수를 HUD에 즉시 반영합니다.
			HandleTeamMoneyUpdated(GS->TotalScore);
			HandleRemainingFurnitureUpdated(GS->RemainingFurniture);
		}
	}

	// Subscribe to MockUIController delegates
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->OnTeamMoneyUpdated.AddUniqueDynamic(this, &US_InGame::HandleTeamMoneyUpdated);
		//MockController->OnFurnitureSettled.AddUniqueDynamic(this, &US_InGame::HandleFurnitureSettled);
		MockController->OnInteractTargetChanged.AddUniqueDynamic(this, &US_InGame::HandleInteractTargetChanged);
		//MockController->OnDurabilityChanged.AddUniqueDynamic(this, &US_InGame::HandleDurabilityChanged);
		MockController->OnRemainingFurnitureUpdated.AddUniqueDynamic(this, &US_InGame::HandleRemainingFurnitureUpdated);

		UE_LOG(LogTemp, Log, TEXT("[UI InGameHUD] Successfully bound to MockUIController delegates."));
	}
}

void US_InGame::NativeDestruct()
{
	// Unsubscribe from MockUIController delegates
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		MockController->OnTeamMoneyUpdated.RemoveAll(this);
		//MockController->OnFurnitureSettled.RemoveAll(this);
		MockController->OnInteractTargetChanged.RemoveAll(this);
		//MockController->OnDurabilityChanged.RemoveAll(this);
		MockController->OnRemainingFurnitureUpdated.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void US_InGame::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// ElapsedTime은 매 프레임 서버에서 갱신되는 값이라 델리게이트가 아닌 Tick 폴링으로 동기화한다.
	// (리슨 서버 호스트는 같은 GameState 인스턴스를 즉시 읽고, 클라이언트는 복제된 최신값을 읽는다.)
	if (UWorld* World = GetWorld())
	{
		if (ATeamCarryGameState* GS = World->GetGameState<ATeamCarryGameState>())
		{
			UpdateTimerDisplay(GS->RemainingTime);
		}
	}
}

TOptional<FUIInputConfig> US_InGame::GetDesiredInputConfig() const
{
	// TOptional 객체로 감싸서 반환합니다.
	return TOptional<FUIInputConfig>(FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently, true));
}

void US_InGame::HandleTeamMoneyUpdated(int32 NewTotalMoney)
{
	UE_LOG(LogTemp, Log, TEXT("[UI InGameHUD] HUD Received Team Money Update: $%d / $%d"), NewTotalMoney, CachedTotalLevelValue);

	if (PB_TeamMoney)
	{
		const float Percent = CachedTotalLevelValue > 0 ? static_cast<float>(NewTotalMoney) / static_cast<float>(CachedTotalLevelValue) : 0.0f;
		PB_TeamMoney->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
	}
	if (TextBlock_Score)
	{
		TextBlock_Score->SetText(FText::Format(NSLOCTEXT("InGameUI", "MoneyGaugeFormat", "{0} / {1}"),
			FText::AsNumber(NewTotalMoney), FText::AsNumber(CachedTotalLevelValue)));
	}
}

void US_InGame::UpdateTimerDisplay(float ElapsedTime)
{
	const int32 TotalSeconds = FMath::Max(0, FMath::FloorToInt(ElapsedTime));
	if (TotalSeconds == LastDisplayedSeconds)
	{
		return;
	}
	LastDisplayedSeconds = TotalSeconds;

	const int32 Minutes = TotalSeconds / 60;
	const int32 Seconds = TotalSeconds % 60;

	if (TextBlock_Timer)
	{
		TextBlock_Timer->SetText(FText::FromString(FString::Printf(TEXT("%02d : %02d"), Minutes, Seconds)));
	}
}

void US_InGame::HandleRemainingFurnitureUpdated(int32 NewCount)
{
	// 분모(전체 상자 개수)도 파괴된 것은 제외하고 동적으로 계산한다(v3 내부 개정, 명세 4장-7).
	// DestroyedFurnitureCount는 RemainingFurniture와 같은 호출(OnFurnitureDestroyed) 안에서 함께
	// 갱신되므로, 이 델리게이트가 발화하는 시점에 GameState에서 직접 최신값을 읽으면 항상 일치한다.
	int32 DenominatorCount = CachedTotalFurnitureCount;
	if (const UWorld* World = GetWorld())
	{
		if (const ATeamCarryGameState* GS = World->GetGameState<ATeamCarryGameState>())
		{
			DenominatorCount = CachedTotalFurnitureCount - GS->DestroyedFurnitureCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[UI InGameHUD] HUD Received Remaining Furniture Update: %d / %d"), NewCount, DenominatorCount);
	if (Txt_FurnitureCount)
	{
		// "이동 가능한 개수 / 상자 개수(파괴된 것 제외)" 분수 표기(UI_Technical_Spec.md 4장-7).
		Txt_FurnitureCount->SetText(FText::Format(NSLOCTEXT("InGameUI", "FurnitureCountFormat", "{0} / {1}"),
			FText::AsNumber(NewCount), FText::AsNumber(DenominatorCount)));
	}
}

//void US_InGame::HandleFurnitureSettled(int32 AddedMoney, int32 Grade)
//{
//	UE_LOG(LogTemp, Warning, TEXT("[UI InGameHUD] HUD Received Furniture Settled Event: +$%d, Grade: %d"), AddedMoney, Grade);
//	
//	// Temporarily display a settle notification on screen for visual feedback
//	if (TextBlock_WarnPlayers)
//	{
//		FText SettleText = FText::Format(NSLOCTEXT("InGameUI", "FurnitureSettleFormat", "+${0} (Perfect!)"), FText::AsNumber(AddedMoney));
//		TextBlock_WarnPlayers->SetText(SettleText);
//		TextBlock_WarnPlayers->SetVisibility(ESlateVisibility::Visible);
//	}
//}

void US_InGame::HandleInteractTargetChanged(AActor* Target, FString Key)
{
	UE_LOG(LogTemp, Log, TEXT("[UI InGameHUD] HUD Received Interact Target Change: %s"), *Key);
	if (TextBlock_InteractPrompt)
	{
		if (Key.IsEmpty())
		{
			TextBlock_InteractPrompt->SetText(FText::GetEmpty());
		}
		else
		{
			TextBlock_InteractPrompt->SetText(FText::FromString(Key));
		}
	}
}

//void US_InGame::HandleDurabilityChanged(float Current, float Max)
//{
//	if (ProgressBar_Durability)
//	{
//		float Percent = Max > 0.0f ? (Current / Max) : 0.0f;
//		ProgressBar_Durability->SetPercent(Percent);
//		
//		// If durability is low, show low durability warning
//		if (TextBlock_WarnPlayers && Percent < 0.3f && Percent > 0.0f)
//		{
//			TextBlock_WarnPlayers->SetText(NSLOCTEXT("InGameUI", "LowDurabilityWarning", "주의: 가구 부서짐 위험!"));
//			TextBlock_WarnPlayers->SetVisibility(ESlateVisibility::Visible);
//		}
//		else if (TextBlock_WarnPlayers && (Percent >= 0.3f || Percent == 0.0f))
//		{
//			TextBlock_WarnPlayers->SetVisibility(ESlateVisibility::Collapsed);
//		}
//	}
//}

void US_InGame::HandleMenuClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI InGame] Open Main Menu."));
	}
}