// TCPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "TeamCarry/UI/GameUIPlayerController.h"
#include "TCPlayerController.generated.h"

/**
 * ATCPlayerController - UI 호스트(AGameUIPlayerController) + 로비 네트워크 RPC.
 *
 * AGameUIPlayerController 를 상속해 위젯 호스팅 능력을 그대로 갖고,
 * 그 위에 로비의 Ready/시작 요청을 서버로 올리는 RPC 를 더한다.
 * 모든 레벨의 PlayerControllerClass 를 이 클래스(또는 BP 서브클래스)로 지정한다(EDITOR-TASKS §2).
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

private:
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bInReady);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartGame();
};
