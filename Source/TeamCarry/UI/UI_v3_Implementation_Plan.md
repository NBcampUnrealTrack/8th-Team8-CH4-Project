# UI_v3_Implementation_Plan.md
# 로딩 화면 동기화 수정 + UI_Technical_Spec v3 기능 구현 — 실행 대기 중인 계획

> **상태: 미착수.** 이 문서는 실행하지 않은 계획서다. 다른 팀원들의 작업 상황(진행 중인 브랜치, 담당 파일)을 먼저 확인한 뒤 착수한다. 이 파일을 다시 제시받으면 아래 내용을 그대로 구현한다.

## Context

두 가지 작업을 진행한다.

1. **v3 스펙 반영:** 회의 내용을 반영해 `UI_Technical_Spec.md`를 v3로 갱신했다(게이지 UI, W_HelpPanel, O_PauseMenu 재구성, RestartStage()). 이제 실제 C++/UI 코드를 문서와 일치시킨다.
2. **로딩 화면 버그 수정 (튜터 피드백):** 레벨 트래블 시 로딩 화면이 아예 안 보이고 바로 인게임으로 넘어가는 현상이 있음. 조사 결과 두 가지 근본 원인을 확인했다.
   - **원인 A:** `UTCSessionFlow::OnTravelStarted`는 `HostStartGame()`/`HostReturnToLobby()`/`CompleteTutorial()` 내부에서 `IsHost()`인 프로세스에서만 로컬로 브로드캐스트된다. 같은 세션의 다른 클라이언트는 엔진의 seamless travel 넷 핸드셰이크로 조용히 맵을 이동당할 뿐 이 델리게이트를 전혀 받지 못해 `ReplaceState(Loading)`이 호출되지 않는다. 새 맵의 `ATCPlayerController::BeginPlay`가 곧장 목적지 State로 바꿔버려 로딩 화면 없이 순간이동한 것처럼 보인다. (`TCSessionFlow.cpp:186-247`, `TCPlayerController.cpp:34-112`)
   - **원인 B:** Loading에서 목적지 화면으로 넘어가는 시점이 전적으로 "내 PlayerController가 새 맵에서 BeginPlay를 언제 실행했는가"에만 의존한다. 다른 클라이언트의 로딩 완료 여부와 무관하게 각자 따로 넘어간다 — "전원 로딩 완료 후 진입"이라는 개념이 코드 어디에도 없다.
   - 튜터 피드백은 명시적으로 "인게임 레벨"(스테이지 맵) 진입에 한정된 요구이므로, **전원 대기 게이트는 스테이지 맵(S_InGame) 진입에만 적용**하고, 로비/타이틀 등 다른 목적지는 "로딩 화면이 확실히 보이게" 만드는 원인 A 수정만 적용한다(로비는 참가자가 서로 다른 시점에 합류하므로 "전원 대기" 개념 자체가 성립하지 않음).

이미 조사를 통해 관련 코드 경로(TCSessionFlow, MockUIController, TCPlayerController, TCPlayerState, TCLobbyGameMode/GameState, TeamCarryGameMode/GameState, S_InGame/O_PauseMenu/O_Confirm/W_SessionLog)를 모두 확인했다. 착수 시 먼저 이 파일이 여전히 최신 코드 상태와 맞는지(파일 경로/라인 번호가 그대로인지) 재확인할 것 — 다른 사람의 작업으로 해당 파일들이 바뀌었을 수 있음.

---

## Phase 1. 로딩 화면 동기화 수정

### 1-A. 모든 클라이언트에서 로딩 화면이 확실히 뜨게 만들기

기존 `OnTravelStarted`(호스트 로컬 전용)는 그대로 두고, `UTCSessionFlow::HostServerTravel()`(`TCSessionFlow.cpp:235-247`, `HostStartGame`/`HostReturnToLobby`/`CompleteTutorial`/신규 `RestartStage()`가 공유하는 private 헬퍼)에서 `World->ServerTravel(MapPath)` 호출 **직전**에, 현재 접속 중인 모든 `ATCPlayerController`에게 신규 Client RPC를 쏜다.

