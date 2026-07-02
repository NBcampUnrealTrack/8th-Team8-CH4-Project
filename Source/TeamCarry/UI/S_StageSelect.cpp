// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/S_StageSelect.h"
#include "Components/Button.h"
#include "Components/ListView.h"
#include "TeamCarry/UI/MockUIController.h"

void US_StageSelect::NativeConstruct()
{
	Super::NativeConstruct();

	// 진입 버튼 바인딩(선택한 스테이지로 S_InGame 전환).
	if (Btn_Enter)
	{
		Btn_Enter->OnClicked.AddUniqueDynamic(this, &US_StageSelect::HandleEnterClicked);
	}

	// 뒤로 가기 버튼 바인딩(O_Confirm 경유 메인 메뉴 복귀).
	if (Btn_Back)
	{
		Btn_Back->OnClicked.AddUniqueDynamic(this, &US_StageSelect::HandleBackClicked);
	}

	// 리스트 항목 클릭 = 즉시 진입. 네이티브 이벤트로 바인딩한다.
	if (List_Stages)
	{
		List_Stages->OnItemClicked().AddUObject(this, &US_StageSelect::HandleStageItemClicked);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI StageSelect] List_Stages is not bound. Check the WBP hierarchy."));
	}

	// List_Stages 위젯이 정상적으로 바인딩되어 있는지 확인
	if (List_Stages)
	{
		// 임시로 3개의 스테이지 데이터를 생성하여 리스트에 추가합니다.
		for (int32 i = 1; i <= 3; ++i)
		{
			// UObject 기반의 데이터 클래스(예: UStageItemData)를 생성해야 합니다.
			//UStageItemData* NewStageData = NewObject<UStageItemData>(this);

			// 생성한 데이터를 리스트뷰에 추가합니다. 이 순간 화면에 항목이 1개씩 그려집니다.
			//List_Stages->AddItem(NewStageData);
		}
	}
}

void US_StageSelect::NativeDestruct()
{
	// 네이티브 이벤트 구독 해제(중복 바인딩/댕글링 방지).
	if (List_Stages)
	{
		List_Stages->OnItemClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

UWidget* US_StageSelect::NativeGetDesiredFocusTarget() const
{
	if (Btn_Enter)
	{
		return Btn_Enter;
	}

	return Super::NativeGetDesiredFocusTarget();
}

bool US_StageSelect::NativeOnHandleBackAction()
{
	// ESC = 뒤로 가기 버튼과 동일 처리.
	HandleBackClicked();
	return true;
}

void US_StageSelect::HandleEnterClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[UI StageSelect] Enter button clicked."));
	EnterSelectedStage();
}

void US_StageSelect::HandleStageItemClicked(UObject* /*Item*/)
{
	// 리스트에서 스테이지를 직접 클릭해 진입하는 경로.
	UE_LOG(LogTemp, Log, TEXT("[UI StageSelect] Stage item clicked."));
	EnterSelectedStage();
}

void US_StageSelect::EnterSelectedStage()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// 명세 3-5: 맵 선택 후 메모리 세이브 데이터 갱신(저장) → S_InGame 씬 전환.
		// (세이브 갱신은 백엔드 연동 단계에서 이 지점에 추가한다.)
		UE_LOG(LogTemp, Log, TEXT("[UI StageSelect] Entering stage. Replacing to InGame."));
		MockController->ReplaceState(EE_UIState::InGame);
	}
}

void US_StageSelect::HandleBackClicked()
{
	if (UMockUIController* MockController = GetGameInstance()->GetSubsystem<UMockUIController>())
	{
		// 명세 1 흐름: 뒤로 → O_Confirm 모달 호출. 확정 시 메인 메뉴 복귀는 모달이 처리한다.
		UE_LOG(LogTemp, Log, TEXT("[UI StageSelect] Back clicked. Pushing O_Confirm overlay."));
		MockController->PushOverlay(TEXT("O_Confirm"));
	}
}
