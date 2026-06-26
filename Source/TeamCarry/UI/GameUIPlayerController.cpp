// Fill out your copyright notice in the Description page of Project Settings.

#include "TeamCarry/UI/GameUIPlayerController.h"

#include "CommonActivatableWidget.h"
#include "TeamCarry/UI/W_RootLayout.h"
#include "TeamCarry/UI/MockUIController.h"

UMockUIController* AGameUIPlayerController::GetRouter() const
{
	// PC -> Subsystem 은 항상 안전: GameInstance 수명 == Subsystem 수명.
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockUIController>() : nullptr;
}

void AGameUIPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// UI 생성은 로컬 컨트롤러만 수행(서버의 원격 PC 가 위젯을 만들지 않도록 차단).
	if (!IsLocalController())
	{
		return;
	}

	// 1) 루트 레이아웃 생성 + 뷰포트 등록 (PC 가 UI 생성 권한 소유).
	if (RootLayoutClass)
	{
		RootLayout = CreateWidget<UW_RootLayout>(this, RootLayoutClass);
		if (RootLayout)
		{
			RootLayout->AddToViewport(0);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameUIPC] RootLayoutClass is not set. Assign WBP_RootLayout in the controller defaults."));
	}

	// 2) 커서/입력 모드. 세부 입력 컨텍스트는 각 ActivatableWidget 의 GetDesiredInputConfig 가 갱신한다.
	bShowMouseCursor = true;
	SetInputMode(FInputModeGameAndUI());

	// 3) 호스트 등록 후, 초기 화면은 반드시 라우터 경유로 진입(명세 5-1 단일 경로).
	if (UMockUIController* Router = GetRouter())
	{
		Router->RegisterUIHost(this);
		Router->ReplaceState(InitialState);
	}
}

void AGameUIPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMockUIController* Router = GetRouter())
	{
		Router->UnregisterUIHost(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AGameUIPlayerController::ShowState(EE_UIState NewState)
{
	if (!RootLayout)
	{
		return;
	}

	const TSubclassOf<UCommonActivatableWidget>* Found = ScreenWidgetClasses.Find(NewState);
	if (!Found || !*Found)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameUIPC] No screen widget class mapped for state %d."), (int32)NewState);
		return;
	}

	RootLayout->ShowScreen(*Found);
}

UCommonActivatableWidget* AGameUIPlayerController::ShowOverlay(FName OverlayId)
{
	if (!RootLayout)
	{
		return nullptr;
	}

	const TSubclassOf<UCommonActivatableWidget>* Found = OverlayWidgetClasses.Find(OverlayId);
	if (!Found || !*Found)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameUIPC] No overlay widget class mapped for id '%s'."), *OverlayId.ToString());
		return nullptr;   // 라우터가 이 nullptr 을 보고 스택을 롤백한다.
	}

	return RootLayout->PushOverlay(*Found);
}

void AGameUIPlayerController::HideTopOverlay()
{
	if (RootLayout)
	{
		RootLayout->PopOverlay();
	}
}
