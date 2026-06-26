// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TeamCarry/UI/UIHost.h"
#include "GameUIPlayerController.generated.h"

class UW_RootLayout;
class UCommonActivatableWidget;
class UMockUIController;

/**
 * AGameUIPlayerController - UI 생성 권한을 가진 범용 컨트롤러.
 *
 * 메뉴/인게임 등 레벨 종류에 무관한 범용 UI 호스트다. 화면/오버레이 위젯 클래스는
 * 에디터(BP 서브클래스)에서 주입하므로, 레벨별 GameMode 가 이 컨트롤러를
 * PlayerControllerClass 로 지정하기만 하면 재사용된다.
 *
 * 역할 분리:
 *  - 상태/스택 관리는 UMockUIController(GameInstanceSubsystem)가 담당.
 *  - 실제 위젯 생성(CreateWidget/AddToViewport)과 입력 모드 제어는 본 컨트롤러가 담당.
 *
 * 통신 패턴:
 *  - PC -> Subsystem : GetSubsystem<UMockUIController>() (GI 수명과 동일하여 항상 안전).
 *  - Subsystem -> PC : BeginPlay 에서 IUIHost 로 자기 등록, EndPlay 에서 해제(약참조로 보관).
 */
UCLASS()
class TEAMCARRY_API AGameUIPlayerController : public APlayerController, public IUIHost
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- IUIHost: 라우터가 위임하는 실제 생성/제거 진입점 ---
	virtual void ShowState(EE_UIState NewState) override;
	virtual UCommonActivatableWidget* ShowOverlay(FName OverlayId) override;
	virtual void HideTopOverlay() override;

protected:
	// PC 가 소유·생성하는 루트 레이아웃 위젯 클래스(에디터에서 WBP_RootLayout 지정).
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UW_RootLayout> RootLayoutClass;

	// 풀스크린 화면: State -> 위젯 클래스 (에디터 할당).
	UPROPERTY(EditDefaultsOnly, Category = "UI|Screens")
	TMap<EE_UIState, TSubclassOf<UCommonActivatableWidget>> ScreenWidgetClasses;

	// 오버레이: 이름(ID) -> 위젯 클래스. 키는 PushOverlay 인자(예: "O_Settings")와 일치시킨다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Overlays")
	TMap<FName, TSubclassOf<UCommonActivatableWidget>> OverlayWidgetClasses;

	// 첫 진입 화면(에디터에서 변경 가능).
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	EE_UIState InitialState = EE_UIState::MainMenu;

private:
	// 생성된 루트 레이아웃 인스턴스(런타임 보관).
	UPROPERTY(Transient)
	TObjectPtr<UW_RootLayout> RootLayout;

	// 라우터(서브시스템) 접근 헬퍼. PC 가 살아있는 한 항상 유효.
	UMockUIController* GetRouter() const;
};
