// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/MockUIController.h"
#include "TeamCarry/UI/UIHost.h"
#include "Network/Session/TCSessionFlow.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

UMockUIController::UMockUIController()
	: CurrentState(EE_UIState::None)
{
	// 지속형 로딩 위젯 클래스(트래블 구간 전용, HandleTravelStarted 참고).
	static ConstructorHelpers::FClassFinder<UUserWidget> LoadingWidgetFinder(
		TEXT("/Game/Developers/MinkiCho/Blueprint/UI/WBP_S_Loading"));
	if (LoadingWidgetFinder.Succeeded())
	{
		LoadingWidgetClass = LoadingWidgetFinder.Class;
	}
}

void UMockUIController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("UMockUIController Initialized."));

	// 레벨 트래블 시작 통지 구독(명세 4장-9) — S_Loading 표시 트리거.
	if (UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr)
	{
		Flow->OnTravelStarted.AddUniqueDynamic(this, &UMockUIController::HandleTravelStarted);
	}
}

void UMockUIController::Deinitialize()
{
	if (UTCSessionFlow* Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTCSessionFlow>() : nullptr)
	{
		Flow->OnTravelStarted.RemoveDynamic(this, &UMockUIController::HandleTravelStarted);
	}

	Super::Deinitialize();
}

void UMockUIController::HandleTravelStarted(const FString& TargetMapPath)
{
	UE_LOG(LogTemp, Log, TEXT("[UI Router] Travel started -> %s. Showing S_Loading."), *TargetMapPath);
	ShowLoadingScreenNow();
}

void UMockUIController::ShowLoadingScreenNow()
{
	ShowPersistentLoadingWidget();
	ReplaceState(EE_UIState::Loading);
}

void UMockUIController::ShowPersistentLoadingWidget()
{
	if (PersistentLoadingWidget && PersistentLoadingWidget->IsInViewport())
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI || !LoadingWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI Router] LoadingWidgetClass 없음 — 지속형 로딩 위젯을 띄울 수 없음"));
		return;
	}

	// 트래블마다 항상 새로 생성한다(기존 인스턴스를 재사용하지 않는다). CreateWidget()은
	// 생성 시점의 World를 위젯 내부(PlayerContext/CachedWorld)에 스냅샷으로 캡처해두는데,
	// 인스턴스를 재사용하면 이후 트래블에서 그 캐시된(이미 GC 대상이 된) World가 그대로 남아
	// AddToScreen() 내부의 GetGameViewport()가 null이 되면서 AddToViewport()가 예외 없이
	// 조용히 실패하는 문제가 있었다(로딩 게이지 화면이 몇 번의 트래블 뒤부터 아예 안 뜨던 버그).
	// GameInstance 소유로 매번 새로 만들어도(PlayerController 는 트래블마다 파괴/재생성되지만
	// GameInstance는 프로세스 내내 유지되므로) 구 PC 파괴~신규 PC BeginPlay 사이의 공백 구간은
	// 여전히 메워준다.
	PersistentLoadingWidget = CreateWidget<UUserWidget>(GI, LoadingWidgetClass);

	if (PersistentLoadingWidget)
	{
		PersistentLoadingWidget->AddToViewport(1000);
	}
}

void UMockUIController::HidePersistentLoadingWidget()
{
	if (PersistentLoadingWidget && PersistentLoadingWidget->IsInViewport())
	{
		PersistentLoadingWidget->RemoveFromParent();
	}
}

