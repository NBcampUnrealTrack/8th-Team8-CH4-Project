// TCPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "TeamCarry/UI/GameUIPlayerController.h"
#include "TeamCarry/UI/MockUIController.h"
#include "TCPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class ATCStageSelectBoard;
class UUserWidget;
class US_Loading;

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

	// UTCSessionFlow::HostServerTravel() 이 서버 트래블 직전, 접속 중인 모든 PC에 호출한다(명세 2장
	// "로딩 화면 동기화 수정"). 호스트 로컬(OnTravelStarted)과 달리, 다른 클라이언트는 이 RPC로만
	// 로딩 화면(S_Loading)을 확실히 띄울 수 있다 — 호스트 자신도 포함되지만 MockUIController의
	// ShowPersistentLoadingWidget()이 IsInViewport() 체크로 idempotent라 안전하다.
	UFUNCTION(Client, Reliable, Category = "TeamCarry|Session")
	void ClientShowLoadingScreen();

	// 스테이지 맵(S_InGame) 진입 로딩 완료 보고. 내 화면은 BeginPlay() 의 스테이지 맵 분기에서
	// 이미 즉시 InGame으로 전환되며(ESC 등 글로벌 입력이 처음부터 동작하도록), 이 RPC는 서버에
	// "전원 로딩 완료" 판정(GameState::CurrentPhase 게이팅, 카운트다운 시작)용 보고만 한다.
	UFUNCTION(Server, Reliable, Category = "TeamCarry|Session")
	void ServerReportMapLoaded();

	// ATeamCarryGameMode::NotifyPlayerFinishedLoading() 이 전원 로딩 완료(또는 재접속/후발 합류) 시
	// 호출한다. 정상 경로에서는 BeginPlay() 가 이미 InGame 전환을 마쳐 두어 idempotent no-op이며,
	// 응답 없는 클라이언트를 강제 진입시키는 타임아웃 폴백의 안전망으로 남아 있다.
	UFUNCTION(Client, Reliable, Category = "TeamCarry|Session")
	void ClientNotifyAllPlayersLoaded();

	// ATeamCarryGameMode 의 지연 안내 타이머(LoadingStallNoticeSeconds)가 호출한다. 게임 상태는
	// 일절 변하지 않으며, 로딩 화면에 안내 문구와 나가기 버튼만 노출시킨다 — 전원 로딩 완료가
	// 게이트의 유일한 통과 조건이라는 원칙은 그대로 유지된다.
	UFUNCTION(Client, Reliable, Category = "TeamCarry|Session")
	void ClientNotifyLoadingStalled();

	// BP_StageSelectBoard와의 상호작용 시 진입하는 "게시판 클릭 모드"(명세 4장-5, 게시판 UI 개정).
	// 마우스 커서를 노출해 캐릭터의 WidgetInteractionComponent로 BoardScreen(월드 스페이스 위젯)의
	// 목록/확인/취소 버튼을 클릭할 수 있게 한다. ATCStageSelectBoard::OnInteract_Implementation(서버)이
	// 상호작용한 플레이어의 PC에 Client RPC로 호출한다.
	// Board: 레벨에 배치된 액터라 클라이언트에도 동일 인스턴스가 존재 — Input_BoardListUp/Down이
	// 리스트 탐색을 위임할 대상을 찾을 수 있도록 ActiveBoard에 저장해 둔다.
	UFUNCTION(Client, Reliable, Category = "TeamCarry|Lobby")
	void ClientEnterBoardInteractionMode(ATCStageSelectBoard* Board);

	// 게시판 클릭 모드 종료. W_StageBoardScreen의 확인/취소 클릭 시(호스트 자신의 로컬 호출,
	// 리슨 서버이므로 서버=호스트 프로세스) 또는 ESC 시 호출한다.
	UFUNCTION(BlueprintCallable, Category = "TeamCarry|Lobby")
	void ExitBoardInteractionMode();

	// ATCPlayerCharacter::Interact()가 좌클릭을 GrabComponent(가구 잡기) 대신
	// WidgetInteraction(월드 위젯 클릭)으로 넘길지 판단하는 데 사용한다.
	FORCEINLINE bool IsBoardInteractionModeActive() const { return bBoardInteractionModeActive; }