- `ATCPlayerController`에 `UFUNCTION(Client, Reliable) void ClientShowLoadingScreen();` 추가 (`TCPlayerController.h`). 구현은 `UMockUIController::ShowPersistentLoadingWidget()` + best-effort `ReplaceState(Loading)` 호출.
- `HostServerTravel()`에서 `World->GetPlayerControllerIterator()`를 순회하며 `ATCPlayerController`마다 `ClientShowLoadingScreen()` 호출(호스트 자신도 포함되지만 `ShowPersistentLoadingWidget()`가 `IsInViewport()` 체크로 idempotent라 안전).

엔진의 `FCoreUObjectDelegates::PreLoadMap` 같은 저수준 델리게이트를 쓰는 대안도 검토했으나, seamless travel 중 클라이언트에서 정확히 언제/어떻게 발화하는지 이 프로젝트의 엔진 버전으로 검증되지 않아 불확실성이 크다. 반면 위 RPC 방식은 이미 이 코드베이스 전역에서 쓰는 Server/Client RPC 패턴(`ServerSetReady` 등)과 동일한 신뢰도로 동작하므로 이쪽을 채택한다. `JoinRoomByCode`(클라이언트가 스스로 트래블 개시)와 `LeaveToTitle`(각자 로컬 `OpenLevel`)은 이미 자기 자신에게 로컬로 `OnTravelStarted`가 발화되므로 수정 불필요.

### 1-B. 스테이지 맵(S_InGame) 진입 시 "전원 로딩 완료" 게이트

`ATCLobbyGameState::AreAllPlayersReady()`/`ATCPlayerState::SetReadyAuthoritative()`/`ServerSetReady` RPC 체인(`TCLobbyGameState.cpp:92-114`, `TCPlayerState.cpp:24-38`, `TCPlayerController.cpp:19-22,227-233`)과 동일한 형태로 구현한다. 단, 이 플래그는 다른 클라이언트 UI에 보여줄 필요가 없고 서버만 알면 되므로 **복제하지 않는다**(bIsReady와의 의도적 차이).

1. **`ATCPlayerState`**: `bool bHasLoadedCurrentMap = false;`(non-replicated, 서버 전용 북키핑) 추가. `SetHasLoadedCurrentMapAuthoritative(bool)` 세터 추가(HasAuthority 가드, 변경 없으면 조기 리턴 — `SetReadyAuthoritative` 패턴 동일).

2. **`ATCPlayerController`**:
   - `BeginPlay()`의 스테이지 맵(`else`) 분기(`TCPlayerController.cpp:99-109`)를 수정: 즉시 `ReplaceState(InGame)` 하지 않고, 대신 `ServerReportMapLoaded()` RPC(Server, Reliable, 신규)만 호출하고 리턴.
   - `ServerReportMapLoaded_Implementation()`: `GetWorld()->GetAuthGameMode<ATeamCarryGameMode>()`를 찾아 `GameMode->NotifyPlayerFinishedLoading(this)` 호출.
   - `ClientNotifyAllPlayersLoaded()` RPC(Client, Reliable, 신규): `MockController->ReplaceState(EE_UIState::InGame)` + 기존 `bShowMouseCursor`/`SetInputMode(FInputModeGameAndUI())` 코드를 여기로 이동.

