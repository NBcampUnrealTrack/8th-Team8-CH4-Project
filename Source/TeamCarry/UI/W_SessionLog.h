// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "W_SessionLog.generated.h"

class UTextBlock;

/**
 * UW_SessionLog - 접속 로그(입장/퇴장 등) 텍스트 칸(명세 3장·4장-3·4장-7·7장-4).
 *
 * S_Lobby 와 S_InGame 에 동일하게 배치되는 단순 위젯(버튼과 같은 급, UCommonUserWidget).
 * UMockUIController::OnSessionLogAdded 를 구독해 줄 단위로 누적 표시하고, 최대 N줄을
 * 넘으면 오래된 줄부터 제거한다.
 */
UCLASS()
class TEAMCARRY_API UW_SessionLog : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_Log;

	// 화면에 유지할 최대 줄 수(넘으면 오래된 줄부터 제거).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|SessionLog")
	int32 MaxDisplayedLines = 8;

private:
	UFUNCTION()
	void HandleSessionLogAdded(FText LogMessage);

	void RefreshDisplayText();

	TArray<FText> DisplayedLines;
};