protected:
	// --- UI 테스트용 BeginPlay() ---
	virtual void BeginPlay() override;

	// 글로벌 UI 단축키(IA_ToggleESCUI/IA_SkipTutorial) 바인딩.
	virtual void SetupInputComponent() override;

	// 게시판 클릭 모드 중 BoardSelectCursorWidgetInstance를 매 프레임 마우스 위치로 옮긴다.
	// (EMouseCursor::Custom + SetSoftwareCursorWidget 방식은 이 프로젝트 창 설정에서 실제로
	// 렌더링되지 않는 것을 확인해 — 화면에 아무 커서 변화도 없었다 — 위젯을 직접 뷰포트에 얹어
	// 마우스를 따라다니게 하는 방식으로 대체했다.)
	virtual void PlayerTick(float DeltaTime) override;

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

	// 게시판 클릭 모드 중 마우스를 따라다니며 표시할 위젯 클래스(빨간 점).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI")
	TSubclassOf<UUserWidget> BoardSelectCursorWidgetClass;

	// BoardSelectCursorWidgetClass의 실제 인스턴스. BeginPlay()에서 한 번 생성해 두고,
	// 게시판 클릭 모드 진입/종료 시 뷰포트에 추가/제거만 한다.
	UPROPERTY()
	TObjectPtr<UUserWidget> BoardSelectCursorWidgetInstance;

	// 게시판 클릭 모드(bBoardInteractionModeActive) 중에만 유효한 리스트 탐색(위/아래 화살표).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI")
	TObjectPtr<UInputAction> IA_BoardListUp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI")
	TObjectPtr<UInputAction> IA_BoardListDown;

	// --- 로비 단축키(F1~F4, S_Lobby 전용 — IMC_GlobalUI 에 함께 배정) ---
	// F1: 준비 토글(Btn_Ready 와 동일 동작).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI")
	TObjectPtr<UInputAction> IA_LobbyReady;

	// F2: 게임 시작(방장 전용, Btn_Start 와 동일 동작).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI")
	TObjectPtr<UInputAction> IA_LobbyStart;

	// F3: 스테이지 선택 보드로 이동(방장 전용, Btn_StageSelect 와 동일 동작).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI")
	TObjectPtr<UInputAction> IA_LobbyStageSelect;

	// F4: 조작법(O_KeyGuide) 열기(전원, Btn_KeyGuide 와 동일 동작).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI")
	TObjectPtr<UInputAction> IA_LobbyHelp;

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

	// IA_BoardListUp/Down 핸들러: 게시판 클릭 모드 중에만 ActiveBoard의 BoardScreen에 위임한다.
	void Input_BoardListUp();
	void Input_BoardListDown();

	// IA_LobbyReady/Start/StageSelect/Help 핸들러(F1~F4). 전부 State가 Lobby이고, 게시판 클릭
	// 모드가 아니며, 오버레이가 떠 있지 않을 때만 동작한다(Input_ToggleLobbyCursor와 동일 가드).
	// Start/StageSelect는 방장 전용이라 Btn_Start/Btn_StageSelect의 노출 조건과 동일하게 IsHost()도 확인한다.
	void Input_LobbyReady();
	void Input_LobbyStart();
	void Input_LobbyStageSelect();
	void Input_LobbyHelp();

	// 위 4개 핸들러가 공통으로 확인하는 가드(Lobby 상태 + 게시판 모드 아님 + 오버레이 없음).
	bool CanHandleLobbyShortcut() const;

	// 현재 로비 커서가 켜져 있는지(Alt 토글 상태).
	bool bLobbyCursorActive = false;

	// 현재 게시판 클릭 모드(ExitBoardInteractionMode 참고)가 켜져 있는지.
	bool bBoardInteractionModeActive = false;

	// ClientEnterBoardInteractionMode(Board)로 진입한 게시판. Input_BoardListUp/Down이
	// 리스트 탐색을 위임할 대상을 여기서 찾는다.
	TWeakObjectPtr<ATCStageSelectBoard> ActiveBoard;

	// UMockUIController::OnStateChanged 구독 핸들러.
	// 오버레이를 2단 이상 중첩해서 열고 닫으면(예: O_PauseMenu 위에서 O_Settings/O_KeyGuide/
	// O_SaveLoad 를 열었다 닫는 경우) CommonUI 라우터가 leaf-most 위젯을 재계산하는 과정에서
	// 게임 뷰포트 포커스 복원이 신뢰할 수 없게 되는 경우가 재현된다(원인은 CommonUI/Slate
	// 내부로 추정, 정확한 근본 원인 미상). BeginPlay() 에서 이미 검증된 방식(PC 가 직접
	// SetInputMode 호출)을 InGame/Tutorial 로 돌아올 때마다 다시 적용해 확실히 복구한다.
	UFUNCTION()
	void HandleUIStateChanged(EE_UIState NewState);

	// 마무리 연출 완료 후 지속형 로딩 위젯을 내린다. 델리게이트 콜백과 세이프티 타이머가 겹칠 수
	// 있으므로 재진입 가드를 둔다. (PC 는 하드/Seamless 트래블 모두에서 새로 스폰되므로 플래그
	// 초기화는 불필요하다.)
	void FinishLoadingScreen();

	FTimerHandle FinishAnimSafetyHandle;
	bool bLoadingScreenFinished = false;

	// 맵 로딩 완료 보고 재시도. 트래블 직후에는 이 PC 의 액터 채널이 아직 완전히 열리지 않아
	// 서버 RPC 한 발이 유실될 수 있는데, 게이트는 전원 보고를 요구하고 강제 시작 폴백이 없어
	// 한 명만 유실돼도 전원이 로딩에서 영구 대기한다. 보고는 멱등(플래그 set + 게이트 재평가)이라
	// 서버가 응답(ClientNotifyAllPlayersLoaded)할 때까지 주기적으로 다시 보낸다.
	void StartMapLoadedReportRetry();

	FTimerHandle MapLoadedReportRetryHandle;
};