3. **`ATeamCarryGameMode`**:
   - `PostLogin`(신규 오버라이드, 새로 접속하는 케이스)과 `HandleSeamlessTravelPlayer`(신규 오버라이드, 로비→스테이지처럼 seamless travel로 도착하는 케이스)에서 해당 플레이어의 `PS->bHasLoadedCurrentMap = false`로 리셋 — `ATCLobbyGameMode`가 `LobbySlotIndex`를 두 경로 모두에서 배정하는 것과 동일한 이중 훅 패턴(`TCLobbyGameMode.cpp:44-81`).
   - `BeginPlay()`(`TeamCarryGameMode.cpp:28-46`): `SetGamePhase(WaitingToStart)`는 유지하되 `StartCountdown()` 즉시 호출을 제거. 대신 타임아웃 세이프티 타이머(예: 20초 — 클라이언트가 응답 없이 멈추는 경우 전체가 무한 대기하지 않도록)를 걸어 시간 초과 시 강제로 진행.
   - 신규 `NotifyPlayerFinishedLoading(APlayerController* PC)`: 해당 플레이어의 `PS->bHasLoadedCurrentMap = true` 설정 후 `AreAllConnectedPlayersLoaded()`(신규, `PlayerArray` 스캔 — `ATCLobbyGameState::AreAllPlayersReady()`와 동일한 형태) 체크.
     - **게임이 아직 `WaitingToStart`인 경우(정상 동시 시작):** 전원 로딩 완료 시 타임아웃 타이머 취소 → `StartCountdown()` 호출 → 현재 접속 중인 모든 `ATCPlayerController`에 `ClientNotifyAllPlayersLoaded()` 호출.
     - **게임이 이미 `Playing` 이후 단계인 경우(재접속/후발 합류):** 기존 재접속 로직(`TeamCarryGameMode.cpp:118-152`)과 충돌하지 않도록, 전체 게이트를 기다리지 않고 **그 플레이어 한 명에게만** 즉시 `ClientNotifyAllPlayersLoaded()` 호출.
   - `Logout` 훅에서도 게이트 재확인(대기 중 한 명이 나가서 남은 인원이 이미 전원 로딩 완료 상태가 되는 경우 대비).

`RestartStage()`(Phase 2에서 추가)는 스테이지 맵으로의 재트래블이므로 이 게이트를 자동으로 그대로 적용받는다 — 별도 처리 불필요.

---

## Phase 2. UI_Technical_Spec v3 기능 구현

### 2-1. 게이지 UI (S_InGame)

- **`ATeamCarryGameState`**: `int32 TotalLevelValue`(plain `Replicated`, 스테이지 중 불변) 추가.
- **`ATeamCarryGameMode::BeginPlay()`**: 기존에 `ATCFurnitureActor`를 순회해 `SetTotalFurnitureCount()`를 호출하는 루프(`TeamCarryGameMode.cpp:32-34`)에 이어서, 각 액터의 `FurnitureDataRow`(`FDataTableRowHandle`, `AFurnitureActor` 기반 클래스 소유 — `Plugins/CatchCharacter/.../FurnitureActor.h:40`)에서 BaseScore를 읽어 합산 → `GS->TotalLevelValue`에 저장(전체 최대 획득 가능 값어치 = 각 가구가 100% 내구도로 배달됐을 때의 BaseScore 합).
- **`S_InGame.h/.cpp`**: 기존 `TextBlock_Score`(`S_InGame.h:34`) 대신 `UProgressBar* PB_TeamMoney`(BindWidgetOptional) + 내부 텍스트(같은 위젯 안의 `TextBlock_Score`를 오버레이 텍스트로 재사용 가능, "현재/전체" 포맷으로 표시) 추가. `HandleTeamMoneyUpdated`(`S_InGame.cpp:124-131`)를 채움 비율 계산(`NewMoney / TotalLevelValue`) + 텍스트 갱신 로직으로 교체. `TotalLevelValue`는 `NativeConstruct` 시점에 `ATeamCarryGameState`에서 1회 조회해 캐시(기존에 `GS->TotalScore`를 미리 읽어오는 것과 같은 자리, `S_InGame.cpp:64-73`).

### 2-2. W_HelpPanel (구 O_KeyGuide 대체)

- 신규 C++ 클래스 `UW_HelpPanel : public UCommonUserWidget`(`Source\TeamCarry\UI\W_HelpPanel.h/.cpp`) — `W_SessionLog`와 같은 급의 최소 클래스. 정적 콘텐츠이므로 델리게이트 구독 없음. `BindWidgetOptional` 텍스트 하나(예: `Txt_HelpContent`, Multi-line)만 두고 내용은 Blueprint에서 직접 채우거나 `EditDefaultsOnly` 텍스트 배열로 노출.
- `WBP_S_InGame`에 이 위젯을 우측 중단~하단에 배치(에디터 작업, 아래 "에디터/블루프린트 작업" 참고).

