// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MockUIController.generated.h"

// Forward declaration
class AActor;
class IUIHost;

UENUM(BlueprintType)
enum class EE_UIState : uint8
{
	None,
	Boot,
	MainMenu,
	SlotSelect,
	CharacterSelect,
	Tutorial,
	StageSelect,
	InGame,
	Result
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

// 7. 남은 가구 개수 변경
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRemainingFurnitureUpdated, int32, NewCount);

// 8. 게임 결과(최종 점수/별 개수/소요 시간) 확정 델리게이트 수정
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGameResultReady, int32, FinalScore, int32, StarCount, float, ElapsedTime);

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

	UPROPERTY(BlueprintAssignable, Category = "UI|Delegates")
	FOnRemainingFurnitureUpdated OnRemainingFurnitureUpdated;

	UPROPERTY(BlueprintAssignable, Category = "UI|Delegates")
	FOnGameResultReady OnGameResultReady;


	// --- Routing & Stack Management ---
	UFUNCTION(BlueprintCallable, Category = "UI|Controller")
	void ReplaceState(EE_UIState NewState);

	UFUNCTION(BlueprintCallable, Category = "UI")
	class UCommonActivatableWidget* PushOverlay(const FString& OverlayName);

	UFUNCTION(BlueprintCallable, Category = "UI|Controller")
	void PopCurrentOverlay();

	UFUNCTION(BlueprintPure, Category = "UI|Controller")
	EE_UIState GetCurrentState() const { return CurrentState; }

	// ReplaceState() 직전까지의 화면. 화면이 어디서 진입했는지에 따라 뒤로가기 목적지를
	// 분기해야 하는 화면(예: S_StageSelect)에서 사용한다.
	UFUNCTION(BlueprintPure, Category = "UI|Controller")
	EE_UIState GetPreviousState() const { return PreviousState; }


	// --- Lobby Helpers ---
	// S_CharacterSelect에서 Btn_Ready 클릭 시 호출할 더미 준비처리 함수
	UFUNCTION(BlueprintCallable, Category = "UI|Controller")
	void SetLobbySlotReady(int32 SlotIndex, bool bIsReady);


	// --- Real Game Data Injection ---
	// GameState 등 실제 데이터 소스에서 호출하여 UI를 갱신시키는 징검다리 함수들.
	UFUNCTION(BlueprintCallable, Category = "UI|GameData")
	void UpdateTeamMoney(int32 NewMoney);

	UFUNCTION(BlueprintCallable, Category = "UI|GameData")
	void UpdateRemainingFurniture(int32 NewCount);

	// 게임 종료 시 최종 점수/별 개수를 확정하고, 지난 시간도 체크하며 S_Result 화면으로 전환한다.
	// (S_Result는 위젯 생성 시점에 GetLastFinalScore()/GetLastStarCount()로 캐시된 값을 읽어간다.)
	UFUNCTION(BlueprintCallable, Category = "UI|GameData")
	void TriggerGameResult(int32 FinalScore, int32 StarCount, float ElapsedTime);

	UFUNCTION(BlueprintPure, Category = "UI|GameData")
	int32 GetLastFinalScore() const { return LastFinalScore; }

	UFUNCTION(BlueprintPure, Category = "UI|GameData")
	int32 GetLastStarCount() const { return LastStarCount; }

	UFUNCTION(BlueprintPure, Category = "UI|GameData")
	float GetLastElapsedTime() const { return LastElapsedTime; }

	// --- UI Host (PlayerController) 등록 ---
	// PC 가 BeginPlay/EndPlay 에서 자신을 호스트로 등록/해제한다.
	// 라우터는 실제 위젯 생성/제거를 이 호스트에 위임한다.
	void RegisterUIHost(const TScriptInterface<IUIHost>& InHost);
	void UnregisterUIHost(const TScriptInterface<IUIHost>& InHost);


private:
	// Current State tracking
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI|Controller", meta = (AllowPrivateAccess = "true"))
	EE_UIState CurrentState;

	// ReplaceState() 로 갱신되기 직전의 CurrentState. GameInstance 서브시스템이라
	// 같은 레벨 내 화면 교체는 물론 ServerTravel(레벨 이동)을 넘어서도 유지된다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI|Controller", meta = (AllowPrivateAccess = "true"))
	EE_UIState PreviousState = EE_UIState::None;

	// Active overlay list (mock representation of UI stack)
	TArray<FString> MockOverlayStack;

	// TriggerGameResult()로 캐시되는 최종 결과 데이터.
	// S_Result 위젯은 ReplaceState()에 의해 TriggerGameResult() 내부에서 동기적으로 생성되므로,
	// NativeConstruct 시점에 이 값들을 즉시 읽어갈 수 있다.
	UPROPERTY()
	int32 LastFinalScore = 0;

	UPROPERTY()
	int32 LastStarCount = 0;

	UPROPERTY()
	float LastElapsedTime = 0.0f;

	// 연속 버튼 클릭으로 입력 누락 방지를 위한 타이머.
	float LastMenuToggleTime = 0.0f;
	const float MenuToggleCooldown = 0.2f;

	// 실제 위젯 생성/제거를 위임할 호스트(PC). 약참조로 보관하여 PC 파괴 시 dangling 을 방지한다.
	UPROPERTY()
	TWeakObjectPtr<UObject> UIHostObject;

	// 약참조를 IUIHost* 로 해석. PC 가 이미 파괴되었으면 nullptr 을 반환한다.
	IUIHost* GetUIHost() const;
};
