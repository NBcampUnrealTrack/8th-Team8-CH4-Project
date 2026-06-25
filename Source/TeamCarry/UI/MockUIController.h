// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MockUIController.generated.h"

// Forward declaration
class AActor;

UENUM(BlueprintType)
enum class EE_UIState : uint8
{
	None,
	MainMenu,
	CharacterSelect,
	Tutorial,
	StageSelect,
	InGame,
	Result,
	// 명세 2: 신규 풀스크린 State. 기존 항목의 enum 값 보존을 위해 끝에 추가한다.
	Boot,       // S_Boot: 앱 실행 시 로고/인트로 연출
	SlotSelect  // S_SlotSelect: 세이브 슬롯 관리(이어하기/새 게임 통합)
};

USTRUCT(BlueprintType)
struct FPlayerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UI|Lobby")
	FString PlayerName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UI|Lobby")
	bool bIsReady = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UI|Lobby")
	int32 CharacterIndex = 0;
};

// 1. 점수 시스템: 보유 금액 변경
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTeamMoneyUpdated, int32, NewTotalMoney);

// 2. 점수 시스템: 가구 정착 및 정산
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFurnitureSettled, int32, AddedMoney, int32, Grade);

// 3. 상호작용 대상 변경
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractTargetChanged, AActor*, Target, FString, Key);

// 4. 가구 내구도 변경
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDurabilityChanged, float, Current, float, Max);

// 5. 로비 슬롯 정보 업데이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLobbySlotUpdated, int32, SlotIndex, FPlayerInfo, PlayerInfo);

// 6. UI 상태(화면) 전환 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateChanged, EE_UIState, NewState);

// 명세 ③ 수정: 전원 준비완료 시 호스트가 시작을 누르면 카운트다운 없이 즉시 전환된다.
//             따라서 로비 카운트다운 델리게이트(FOnLobbyCountdownUpdated)는 제거되었다.


/**
 * UMockUIController - UI Prototype Game Instance Subsystem
 */
UCLASS()
class TEAMCARRY_API UMockUIController : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UMockUIController();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Delegates ---
	UPROPERTY(BlueprintAssignable, Category = "UI|Delegates")
	FOnTeamMoneyUpdated OnTeamMoneyUpdated;

	UPROPERTY(BlueprintAssignable, Category = "UI|Delegates")
	FOnFurnitureSettled OnFurnitureSettled;

	UPROPERTY(BlueprintAssignable, Category = "UI|Delegates")
	FOnInteractTargetChanged OnInteractTargetChanged;

	UPROPERTY(BlueprintAssignable, Category = "UI|Delegates")
	FOnDurabilityChanged OnDurabilityChanged;

	UPROPERTY(BlueprintAssignable, Category = "UI|Delegates")
	FOnLobbySlotUpdated OnLobbySlotUpdated;

	UPROPERTY(BlueprintAssignable, Category = "UI|Delegates")
	FOnStateChanged OnStateChanged;


	// --- Routing & Stack Management ---
	UFUNCTION(BlueprintCallable, Category = "UI|Controller")
	void ReplaceState(EE_UIState NewState);

	UFUNCTION(BlueprintCallable, Category = "UI|Controller")
	void PushOverlay(const FString& OverlayName);

	UFUNCTION(BlueprintCallable, Category = "UI|Controller")
	void PopCurrentOverlay();

	UFUNCTION(BlueprintPure, Category = "UI|Controller")
	EE_UIState GetCurrentState() const { return CurrentState; }


	// --- Lobby & Simulation Helpers ---
	// S_CharacterSelect에서 Btn_Ready 클릭 시 호출할 더미 준비처리 함수
	UFUNCTION(BlueprintCallable, Category = "UI|Controller")
	void SetLobbySlotReady(int32 SlotIndex, bool bIsReady);


private:
	// Current State tracking
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI|Controller", meta = (AllowPrivateAccess = "true"))
	EE_UIState CurrentState;

	// In-game Simulation Timer variables
	FTimerHandle SimulationTimerHandle;
	int32 SimulationTicks;
	int32 SimulatedMoney;
	float SimulatedDurability;

	// Core simulation update tick
	void UpdateInGameMockSimulation();

	// Active overlay list (mock representation of UI stack)
	TArray<FString> MockOverlayStack;
};