### 2-3. O_PauseMenu 재구성 + RestartStage()

- **`UTCSessionFlow`**: `RestartStage()` 추가 (`HostReturnToLobby()`와 동일한 모양 — `IsHost()` 가드 후 `HostServerTravel(GetSelectedStageMapPath())`).
- **`O_PauseMenu.h/.cpp`**:
  - 제거: `Btn_KeyGuide`/`Btn_ToTitle` BindWidget 멤버, `HandleKeyGuideClicked`, `HandleToTitleClicked`/`OnConfirmToTitle`, `NativeConstruct`의 해당 바인딩 블록(`O_PauseMenu.h:47,53-59,71-79`, `O_PauseMenu.cpp:41-45,61-66,107-158`).
  - 추가: `Btn_ToLobby`/`Btn_Reset` BindWidget 멤버 + `HandleToLobbyClicked`/`HandleResetClicked` + confirm 브리지(`OnConfirmReturnToLobby`, `OnConfirmResetStage`, `UFUNCTION()` 무인자 — `FOnConfirmYesAction`용). 구현은 기존 `HandleToTitleClicked`가 쓰던 O_Confirm 패턴을 그대로 재사용(`O_PauseMenu.cpp:128-158` 참고: `PushOverlay("O_Confirm")` → `Cast<UO_Confirm>` → `SetupConfirm(title, desc, YesAction)`), Yes 콜백에서 각각 `UTCSessionFlow::HostReturnToLobby()` / `RestartStage()` 호출.
  - **호스트 전용 게이팅 신규 구현**: 현재 `Btn_Save`는 `IsHost()` 체크가 주석으로만 존재하고 실제 구현이 없다(`O_PauseMenu.cpp:51-58`) — 이번에 `Btn_Save`·`Btn_ToLobby`·`Btn_Reset` 세 버튼 모두에 대해 `NativeConstruct`에서 `UTCSessionFlow::IsHost()`로 `SetVisibility(Collapsed)` 처리를 실제로 구현한다(스펙 4장-12 요구사항).

### 2-4. 에디터/블루프린트 작업 (unreal-mcp 경유, best-effort)

C++ 변경만으로는 `BindWidgetOptional` 대상 위젯이 실제 WBP 애셋에 없으면 아무 효과가 없다. 연결된 Unreal Editor의 MCP 툴셋(UMGToolSet/BlueprintTools/ObjectTools/LiveCodingToolset)으로 다음을 시도한다:

- `WBP_O_PauseMenu`: Btn_KeyGuide/Btn_ToTitle 제거, Btn_ToLobby/Btn_Reset 추가(기존 버튼 복제 방식으로 스타일 유지).
- `WBP_S_InGame`: 기존 점수 TextBlock을 ProgressBar(PB_TeamMoney) + 오버레이 텍스트 구조로 교체, W_HelpPanel 자식 위젯 배치(우측 중단~하단).
- 신규 `WBP_W_HelpPanel`(부모: `UW_HelpPanel`) 생성.
- C++ 변경 후 LiveCodingToolset으로 컴파일까지 확인.

에디터 자동화 툴은 처음 써보는 것이라 신뢰도가 검증되지 않았다. 자동화가 막히는 항목이 있으면 무엇을 수동으로 마무리해야 하는지 명확히 알려준다(예: 위젯 트리 구조가 복잡해 자동 배치가 애매한 경우).

기존 감사에서 발견한 고아 블루프린트(`WBP_S_StageSelect` 등)나 `L_StageSelect.umap` 정리는 이번 작업 범위가 아니므로 건드리지 않는다.

---

## 파일 변경 목록 요약

