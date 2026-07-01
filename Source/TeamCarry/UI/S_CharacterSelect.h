// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "TeamCarry/UI/MockUIController.h"
#include "S_CharacterSelect.generated.h"

class UButton;
class UTextBlock;
class ATCPlayerController;
class ATCLobbyGameState;

/**
 * US_CharacterSelect - Lobby and Character Select Screen implementing CommonActivatableWidget
 */
UCLASS()
class TEAMCARRY_API US_CharacterSelect : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- Control Buttons ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Ready;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Start;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UButton> Btn_Back;

	// --- Optional Slot Texts for Visualizing Lobby State ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Slot1_Name;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Slot1_Status;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Slot2_Name;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Slot2_Status;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Slot3_Name;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Slot3_Status;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Slot4_Name;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Slot4_Status;

private:
	UFUNCTION()
	void HandleReadyClicked();

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleBackClicked();

	// Delegate listener (mock 경로 — MockUIController 목 데이터)
	UFUNCTION()
	void HandleLobbySlotUpdated(int32 SlotIndex, FPlayerInfo PlayerInfo);

	// 네트워크 경로 — ATCLobbyGameState 의 로비 변경 통지
	UFUNCTION()
	void HandleLobbyPlayersChanged();

	// 네트워크 로비가 존재하면 true(복제 PlayerState 기반). false면 mock 경로 사용.
	bool TryBindNetworkLobby();

	// GameState->PlayerArray 를 읽어 슬롯 텍스트/시작버튼을 갱신.
	void RefreshLobbyFromGameState();

	// 소유 PC 를 ATCPlayerController 로 캐스팅(없으면 nullptr).
	ATCPlayerController* GetTCPlayerController() const;

	// 슬롯 인덱스(0~3)별 Name/Status 텍스트 위젯 반환.
	void GetSlotTexts(int32 SlotIndex, UTextBlock*& OutName, UTextBlock*& OutStatus) const;

	bool bLocalPlayerReady = false;

	// 네트워크 로비 바인딩 여부(true면 mock 더미 슬롯/시뮬레이션 비활성).
	bool bNetworkedLobby = false;

	// 구독 중인 로비 GameState(해제용).
	TWeakObjectPtr<ATCLobbyGameState> BoundLobbyState;
};
