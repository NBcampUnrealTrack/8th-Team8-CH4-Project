// TCPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "TeamCarry/UI/GameUIPlayerController.h"
#include "TCPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

/**
 * ATCPlayerController - UI 호스트(AGameUIPlayerController) + 로비 네트워크 RPC.
 *
 * AGameUIPlayerController 를 상속해 위젯 호스팅 능력을 그대로 갖고,
 * 그 위에 로비의 Ready/시작 요청을 서버로 올리는 RPC 를 더한다.
 * 모든 레벨의 PlayerControllerClass 를 이 클래스(또는 BP 서브클래스)로 지정한다(EDITOR-TASKS §2).
 *
 * 글로벌 UI 단축키(일시정지/튜토리얼 스킵)도 이 컨트롤러가 Enhanced Input 으로 직접 처리한다.
 * HUD 위젯(S_InGame/S_Tutorial)은 더 이상 NativeOnKeyDown 을 갖지 않으므로,
 * 위젯이 포커스를 잃어도(마우스로 다른 위젯을 클릭해도) 단축키가 항상 동작한다.
 */
UCLASS()
class TEAMCARRY_API ATCPlayerController : public AGameUIPlayerController
{
	GENERATED_BODY()

public:
	// ── 클라이언트(소유) → 서버 의도 헬퍼 ──
	// UI(S_CharacterSelect)가 호출한다. 내부에서 Server RPC 로 권위에 위임.

	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Lobby")
	void RequestSetReady(bool bInReady);

	// 호스트 전용: 전원 준비 시 게임 시작(레벨 트래블).
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Lobby")
	void RequestStartGame();

protected:
	// --- UI 테스트용 BeginPlay() ---
	virtual void BeginPlay() override;

	// 글로벌 UI 단축키(IA_ToggleESCUI/IA_SkipTutorial) 바인딩.
	virtual void SetupInputComponent() override;

	// --- 글로벌 UI 입력 컨텍스트/액션 ---
	// 캐릭터(ATCPlayerCharacter)의 이동 IMC 와는 별개로, 폰 스폰 여부·HUD 포커스 상태와 무관하게
	// 항상 살아있어야 하는 컨트롤러 레벨 입력이다(에디터에서 IMC/IA 에셋 지정).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI")
	TObjectPtr<UInputMappingContext> IMC_GlobalUI;

	// 일시정지 메뉴 토글(예: ESC).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI")
	TObjectPtr<UInputAction> IA_ToggleESCUI;

	// 튜토리얼 건너뛰기(예: P, 또는 스킵 전용 키).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI")
	TObjectPtr<UInputAction> IA_SkipTutorial;

private:
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bInReady);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartGame();

	// IA_ToggleESCUI 핸들러: 현재 State 가 InGame/Tutorial 일 때만 O_PauseMenu 오버레이를 연다.
	// (오버레이를 닫는 동작은 CommonUI 의 NativeOnHandleBackAction 이 자체 처리하므로,
	//  컨트롤러는 여는 로직만 담당한다.)
	void Input_ToggleESCUI();

	// IA_SkipTutorial 핸들러: 현재 State 가 Tutorial 일 때만 StageSelect 로 직행한다.
	void Input_SkipTutorial();
};
