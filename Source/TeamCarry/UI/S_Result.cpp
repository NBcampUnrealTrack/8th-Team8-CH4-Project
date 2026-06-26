// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/S_Result.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "TeamCarry/UI/MockUIController.h"

void US_Result::NativeConstruct()
{
	Super::NativeConstruct();

	// 확인 버튼 바인딩(스테이지 선택 복귀).
	if (Btn_Confirm)
	{
		Btn_Confirm->OnClicked.AddUniqueDynamic(this, &US_Result::HandleConfirmClicked);
	}

	// 타이틀 복귀 버튼 바인딩(보조 경로).
	if (Btn_ToTitle)
	{
		Btn_ToTitle->OnClicked.AddUniqueDynamic(this, &US_Result::HandleToTitleClicked);
	}

	// 점수/통계 텍스트는 정산 결과 연동 시 채워진다.
	// (프로토타입에서는 바인딩 유효성만 로깅한다.)
	if (!Txt_Score || !Txt_Stats)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI Result] Score/Stats TextBlock is not bound. Check the WBP hierarchy."));
	}
}

UWidget* US_Result::NativeGetDesiredFocusTarget() const
{
	if (Btn_Confirm)
	{
		return Btn_Confirm;
	}

	return Super::NativeGetDesiredFocusTarget();
}

void US_Result::HandleConfirmClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// 명세 3-7 / 1 흐름: 확인 → S_StageSelect 로 복귀(다음 스테이지 선택).
		UE_LOG(LogTemp, Log, TEXT("[UI Result] Confirm clicked. Replacing to StageSelect."));
		MockController->ReplaceState(EE_UIState::StageSelect);
	}
}

void US_Result::HandleToTitleClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// 보조 경로: 타이틀(S_MainMenu)로 복귀.
		UE_LOG(LogTemp, Log, TEXT("[UI Result] To Title clicked. Replacing to MainMenu."));
		MockController->ReplaceState(EE_UIState::MainMenu);
	}
}
