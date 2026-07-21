// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/S_Tutorial.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Network/Session/TCSessionFlow.h"

void US_Tutorial::NativeConstruct()
{
	Super::NativeConstruct();

	// 건너뛰기 버튼 바인딩.
	if (Btn_Skip)
	{
		Btn_Skip->OnClicked.AddUniqueDynamic(this, &US_Tutorial::HandleSkipClicked);
	}

	// 단계 안내 텍스트는 게임플레이 진행 델리게이트 연동 시 갱신된다.
	// (프로토타입에서는 바인딩 유효성만 로깅한다.)
	if (!Txt_StepDescription)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI Tutorial] Txt_StepDescription is not bound. Check the WBP hierarchy."));
	}
}

UWidget* US_Tutorial::NativeGetDesiredFocusTarget() const
{
	return Super::NativeGetDesiredFocusTarget();
}

TOptional<FUIInputConfig> US_Tutorial::GetDesiredInputConfig() const
{
	// TOptional 객체로 감싸서 반환합니다.
	return TOptional<FUIInputConfig>(FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently, true));
}

void US_Tutorial::HandleSkipClicked()
{
	// 명세 4장-6: 마지막 Step 완료/건너뛰기 모두 동일하게 S_Lobby 로 복귀한다(L_StageSelect 직행 폐기).
	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI Tutorial] Skip clicked. Completing tutorial and returning to lobby."));
		Flow->CompleteTutorial();
	}
}
