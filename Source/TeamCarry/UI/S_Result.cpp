// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/S_Result.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "TeamCarry/UI/MockUIController.h"
#include "Network/Session/TCSessionFlow.h"
#include "Engine/World.h"
// UGameplayStatics.h 인클루드는 더 이상 UI에서 필요하지 않으므로 삭제되었습니다.

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

	// 점수/통계/별 개수 텍스트는 정산 결과 연동 시 채워진다.
	// WBP에서 아직 바인딩되지 않은 위젯이 있어도(Optional) 크래시 없이 로깅만 하고 넘어간다.
	if (!Txt_Score || !Txt_ElapsedTime || !Txt_StarCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI Result] Score/Stats/StarCount TextBlock is not bound. Check the WBP hierarchy."));
	}

	// S_Result는 UMockUIController::TriggerGameResult()에 의해 ReplaceState()로 생성되며,
	// 이 시점에는 이미 최종 점수/별 개수가 컨트롤러에 캐시되어 있으므로 즉시 읽어서 채운다.
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		const int32 FinalScore = MockController->GetLastFinalScore();
		const int32 StarCount = MockController->GetLastStarCount();
		const float ElapsedTime = MockController->GetLastElapsedTime();

		if (Txt_Score)
		{
			Txt_Score->SetText(FText::Format(NSLOCTEXT("ResultUI", "ScoreFormat", "${0}"), FText::AsNumber(FinalScore)));
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

			FString TimeString = FString::Printf(TEXT("클리어 시간: %02d 분 %02d 초"), Minutes, Seconds);
			Txt_ElapsedTime->SetText(FText::FromString(TimeString));
		}

		UE_LOG(LogTemp, Log, TEXT("[UI Result] Result Display Updated: Score=%d, Star=%d, Time=%.1fs"), FinalScore, StarCount, ElapsedTime);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI Result] MockUIController subsystem not found. Score/StarCount display left unset."));
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

// 확인 버튼 클릭 시 스테이지 선택 레벨로 이동
void US_Result::HandleConfirmClicked()
{
	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI Result] Confirm clicked. Requesting Stage Select."));

		// 캡슐화가 완벽히 지켜진 스테이지 선택 전용 함수를 호출합니다.
		// (싱글/멀티 판별 및 클라이언트 대기 처리는 Flow 내부에서 알아서 안전하게 진행됩니다.)
		Flow->HostReturnToStageSelect();
	}
}

// 타이틀 복귀 버튼 클릭 시 타이틀 레벨로 이동
void US_Result::HandleToTitleClicked()
{
	if (UTCSessionFlow* Flow = GetGameInstance()->GetSubsystem<UTCSessionFlow>())
	{
		UE_LOG(LogTemp, Log, TEXT("[UI Result] To Title clicked. Requesting Leave To Title."));

		// TCSessionFlow의 LeaveToTitle이 이미 세션 파기 및 타이틀 이동을 모두 처리합니다.
		Flow->LeaveToTitle();
	}
}