| 파일 | 변경 내용 |
|---|---|
| `TCSessionFlow.h/.cpp` | `RestartStage()` 추가, `HostServerTravel()`에 클라이언트 로딩화면 RPC 루프 추가 |
| `TCPlayerController.h/.cpp` | `ClientShowLoadingScreen()`, `ServerReportMapLoaded()`, `ClientNotifyAllPlayersLoaded()` RPC 추가, `BeginPlay()` 스테이지 분기 수정 |
| `TCPlayerState.h/.cpp` | `bHasLoadedCurrentMap` + `SetHasLoadedCurrentMapAuthoritative()` 추가 |
| `TeamCarryGameMode.h/.cpp` | `PostLogin`/`HandleSeamlessTravelPlayer` 오버라이드, `BeginPlay()` 카운트다운 지연, `NotifyPlayerFinishedLoading()`/`AreAllConnectedPlayersLoaded()` 추가, `TotalLevelValue` 산정 |
| `TeamCarryGameState.h/.cpp` | `TotalLevelValue` 필드 추가 |
| `S_InGame.h/.cpp` | `PB_TeamMoney` 게이지로 교체, 채움 비율/텍스트 로직 |
| `O_PauseMenu.h/.cpp` | Btn_KeyGuide/Btn_ToTitle 제거, Btn_ToLobby/Btn_Reset 추가, 호스트 게이팅 구현 |
| `W_HelpPanel.h/.cpp`(신규) | 정적 도움말 패널 |
| WBP 애셋 (에디터) | 위 변경사항 반영한 위젯 트리 수정 |

---

## 검증

1. C++ 변경 후 LiveCodingToolset(또는 일반 빌드)으로 컴파일 성공 확인.
2. 에디터 PIE로 2인 이상(리슨서버 + 클라이언트 1개 이상) 세션 실행:
   - 로비 → 스테이지 진입(HostStartGame): 호스트/클라이언트 모두 S_Loading이 보이고, 모든 클라이언트의 로딩이 끝나기 전까지 인게임으로 넘어가지 않는지 확인(일부러 한쪽 로딩을 지연시켜 다른 쪽이 대기하는지 체크).
   - O_PauseMenu에서 [로비 복귀]/[리셋] 클릭 → O_Confirm 확인 → 각각 정상 동작 확인. 클라이언트 화면에서 두 버튼이 안 보이는지(호스트 전용) 확인.
   - 가구를 트럭에 실을 때 PB_TeamMoney 게이지와 텍스트("현재/전체")가 갱신되는지 확인.
   - 재접속(reconnect) 시나리오가 기존처럼 정상 동작하는지 확인(게이트 로직이 재접속 경로를 막지 않는지).
3. UI_Technical_Spec.md와 실제 동작 일치 여부 최종 확인.

---

## 착수 전 재확인 체크리스트 (다른 사람 작업 확인 후)

- [ ] 위에서 인용한 파일:라인 번호가 여전히 유효한지(다른 팀원이 같은 파일을 건드렸을 수 있음)
- [ ] `ATeamCarryGameMode`(BP_TeamCarryGameMode)의 `bUseSeamlessTravel` 실제 값 — C++에서 설정하지 않고 블루프린트 디폴트에만 있어 정적 검색으로 확인 불가했음. 에디터에서 Class Defaults 직접 확인 필요.
- [ ] `TCPlayerState`에 이미 다른 사람이 유사한 "로딩 완료" 플래그를 추가하지 않았는지
- [ ] `O_PauseMenu`/`S_InGame`의 WBP 애셋이 이 계획과 다른 방향으로 이미 수정되지 않았는지

---

## Phase 3. 게시판(Stage Select Board) 월드 UI 구현 — 완료 기록 (2026-07-14~15)

> **상태: 구현 완료, PIE로 단일 클라이언트(호스트) 상호작용 검증 완료.** 아래는 "무엇을 왜 했는가" 기록이 아니라, **.uasset(바이너리) 변경분을 머지 충돌로 잃었을 때 그대로 재현하기 위한 값 목록**이 핵심이다. C++ 변경분은 git으로 정상 추적되므로 걱정할 필요 없음 — 문제는 아래 "에디터/.uasset 변경 값" 섹션뿐이다.

