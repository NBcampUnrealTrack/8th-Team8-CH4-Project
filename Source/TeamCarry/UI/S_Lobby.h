// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "S_Lobby.generated.h"

class UCommonButtonBase;
class UTextBlock;
class ATCPlayerController;
class ATCLobbyGameState;

/**
 * US_Lobby - 플레이어블 로비 화면(명세 4장-3, HUD형 풀스크린).
 *
 * L_Lobby 진입 시 EE_UIState::Lobby 로 ReplaceState 되며, S_InGame 처럼 캐릭터 조작이
 * 살아있는 상태에서 로비 버튼(캐릭터 선택/스테이지 선택/준비/시작)을 제공한다.
 * 로비 복제 상태(ATCPlayerState/ATCLobbyGameState)는 이미 네트워크로 구현되어 있으므로,
 * 구 S_CharacterSelect 의 mock 폴백 경로는 승계하지 않고 네트워크 경로만 사용한다.
 */
UCLASS()
class TEAMCARRY_API US_Lobby : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ESC = '한 단계 뒤로'(명세 5-1). Btn_Back 클릭과 동일하게 O_Confirm(방 나가기) 모달을 거친다.
	virtual bool NativeOnHandleBackAction() override;

	// 항상 "캐릭터 조작"(커서 숨김)을 선언한다(명세 6장-2, S_InGame과 동일 패턴). Alt 로 커서를 꺼내는
	// 동작은 PC(ATCPlayerController::SetLobbyCursorActive)가 직접 SetInputMode 를 호출해 처리한다.
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// --- 로비 버튼(명세 4장-3) ---
	// 전원 노출: 캐릭터 외형 선택 오버레이(O_CharacterSelect, 3단계 예정)를 연다.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_CharacterSelect;

	// 방장 전용: 스테이지 선택 오버레이(O_StageSelect, 5단계 예정)를 연다.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_StageSelect;

	// 전원 노출: 준비 상태 토글.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Ready;

	// 방장 전용: 전원 준비 시에만 활성화되어 게임을 시작한다.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Start;

	// 나가기(뒤로가기). ESC 와 동일하게 동작한다.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Back;

	// '조작법 ' 확인 버튼.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_KeyGuide;

	// --- 방 코드 표시(ATCLobbyGameState::RoomCode 복제값) ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_RoomCode;

	// --- 간이 로비 슬롯 표시(프로토타입 재량, 명세 4장-3) ---
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
	void HandleCharacterSelectClicked();

	UFUNCTION()
	void HandleStageSelectClicked();

	UFUNCTION()
	void HandleReadyClicked();

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleKeyGuideClicked();

	// O_Confirm 팝업에서 '확인'을 눌렀을 때 실행될 브릿지 함수(방 나가기 → 타이틀 복귀).
	UFUNCTION()
	void OnConfirmLeaveLobby();

	// ATCLobbyGameState 의 로비 변경 통지(입퇴장/Ready/캐릭터/방코드 변경 공통 경로).
	UFUNCTION()
	void HandleLobbyPlayersChanged();

	// GameState 구독 시도. 성공 시 true(구독 + 최초 1회 갱신까지 수행).
	bool TryBindNetworkLobby();

	// GameState->PlayerArray 를 읽어 슬롯 텍스트/방코드/버튼 상태를 갱신.
	void RefreshLobbyFromGameState();

	// 방장 전용 버튼(Btn_StageSelect/Btn_Start)의 노출 여부를 IsHost() 기준으로 갱신.
	void RefreshHostOnlyVisibility();

	// 소유 PC 를 ATCPlayerController 로 캐스팅(없으면 nullptr).
	ATCPlayerController* GetTCPlayerController() const;

	// 슬롯 인덱스(0~3)별 Name/Status 텍스트 위젯 반환.
	void GetSlotTexts(int32 SlotIndex, UTextBlock*& OutName, UTextBlock*& OutStatus) const;

	// 로컬 플레이어의 마지막으로 알려진 준비 상태(Btn_Ready 토글 기준값).
	bool bLocalPlayerReady = false;

	// 구독 중인 로비 GameState(해제용).
	TWeakObjectPtr<ATCLobbyGameState> BoundLobbyState;
};
