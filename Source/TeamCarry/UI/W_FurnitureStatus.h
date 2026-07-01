// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "W_FurnitureStatus.generated.h"

class UTextBlock;
class UProgressBar;

/**
 * UW_FurnitureStatus - 인게임 가구 상태창 서브 위젯 (명세 6 S_InGame 하위 컴포넌트).
 *
 * 명세 5-1: HUD(S_InGame)의 하위 컴포넌트이므로 UCommonUserWidget 을 상속한다.
 * (활성화 스택의 단위가 아니므로 UCommonActivatableWidget 이 아니다.)
 *
 * 커서가 가구 위에 올라갔을 때 UMockUIController::OnFurnitureHovered 델리게이트를 통해
 * UpdateFurnitureStatus() 가 호출되어 이름·내구도 게이지를 갱신한다.
 * 가구 데이터 구조체(FFurnitureStatusData)는 백엔드 연동 단계에서 추가될 예정이며,
 * 그 전까지는 개별 파라미터 형태의 임시 인터페이스를 사용한다.
 */
UCLASS()
class TEAMCARRY_API UW_FurnitureStatus : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UW_FurnitureStatus();

	// 가구 데이터 수신 및 UI 갱신 진입점.
	// 백엔드 연동 시 FFurnitureStatusData 구조체 단일 파라미터로 교체될 임시 인터페이스.
	UFUNCTION(BlueprintCallable, Category = "UI|FurnitureStatus")
	void UpdateFurnitureStatus(const FString& Name, float CurrentDurability, float MaxDurability);

	// 위젯을 화면에 표시한다 (S_InGame: Hover 진입 시 호출).
	UFUNCTION(BlueprintCallable, Category = "UI|FurnitureStatus")
	virtual void ShowStatus();

	// 위젯을 숨긴다 (S_InGame: Hover 이탈 시 호출).
	UFUNCTION(BlueprintCallable, Category = "UI|FurnitureStatus")
	virtual void HideStatus();

protected:
	virtual void NativeConstruct() override;

	// 가구 이름 텍스트 블록.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UTextBlock> Txt_FurnitureName;

	// 가구 내구도 게이지 (0.0 ~ 1.0 정규화 값으로 설정).
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "UI|Widget")
	TObjectPtr<UProgressBar> Bar_Durability;
};