### 3-1. 배경

로비의 스테이지 선택을 팝업(O_StageSelect)이 아니라 로비에 배치된 게시판 오브젝트(BP_StageSelectBoard) 앞에서, 그 오브젝트에 붙은 월드 스페이스 화면(W_StageBoardScreen)을 마우스로 직접 조작해 선택하도록 변경(UI_Technical_Spec.md v3, 4장-5·6장-9 참고).

설계 원칙(사용자 확인 완료):
- **로컬(조작자만) 처리:** 마우스 레이캐스트/호버/클릭은 상호작용한 플레이어의 `WidgetInteractionComponent`에서만 일어나고 네트워크를 타지 않는다. 아직 확정 안 한 리스트 하이라이트(`PendingSelectedStageId`)도 로컬 변수.
- **동기화되는 건 확정된 스테이지 ID 하나뿐:** 확인 버튼 → `UTCSessionFlow::SetStageSelection()` → `ATCLobbyGameState::SelectedStageId`(Replicated) → `OnSelectedStageChanged` 델리게이트 브로드캐스트 → 각 클라이언트의 `W_StageBoardScreen::RefreshDisplay()`가 텍스트/하이라이트만 갱신.
- **호스트만 조작 가능:** 리슨 서버 구조상 서버(`ServerTryInteract_Implementation`)에서 재검증하는 `Player->IsLocallyControlled()`가 호스트 자신의 폰에서만 true이므로 별도의 `IsHost()` 체크 없이 자연스럽게 호스트 전용이 됨.

### 3-2. C++ 변경 (git으로 추적됨 — 참고용 요약만)

| 파일 | 변경 내용 |
|---|---|
| `Level/Struct/TCStageSelectBoard.h/.cpp`(신규) | `ATCStageSelectBoard : AActor, ITCInteractable`. `BoardMesh`(root)/`TeleportAnchor`/`BoardScreen`(WidgetComponent) 컴포넌트. `OnInteract`가 `PC->ClientEnterBoardInteractionMode()` 호출(Client RPC이므로 리슨 서버든 아니든 항상 올바른 클라이언트로 라우팅). |
| `Player/Character/TCPlayerCharacter.h/.cpp` | `UWidgetInteractionComponent* WidgetInteraction`(Camera에 부착, Mouse 소스, 기본 비활성) 추가. `Interact()`/`ReleaseInteract()`가 `PC->IsBoardInteractionModeActive()`이면 `GrabComponent` 대신 `WidgetInteraction->Press/ReleasePointerKey(LeftMouseButton)`로 라우팅. |
| `Player/PlayerController/TCPlayerController.h/.cpp` | `ClientEnterBoardInteractionMode()`(Client RPC, 커서+WidgetInteraction 활성화), `ExitBoardInteractionMode()`(다음 틱으로 입력모드 전환을 미룸 — 이유는 3-4 참고), `IsBoardInteractionModeActive()` getter, `Input_ToggleLobbyCursor()`에 게시판 모드 가드 추가. |
| `UI/W_StageBoardScreen.h/.cpp`(신규) | 게시판 화면 위젯(`UCommonUserWidget`). `List_Stages`/`Btn_Confirm`/`Btn_Cancel`. CommonUI 화면 스택(State/Overlay)에 속하지 않는 예외 위젯 — `UMockUIController`를 거치지 않고 `ATCLobbyGameState::OnSelectedStageChanged`를 직접 구독. |
| `UI/W_StageListItem.h/.cpp`(신규) | `IUserObjectListEntry` 네이티브 구현 — 리스트 행 위젯(블루프린트 이벤트 그래프 배선 불필요). |
| `Player/Component/GrabComponent.cpp` | `TryInteract()`/`ServerTryInteract_Implementation()`가 실제로 들 수 있는 가구(`FurnitureGrabSystem` 보유)일 때만 그랩 몽타주 재생 + `GrabbedActor`로 기억하도록 수정(문/게시판 등 단순 토글형 상호작용까지 "가구를 든 상태"로 오판되던 버그 수정). |

