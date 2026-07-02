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