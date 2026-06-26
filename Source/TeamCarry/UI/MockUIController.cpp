// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/MockUIController.h"
#include "TeamCarry/UI/UIHost.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UMockUIController::UMockUIController()
	: CurrentState(EE_UIState::None)
	, SimulationTicks(0)
	, SimulatedMoney(0)
	, SimulatedDurability(100.0f)
{
}

void UMockUIController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("UMockUIController Initialized."));
}

void UMockUIController::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SimulationTimerHandle);
	}
	Super::Deinitialize();
}

void UMockUIController::ReplaceState(EE_UIState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	EE_UIState OldState = CurrentState;
	CurrentState = NewState;

	UE_LOG(LogTemp, Warning, TEXT("[UI State Machine] State Changed: %d -> %d"), (int32)OldState, (int32)NewState);

	// Stop simulation timer if we are leaving S_InGame
	if (OldState == EE_UIState::InGame && GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SimulationTimerHandle);
		UE_LOG(LogTemp, Log, TEXT("[UI Simulation] InGame Simulation Timer Cleared."));
	}

	// 실제 풀스크린 화면 교체를 호스트(PC)에 위임한다.
	// (Replace 는 단순 교체이므로 PushOverlay 와 달리 롤백이 필요 없다.)
	if (IUIHost* Host = GetUIHost())
	{
		Host->ShowState(NewState);
	}

	// Trigger State Changed Delegate
	OnStateChanged.Broadcast(NewState);

	// Start simulation timer if we are entering S_InGame
	if (NewState == EE_UIState::InGame)
	{
		SimulationTicks = 0;
		SimulatedMoney = 100; // Initial money
		SimulatedDurability = 100.0f;

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				SimulationTimerHandle,
				this,
				&UMockUIController::UpdateInGameMockSimulation,
				1.0f,
				true
			);
			UE_LOG(LogTemp, Warning, TEXT("[UI Simulation] InGame Simulation Started. A 10-second timer is active."));
			
			// Broadcast initial values immediately
			OnTeamMoneyUpdated.Broadcast(SimulatedMoney);
			OnDurabilityChanged.Broadcast(SimulatedDurability, 100.0f);
		}
	}
}

void UMockUIController::PushOverlay(const FString& OverlayName)
{
	IUIHost* Host = GetUIHost();
	if (!Host)
	{
		// 띄울 수 없으면 상태도 누적하지 않는다(스택 오염 방지).
		UE_LOG(LogTemp, Warning, TEXT("[UI Stack] No UI Host registered. Push aborted: %s"), *OverlayName);
		return;
	}

	// ① 상태 먼저 갱신: 호스트의 ShowOverlay 내부가 일관된 스택/상태를 보도록 한다.
	MockOverlayStack.Push(OverlayName);

	// ② 실제 생성·뷰포트 추가는 호스트(PC)가 수행.
	UCommonActivatableWidget* Created = Host->ShowOverlay(FName(*OverlayName));

	// ③ 생성 실패 시 롤백: 상태 머신과 실제 화면의 불일치를 막는다(원자성 보장).
	if (!Created)
	{
		MockOverlayStack.Pop();
		UE_LOG(LogTemp, Error, TEXT("[UI Stack] ShowOverlay failed, rolled back: %s"), *OverlayName);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[UI Stack] Push Overlay: %s. Current Stack Size: %d"), *OverlayName, MockOverlayStack.Num());
}

void UMockUIController::PopCurrentOverlay()
{
	if (MockOverlayStack.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI Stack] Attempted to Pop but the Overlay Stack is EMPTY!"));
		return;
	}

	// Pop 은 역순: ① 실제 위젯 제거를 호스트에 위임 → ② 상태에서 제거.
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
	// 다른 PC 가 이미 호스트를 교체했을 수 있으니, 본인이 등록한 경우에만 해제한다.
	if (UIHostObject.Get() == InHost.GetObject())
	{
		UIHostObject = nullptr;
		UE_LOG(LogTemp, Log, TEXT("[UI Router] UI Host unregistered."));
	}
}

IUIHost* UMockUIController::GetUIHost() const
{
	// Get() 이 null(파괴됨)이면 Cast 도 안전하게 null 을 반환한다.
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

void UMockUIController::UpdateInGameMockSimulation()
{
	SimulationTicks++;

	if (SimulationTicks >= 10)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI Simulation] 10 seconds reached! Simulating stage completion. Transitioning to S_Result."));
		ReplaceState(EE_UIState::Result);
		return;
	}

	// 1. Durability decrements continuously by 10% each second
	SimulatedDurability -= 10.0f;
	if (SimulatedDurability < 0.0f)
	{
		SimulatedDurability = 100.0f; // reset durability for demo
	}
	OnDurabilityChanged.Broadcast(SimulatedDurability, 100.0f);

	// 2. Increment team money at certain times
	if (SimulationTicks % 2 == 0)
	{
		SimulatedMoney += 250;
		OnTeamMoneyUpdated.Broadcast(SimulatedMoney);
	}

	// 3. Emit OnFurnitureSettled event halfway (at 5 seconds)
	if (SimulationTicks == 5)
	{
		int32 AddedMoney = 500;
		int32 Grade = 1; // PERFECT
		OnFurnitureSettled.Broadcast(AddedMoney, Grade);
		UE_LOG(LogTemp, Log, TEXT("[UI Simulation] Simulating Furniture Settle: Added $%d, Grade: %d"), AddedMoney, Grade);
	}

	// 4. Emit OnInteractTargetChanged event at 3 seconds and clear at 7 seconds
	if (SimulationTicks == 3)
	{
		OnInteractTargetChanged.Broadcast(nullptr, TEXT("E: 가구 들기"));
		UE_LOG(LogTemp, Log, TEXT("[UI Simulation] Simulating Crosshair Target Found: 'E: 가구 들기'"));
	}
	else if (SimulationTicks == 7)
	{
		OnInteractTargetChanged.Broadcast(nullptr, TEXT(""));
		UE_LOG(LogTemp, Log, TEXT("[UI Simulation] Simulating Crosshair Target Cleared"));
	}
}