### 3-3. 발견/수정한 버그 순서 (재발 시 참고용)

1. **`TeleportAnchor`가 판넬에서 360cm 떨어져 있었음** → 상호작용 감지 범위(캐릭터 전방 약 10~90cm, `GrabComponent::ScanBestTarget`)를 한참 벗어남 → 70cm로 조정.
2. **`BoardScreen`(월드 위젯)이 두께 10cm 불투명 패널의 정중앙(relativeLocation 0,0,0)에 파묻혀 있었음** → 화면 자체는 정상 렌더링되지만 시각적으로 안 보임 → 패널 정면 방향(로컬 X)으로 8cm 빼서 표면 앞에 위치시킴.
3. **`GrabComponent`가 게시판 상호작용도 "가구 잡기"로 취급** → 클릭 시 가구 잡기 애니메이션 재생 + 이동속도 잠금이 잘못 걸림 → `FurnitureGrabSystem` 보유 여부로 분기하도록 수정(3-2 참고).
4. **`WidgetInteractionComponent`는 매 틱 호버만 갱신할 뿐, 실제 클릭은 `PressPointerKey`/`ReleasePointerKey`를 명시적으로 호출해야 UMG에 전달됨** → 게시판 모드 중 좌클릭을 `GrabComponent` 대신 `WidgetInteraction`으로 라우팅하도록 `Interact()`/`ReleaseInteract()` 추가.
5. **확인/취소 클릭 시 무한 재진입 루프** — `ExitBoardInteractionMode()`가 확인 버튼의 `OnClicked` 콜백 스택(=`ReleasePointerKey` 처리 도중) 안에서 곧바로 `SetInputMode()`를 바꾸자, Slate가 마우스 캡처를 게임 뷰포트로 즉시 돌려주면서 아직 처리 중이던 클릭을 캐릭터의 좌클릭 입력으로 다시 잡아버려 게시판 모드가 즉시 재진입됨(로그상 진입→확인→종료가 0.3초 간격으로 반복). → 입력모드 전환 부분만 `SetTimerForNextTick`으로 미뤄 해결.
6. **`Btn_Confirm`/`Btn_Cancel`의 `Visibility`가 `SelfHitTestInvisible`로 되어 있었음** — 이게 실제 "마우스로 게시판 UI를 전혀 조작할 수 없던" 근본 원인. `SelfHitTestInvisible`은 "이 위젯 자체는 클릭을 무시하고 통과시킨다"는 뜻이라 버튼 자신에게는 잘못된 값. `Visible`로 수정.
7. **`BoardScreen`의 `DrawSize`가 정사각형(500×500)** — 실제 패널 면(가로 700cm×세로 500cm)과 비율이 안 맞아 내용이 찌그러져 보임 → `DrawSize`를 (700,500)으로 수정.

### 3-4. 에디터/.uasset 변경 값 (★ 머지 충돌 시 재현용 — 이 섹션이 핵심)

**`/Game/Developers/MinkiCho/Blueprint/Object/BP_StageSelectBoard`** (부모: `ATCStageSelectBoard`)

- `TeleportAnchor` 컴포넌트 (CDO 기본값 + L_Lobby에 배치된 `BP_StageSelectBoard_C_1` 인스턴스 모두 동일하게 적용됨):
  - `relativeLocation = (X=70, Y=0, Z=-210)`
- `BoardScreen` 컴포넌트 (CDO 기본값 + 위 인스턴스 모두):
  - `relativeLocation = (X=8, Y=0, Z=0)`
  - `drawSize = (X=700, Y=500)`
  - `widgetClass = /Game/Developers/MinkiCho/Blueprint/UI/StageSelect/WBP_W_StageBoardScreen.WBP_W_StageBoardScreen_C`
