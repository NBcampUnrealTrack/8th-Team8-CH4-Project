// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "O_CharacterSelect.generated.h"

class UCommonButtonBase;
class UTextBlock;
class UWidget;
class ATCPlayerController;
class ATCLobbyGameState;

/**
 * UO_CharacterSelect - 캐릭터 외형 변경 전용 오버레이(명세 4장-4, 구 S_CharacterSelect 분리).
 *
 * 준비/시작 기능은 없다(S_Lobby 로 이관 완료). 외형 선택은 즉시 적용 방식이라 확인 버튼이 없고,
 * CharacterIndex 를 Server RPC 로 전달해 ATCPlayerState 에 복제한 뒤 OnLobbyPlayersChanged 로
 * 전원에게 통지된다.
 *
 * 참고(3단계 범위): 현재 코드베이스에는 스킨/메시 목록이나 교체 로직이 전혀 없어(사용자 확인 완료),
 * 이 단계에서는 CharacterIndex 값 복제·선택 UI까지만 구현하고 로비 월드 폰의 실제 외형 적용 훅은
 * 의도적으로 생략한다. 목록은 데이터 소스가 생기기 전까지 고정 3개 옵션(0~2)의 플레이스홀더다.
 */
UCLASS()
class TEAMCARRY_API UO_CharacterSelect : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 활성화 중 게임 입력을 차단하고 UI 전용 입력으로 전환한다(명세 4장-4, 6장-2).
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// ESC = '한 단계 뒤로/닫기'(명세 5-1). Btn_Close 클릭과 동일하게 PopCurrentOverlay() 로 닫는다.
	virtual bool NativeOnHandleBackAction() override;

	// --- 외형 옵션 목록(플레이스홀더 고정 3개) ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Option0;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Option1;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Option2;

	// --- 미리보기(플레이스홀더 텍스트 — 실제 썸네일/외형 프리뷰는 미구현) ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Preview;

	// --- 닫기 ---
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UCommonButtonBase> Btn_Close;

private:
	UFUNCTION()
	void HandleOption0Clicked();

	UFUNCTION()
	void HandleOption1Clicked();

	UFUNCTION()
	void HandleOption2Clicked();

	UFUNCTION()
	void HandleCloseClicked();

	// ATCLobbyGameState 의 로비 변경 통지(다른 플레이어의 외형 변경 포함).
	UFUNCTION()
	void HandleLobbyPlayersChanged();

	// 선택 요청 공통 처리: Server RPC 전달 + 미리보기 텍스트 즉시(낙관적) 갱신.
	void RequestSelectOption(int32 InCharacterIndex);

	// 로컬 플레이어의 복제된 CharacterIndex 로 미리보기 텍스트를 갱신.
	void RefreshPreviewFromPlayerState();

	ATCPlayerController* GetTCPlayerController() const;

	// 구독 중인 로비 GameState(해제용).
	TWeakObjectPtr<ATCLobbyGameState> BoundLobbyState;
};
