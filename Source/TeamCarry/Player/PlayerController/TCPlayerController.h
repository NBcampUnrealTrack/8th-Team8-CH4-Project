// TCPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "TeamCarry/UI/GameUIPlayerController.h"
#include "TeamCarry/UI/MockUIController.h"
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

	// O_CharacterSelect 가 호출한다. 즉시 적용 방식(확인 버튼 없음).
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Lobby")
	void RequestSetCharacterIndex(int32 InCharacterIndex);

	// Alt(IA_ToggleLobbyCursor) 토글: S_Lobby 는 GetDesiredInputConfig() 로 "캐릭터 조작"을 항상
	// 고정 선언해 두므로(라우터가 임의로 되돌리지 않도록), 커서를 꺼내는 동작은 여기서 SetInputMode 를
	// 직접 호출해 처리한다. 오버레이 Push/Pop 같은 트리 변경이 없는 한 이 값은 그대로 유지된다.
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Lobby")
	void SetLobbyCursorActive(bool bInActive);

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

	// 로비 커서 토글(Alt). S_Lobby 에서만 유효 — 누르면 마우스가 나와 로비 인라인 버튼을 조작할 수
	// 있고, 다시 누르면 캐릭터 조작으로 복귀한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI")
	TObjectPtr<UInputAction> IA_ToggleLobbyCursor;

private:
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bInReady);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartGame();

	UFUNCTION(Server, Reliable)
	void ServerSetCharacterIndex(int32 InCharacterIndex);

	// IA_ToggleESCUI 핸들러: 현재 State 가 InGame/Tutorial 일 때만 O_PauseMenu 오버레이를 연다.
	// (오버레이를 닫는 동작은 CommonUI 의 NativeOnHandleBackAction 이 자체 처리하므로,
	//  컨트롤러는 여는 로직만 담당한다.)
	void Input_ToggleESCUI();

	// IA_SkipTutorial 핸들러: 현재 State 가 Tutorial 일 때만 StageSelect 로 직행한다.
	void Input_SkipTutorial();

	// IA_ToggleLobbyCursor 핸들러: State 가 Lobby 이고 오버레이가 떠 있지 않을 때만 토글한다
	// (오버레이가 열려 있으면 그쪽 GetDesiredInputConfig 가 이미 입력을 소유하므로 끼어들지 않는다).
	void Input_ToggleLobbyCursor();

	// 현재 로비 커서가 켜져 있는지(Alt 토글 상태).
	bool bLobbyCursorActive = false;

	// UMockUIController::OnStateChanged 구독 핸들러.
	// 오버레이를 2단 이상 중첩해서 열고 닫으면(예: O_PauseMenu 위에서 O_Settings/O_KeyGuide/
	// O_SaveLoad 를 열었다 닫는 경우) CommonUI 라우터가 leaf-most 위젯을 재계산하는 과정에서
	// 게임 뷰포트 포커스 복원이 신뢰할 수 없게 되는 경우가 재현된다(원인은 CommonUI/Slate
	// 내부로 추정, 정확한 근본 원인 미상). BeginPlay() 에서 이미 검증된 방식(PC 가 직접
	// SetInputMode 호출)을 InGame/Tutorial 로 돌아올 때마다 다시 적용해 확실히 복구한다.
	UFUNCTION()
	void HandleUIStateChanged(EE_UIState NewState);
};