- 위 세 값의 축 의미(재현 시 헷갈리지 않도록): `BoardMesh`(루트)의 로컬 X축이 패널의 정면(면 법선) 방향이다. L_Lobby의 `BP_StageSelectBoard_C_1`는 액터 자체가 world yaw=90°이고, 수동으로 추가한 시각적 콜리전 자식 `Cube`는 그 액터 프레임 안에서 상쇄되는 자체 relativeRotation yaw=-90°를 가지고 있어 액터-로컬 X축이 곧 패널의 진짜 정면 방향이 된다(Cube의 relativeScale3D는 (7, 0.1, 5) — 가로 700cm, 두께 10cm, 높이 500cm). 패널을 다시 배치하거나 크기를 바꿀 경우 이 축 관계부터 다시 확인할 것.
- `BoardMesh`(루트): `CollisionProfileName = BlockAll`. 정적 메시는 할당하지 않음(현재 L_Lobby 인스턴스는 별도로 수동 추가한 `Cube`(StaticMeshComponent, relativeScale3D=(7,0.1,5), relativeRotation yaw=-90)가 실제 시각/콜리전을 담당) — 추후 `BoardMesh` 자체에 정식 메시를 할당해 `Cube`를 대체하는 정리 작업이 남아있음.

**`/Game/Developers/MinkiCho/Blueprint/UI/StageSelect/WBP_W_StageBoardScreen`** (부모: `UW_StageBoardScreen`)

- 위젯 트리: `ScaleBox_0` > `Overlay_0` > [`Image_41`(배경, Visibility=Visible), `VerticalBox_88`[`Txt_StageName`, `Spacer_261`, `List_Stages`(ListView, EntryWidgetClass=`WBP_StageListItem`, Fill 사이즈), `Spacer_165`, `HorizontalBox_0`[`Btn_Confirm`, `Spacer_79`, `Btn_Cancel`], `Spacer_102`]]
- `Btn_Confirm`(부모 `WBP_Btn_Apply`), `Btn_Cancel`(부모 `WBP_Btn_Cancel`): **`Visibility = Visible`** (기본/실수로 `SelfHitTestInvisible`이 되면 클릭이 전혀 안 먹힘 — 3-3의 6번 버그, 가장 재발하기 쉬운 지점이니 머지 후 최우선으로 확인).

**`/Game/Developers/MinkiCho/Blueprint/UI/StageSelect/WBP_StageListItem`** (부모: `UW_StageListItem`)

- 위젯 트리: `Overlay_31` > [`Image_49`, `HorizontalBox_178`[`SizeBox_60`[`Image_Stage`], `TextBlock_StageName`]]

**`/Game/Data/Stages/DT_Stages`** (DataTable)

- 테스트용 임시 행 `Stage_2` 추가됨: `stageId=2`, `displayName="스테이지 2 (테스트)"`, `mapPath=/Game/Maps/L_Level1`(Stage_1과 동일한 맵 재사용 — 실제 스테이지 2 레벨이 없어 동기화 테스트 목적으로만 만든 임시 행). **정식 스테이지가 아니므로 최종 빌드 전에 제거하거나 실제 스테이지 2 데이터로 교체할 것.**

### 3-5. 남은 작업

- [ ] `DT_Stages`의 `Stage_2` 임시 행 정리(제거 또는 실데이터로 교체)
- [ ] `BoardMesh`에 정식 정적 메시 할당해 수동 `Cube` 자식 대체
- [ ] `S_Lobby`의 `Txt_InteractPrompt`(게시판을 바라볼 때 뜨는 상호작용 프롬프트)가 실제로 표시되는지 미확인 — `ATCStageSelectBoard::OnFocus_Implementation()`은 구현돼 있으나 PIE에서 프롬프트 노출 자체는 아직 검증 안 됨
- [ ] 2인 이상 PIE로 "호스트만 조작 가능 + 비호스트 화면엔 결과만 동기화" 시나리오 실제 검증(현재까지는 싱글/호스트 단독 테스트만 완료)