void UMockUIController::ReplaceState(EE_UIState NewState)
{
	// 1. 월드가 유효한지, 파괴 중이 아닌지 확인하는 체크 추가
	UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld || CurrentWorld->bIsTearingDown)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI Router] 월드가 파괴 중입니다. 화면 전환 요청을 무시합니다: %d"), (int32)NewState);
		return;
	}

	if (CurrentState == NewState)
	{
		return;
	}

	// 목적지 State 로 실제 전환되는 시점(= 신규 PC 가 자기 화면을 띄우는 시점)에 지속형
	// 로딩 위젯을 내린다. Loading 으로 들어가는 경우는 유지(ShowPersistentLoadingWidget 이 올림).
	if (NewState != EE_UIState::Loading)
	{
		HidePersistentLoadingWidget();
	}

	EE_UIState OldState = CurrentState;
	PreviousState = OldState;
	CurrentState = NewState;

	UE_LOG(LogTemp, Warning, TEXT("[UI State Machine] State Changed: %d -> %d"), (int32)OldState, (int32)NewState);

	// 풀스크린 화면이 통째로 바뀌므로, 이전 화면 위에 쌓여 있던 오버레이(O_PauseMenu, O_Confirm 등)를
	// 먼저 정리한다. 그렇지 않으면 남은 모달이 새 화면(특히 S_Loading)을 계속 가리게 되고,
	// 스택 잔여 항목이 레벨 트래블을 넘어 다음 화면까지 새어나가 Push/Pop 이 어긋난다.
	if (!MockOverlayStack.IsEmpty())
	{
		MockOverlayStack.Empty();
	}

	// 실제 풀스크린 화면 교체를 호스트(PC)에 위임한다.
	// (Replace 는 단순 교체이므로 PushOverlay 와 달리 롤백이 필요 없다.)
	if (IUIHost* Host = GetUIHost())
	{
		Host->ClearAllOverlays();
		Host->ShowState(NewState);
	}

	// Trigger State Changed Delegate
	OnStateChanged.Broadcast(NewState);
}

UCommonActivatableWidget* UMockUIController::PushOverlay(const FString& OverlayName)
{
	// FPlatformTime::Seconds() 는 프로세스 기준 벽시계 시간이라 레벨 트래블로 월드가
	// 바뀌어도 계속 증가한다. World->GetRealTimeSeconds() 를 쓰면 새 레벨에서 0부터
	// 다시 시작해 LastMenuToggleTime(이전 월드의 누적 시간)보다 작아지고, 그 차이가
	// 영원히 음수가 되어 쿨타임 체크가 항상 실패(= 모든 Push 가 막힘)하는 문제가 있었다.
	const double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastMenuToggleTime < MenuToggleCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI Stack] 연타 방지: 쿨타임 중 Push 무시 (%s)"), *OverlayName);
		return nullptr;
	}
	LastMenuToggleTime = CurrentTime; // 마지막 실행 시간 갱신

	IUIHost* Host = GetUIHost();
	if (!Host)
	{
		// 띄울 수 없으면 상태도 누적하지 않는다(스택 오염 방지).
		UE_LOG(LogTemp, Warning, TEXT("[UI Stack] No UI Host registered. Push aborted: %s"), *OverlayName);
		return nullptr; // void가 아니므로 nullptr을 반환해야 합니다.
	}

	//상태 먼저 갱신: 호스트의 ShowOverlay 내부가 일관된 스택/상태를 보도록 한다.
	MockOverlayStack.Push(OverlayName);

	//실제 생성·뷰포트 추가는 호스트(PC)가 수행.
	UCommonActivatableWidget* Created = Host->ShowOverlay(FName(*OverlayName));

	//생성 실패 시 롤백: 상태 머신과 실제 화면의 불일치를 막는다(원자성 보장).
	if (!Created)
	{
		MockOverlayStack.Pop();
		UE_LOG(LogTemp, Error, TEXT("[UI Stack] ShowOverlay failed, rolled back: %s"), *OverlayName);
		return nullptr; // void가 아니므로 nullptr을 반환해야 합니다.
	}

	UE_LOG(LogTemp, Log, TEXT("[UI Stack] Push Overlay: %s. Current Stack Size: %d"), *OverlayName, MockOverlayStack.Num());

	//성공적으w로 생성된 위젯의 포인터를 최종 반환합니다.
	return Created;
}

