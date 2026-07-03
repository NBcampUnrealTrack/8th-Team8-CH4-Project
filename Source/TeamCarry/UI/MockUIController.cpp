// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/MockUIController.h"
#include "TeamCarry/UI/UIHost.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UMockUIController::UMockUIController()
	: CurrentState(EE_UIState::None)
{
}

void UMockUIController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("UMockUIController Initialized."));
}

void UMockUIController::Deinitialize()
{
	Super::Deinitialize();
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

	EE_UIState OldState = CurrentState;
	PreviousState = OldState;
	CurrentState = NewState;

	UE_LOG(LogTemp, Warning, TEXT("[UI State Machine] State Changed: %d -> %d"), (int32)OldState, (int32)NewState);

	// 실제 풀스크린 화면 교체를 호스트(PC)에 위임한다.
	// (Replace 는 단순 교체이므로 PushOverlay 와 달리 롤백이 필요 없다.)
	if (IUIHost* Host = GetUIHost())
	{
		Host->ShowState(NewState);
	}

	// Trigger State Changed Delegate
	OnStateChanged.Broadcast(NewState);
}

UCommonActivatableWidget* UMockUIController::PushOverlay(const FString& OverlayName)
{
	if (UWorld* World = GetWorld())
	{
		float CurrentTime = World->GetRealTimeSeconds(); // 일시정지에 영향받지 않는 현실 시간
		if (CurrentTime - LastMenuToggleTime < MenuToggleCooldown)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UI Stack] 연타 방지: 쿨타임 중 Push 무시 (%s)"), *OverlayName);
			return nullptr;
		}
		LastMenuToggleTime = CurrentTime; // 마지막 실행 시간 갱신
	}

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

	//성공적으로 생성된 위젯의 포인터를 최종 반환합니다.
	return Created;
}

void UMockUIController::PopCurrentOverlay()
{
	if (UWorld* World = GetWorld())
	{
		float CurrentTime = World->GetRealTimeSeconds(); // 일시정지에 영향받지 않는 현실 시간
		if (CurrentTime - LastMenuToggleTime < MenuToggleCooldown)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UI Stack] 연타 방지: 쿨타임 중 Pop 무시"));
			return;
		}
		LastMenuToggleTime = CurrentTime; // 마지막 실행 시간 갱신
	}

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

	ReplaceState(EE_UIState::Result);
}
