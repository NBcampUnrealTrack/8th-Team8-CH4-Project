// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamCarry/UI/MockUIController.h"
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
	MockOverlayStack.Push(OverlayName);
	UE_LOG(LogTemp, Log, TEXT("[UI Stack] Push Overlay: %s. Current Stack Size: %d"), *OverlayName, MockOverlayStack.Num());
}

void UMockUIController::PopCurrentOverlay()
{
	if (MockOverlayStack.Num() > 0)
	{
		FString Popped = MockOverlayStack.Pop();
		UE_LOG(LogTemp, Log, TEXT("[UI Stack] Pop Overlay: %s. Current Stack Size: %d"), *Popped, MockOverlayStack.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI Stack] Attempted to Pop but the Overlay Stack is EMPTY!"));
	}
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