void UMockUIController::PopCurrentOverlay()
{
	const double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastMenuToggleTime < MenuToggleCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI Stack] 연타 방지: 쿨타임 중 Pop 무시"));
		return;
	}
	LastMenuToggleTime = CurrentTime; // 마지막 실행 시간 갱신

	if (MockOverlayStack.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI Stack] Attempted to Pop but the Overlay Stack is EMPTY!"));
		return;
	}

	if (IUIHost* Host = GetUIHost())
	{
		Host->HideTopOverlay();
	}

	FString Popped = MockOverlayStack.Pop();
	UE_LOG(LogTemp, Log, TEXT("[UI Stack] Pop Overlay: %s. Current Stack Size: %d"), *Popped, MockOverlayStack.Num());

	// 마지막 오버레이가 닫혀 게임 화면(캐릭터 조작)으로 돌아가는 시점.
	// CommonUI 라우터의 "leaf-most 위젯에 포커스 대상이 없으면 게임 뷰포트로 포커스" last-resort
	// 처리가, 오버레이를 2단 이상 중첩해서 열고 닫으면(O_PauseMenu 위에서 O_Settings 등을 열었다
	// 닫는 경우) 신뢰할 수 없어지는 것이 재현된다(정확한 근본 원인은 CommonUI/Slate 내부로 추정).
	// CurrentState 는 이 경로에서 바뀌지 않으므로 OnStateChanged 로는 감지할 수 없다 — 여기서
	// 직접, PC(BeginPlay 에서 이미 검증된 SetInputMode 경로)로 입력을 확실히 복구한다.
	// 위젯 제거가 실제로 끝난 다음 프레임까지 미뤄서 재시도한다(같은 프레임 경합 회피).
	if (MockOverlayStack.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<UObject> HostObjectWeak = UIHostObject;
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([HostObjectWeak]()
				{
					if (FSlateApplication::IsInitialized())
					{
						FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
						FSlateApplication::Get().SetAllUserFocusToGameViewport();
					}
					if (APlayerController* PC = Cast<APlayerController>(HostObjectWeak.Get()))
					{
						PC->bShowMouseCursor = true;
						PC->SetInputMode(FInputModeGameAndUI());
					}
				}));
		}
	}
}

void UMockUIController::RegisterUIHost(const TScriptInterface<IUIHost>& InHost)
{
	UIHostObject = InHost.GetObject();
	UE_LOG(LogTemp, Log, TEXT("[UI Router] UI Host registered: %s"), *GetNameSafe(UIHostObject.Get()));
}

void UMockUIController::UnregisterUIHost(const TScriptInterface<IUIHost>& InHost)
{
	//다른 PC 가 이미 호스트를 교체했을 수 있으니, 본인이 등록한 경우에만 해제한다.
	if (UIHostObject.Get() == InHost.GetObject())
	{
		UIHostObject = nullptr;
		UE_LOG(LogTemp, Log, TEXT("[UI Router] UI Host unregistered."));
	}
}

IUIHost* UMockUIController::GetUIHost() const
{
	//Get() 이 null(파괴됨)이면 Cast 도 안전하게 null 을 반환한다.
	return Cast<IUIHost>(UIHostObject.Get());
}

void UMockUIController::SetLobbySlotReady(int32 SlotIndex, bool bIsReady)
{
	FPlayerInfo FakeInfo;
	FakeInfo.PlayerName = FString::Printf(TEXT("Player_%d"), SlotIndex + 1);
	FakeInfo.bIsReady = bIsReady;
	FakeInfo.CharacterIndex = SlotIndex; // Simple index mapping

	UE_LOG(LogTemp, Log, TEXT("[UI Lobby] Slot %d set Ready to: %s"), SlotIndex, bIsReady ? TEXT("True") : TEXT("False"));
	
	// Broadcast Slot Update Delegate
	OnLobbySlotUpdated.Broadcast(SlotIndex, FakeInfo);
}

void UMockUIController::UpdateTeamMoney(int32 NewMoney)
{
	UE_LOG(LogTemp, Log, TEXT("[UI GameData] Team Money Updated: %d"), NewMoney);
	OnTeamMoneyUpdated.Broadcast(NewMoney);
}

void UMockUIController::UpdateRemainingFurniture(int32 NewCount)
{
	UE_LOG(LogTemp, Log, TEXT("[UI GameData] Remaining Furniture Updated: %d"), NewCount);
	OnRemainingFurnitureUpdated.Broadcast(NewCount);
}

void UMockUIController::TriggerGameResult(int32 FinalScore, int32 StarCount, float ElapsedTime)
{
	// S_Result가 생성되기 전에 값을 먼저 캐시해 둔다.
	// ReplaceState()는 위젯을 동기적으로 생성하므로, 이 값들은 S_Result::NativeConstruct에서 바로 읽을 수 있다.
	LastFinalScore = FinalScore;
	LastStarCount = StarCount;
	LastElapsedTime = ElapsedTime;

	// 로그와 방송에 시간 포함
	UE_LOG(LogTemp, Log, TEXT("[UI GameData] Game Result Ready: Score=%d, Star=%d, Time=%.1fs"), FinalScore, StarCount, ElapsedTime);
	OnGameResultReady.Broadcast(FinalScore, StarCount, ElapsedTime); // [수정됨]

	// O_Result 는 인게임 레벨 위 오버레이다(명세 4장-8) — S_InGame HUD 는 아래에 남은 채 가려진다.
	PushOverlay(TEXT("O_Result"));
}
