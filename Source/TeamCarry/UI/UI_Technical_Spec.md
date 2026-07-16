# UI_Technical_Spec_v3.md
# 이사 협동 게임 UI 최종 기획 및 기술 명세서 (개정판: S_Lobby 중심 구조)

> **개정 요약 (v1 → v2)**
> * **S_Lobby 신설:** L_Lobby의 메인 State. S_InGame처럼 캐릭터 조작이 가능한 "플레이어블 로비".
> * **S_CharacterSelect → O_CharacterSelect:** 캐릭터 외형 변경 전용 오버레이로 축소. 준비/시작 기능은 S_Lobby로 이관.
> * **S_StageSelect → O_StageSelect:** 스테이지 선택 오버레이(방장 전용)로 전환. **L_StageSelect 맵 삭제.** 미선택 시 기본 1스테이지.
> * **S_Result → O_Result:** 인게임 레벨 위의 전체화면 오버레이로 전환. [로비로 가기]/[메인 화면으로] 버튼 제공.
> * **S_Loading 신설:** 모든 레벨 트래블 구간에 표시되는 로딩 화면.
> * **접속 로그 위젯(W_SessionLog):** S_Lobby와 S_InGame에 공통 배치.
> * **레벨 구성 확정:** L_Title, L_Lobby, L_Tutorial, 스테이지 맵(L_LevelProto 등)만 레벨로 유지. 그 외 화면은 전부 오버레이.

> **개정 요약 (v2 → v3, 회의 반영)**
> * **인게임 HUD 게이지화:** 점수판(HUD_Score) 텍스트 표기를 팀 값어치 게이지(PB_TeamMoney)로 전환. 게이지 내부 텍스트로 "현재 옮긴 값어치 / 레벨 전체 목표 값어치"를 표시.
> * **W_HelpPanel 신설 (구 O_KeyGuide 팝업 폐기):** S_InGame 화면 우측 중단~하단에 상시 노출되는 세로형 패널로 전환. 조작 키 안내 + 게임 진행에 유용한 팁 문구를 함께 표시.
> * **O_PauseMenu 재구성:** Btn_KeyGuide(조작법), Btn_ToTitle(타이틀로) 제거. 그 자리에 방장 전용 **Btn_ToLobby(로비 복귀)**, **Btn_Reset(리셋)** 신설 — 둘 다 O_Confirm 재확인 필수.
> * **RestartStage() 신규:** UTCSessionFlow에 스테이지 재시작 의도 함수 추가. 현재 선택된 스테이지 맵으로 재트래블(S_Loading 경유)하여 인게임 상태를 초기화.

> **개정 요약 (v3 내부 추가, 2026-07-14 회의 반영 — 버전은 v3 유지, v4로 분리하지 않음)**
> * **O_StageSelect 오픈 방식 변경 (게시판 오브젝트 도입):** 스테이지 선택을 팝업 버튼 클릭으로 여는 대신, L_Lobby에 배치된 **BP_StageSelectBoard**(게시판 액터)와의 월드 상호작용으로 연다. 기존 **Btn_StageSelect**(방장 전용)는 오버레이를 직접 열지 않고, 방장 캐릭터를 보드 앞으로 순간이동시키는 용도로 축소된다. 보드에는 현재 선택된 스테이지를 실시간으로 보여주는 **월드 스크린(W_StageBoardScreen)** 이 부착되어, 방장이 아닌 플레이어도 로비를 돌아다니며 현재 선택 상태를 실시간으로 확인할 수 있다(4장-5, 6장-9).
> * **S_InGame 가구 개수 표기 변경:** 남은 가구 개수 단일 표기(Txt_RemainingFurniture)를 **"이동 가능한 개수 / 전체 상자 개수(파괴된 것 포함)"** 분수 표기(Txt_FurnitureCount)로 전환(4장-7).
> * **로딩 화면 동기화 문제 명세화:** `OnTravelStarted`가 호스트 로컬에서만 발화되어 다른 클라이언트에 로딩 화면이 뜨지 않던 문제와, 스테이지 맵(S_InGame) 진입 시 "전원 로딩 완료" 대기 개념이 없던 문제를 명세에 반영(2장, 4장-9). 실제 구현 순서는 `UI_v3_Implementation_Plan.md` Phase 1을 따른다.
> * **W_HelpPanel 위치/내용 조정:** 배치를 "우측 중단~하단 세로 패널"에서 **화면 우측 하단 정렬**로 변경. 콘텐츠는 실제 사용 중인 조작 키(이동/시점/점프/달리기/상호작용/던지기/시점전환/회전/줌/이모트 등)를 기준으로 갱신하며, 키보드/마우스 아이콘은 추후 확장 포인트로 남긴다(3장, 4장-15).
> * **O_PauseMenu의 S_Lobby 재사용:** S_Lobby에는 지금까지 ESC 시 [설정]에 접근할 경로가 없어 사운드 등 설정을 바꿀 수 없었다. 별도 클래스를 신설하는 대신, 기존 S_Tutorial 호출 시의 컨텍스트별 버튼 노출 분기(4장-12) 선례를 확장해 **O_PauseMenu를 S_Lobby에서도 재사용**한다. Lobby 컨텍스트에서는 Btn_Save·Btn_ToLobby·Btn_Reset을 숨기고, 로비 전용 **Btn_LeaveRoom(나가기, 전원)** 을 노출한다(4장-3, 4장-12).
> * **범위 제외 (별도 진행):** 인게임 폰트 조정, 버튼 세부 디자인(톤앤매너)은 담당자가 별도로 확정할 예정이라 이번 개정에서는 다루지 않는다.

> **개정 요약 (v3 내부 추가, 2026-07-15 — 버전은 v3 유지, v4로 분리하지 않음)**
> * **O_PauseMenu: S_InGame 중 클라이언트 개별 이탈 허용:** 기존에는 스테이지 진행 중(S_InGame) 오조작 방지를 위해 호스트·클라이언트 모두 타이틀로 나가는 경로가 아예 없었으나, 클라이언트가 진행 중인 세션에서 개인적으로 이탈할 수단이 전혀 없던 것은 별도 문제로 확인되어 **Btn_LeaveRoom(나가기)을 S_InGame 컨텍스트에도 확장**한다. 단 이 버튼은 **호스트에게는 계속 숨김 처리되고 일반 클라이언트에게만 노출**된다 — 호스트의 오조작이 파티 전체 진행에 영향을 주는 것을 막는 취지는 그대로 유지하면서, 클라이언트 개인의 이탈만 새로 허용한다. `LeaveToTitle()`은 호출한 로컬 플레이어만 세션에서 이탈시키는 동작이라(호스트가 호출할 때와 달리 세션 자체를 파기하지 않음) 다른 플레이어의 진행 중인 스테이지에는 영향을 주지 않는다(4장-12). **(아래 2026-07-16 항목에서 이 호스트 숨김 방침이 재조정된다.)**

> **개정 요약 (v3 내부 추가, 2026-07-16 — 버전은 v3 유지, v4로 분리하지 않음)**
> * **O_PauseMenu: Btn_LeaveRoom 게이팅 재조정 — S_InGame에서도 호스트 포함 전원 노출:** 위 2026-07-15 항목에서 "호스트에게는 계속 숨김"으로 정했던 방침을 재검토해 폐기한다. `LeaveToTitle()`은 애초에 호출자 권한에 따라 이미 다르게 동작한다 — 호스트가 호출하면 세션(`NAME_GameSession`) 자체가 파기되어 다른 플레이어 전원이 함께 끊기고, 클라이언트가 호출하면 본인만 이탈한다. 이 비대칭 동작이 이미 안전장치 역할을 하므로, Visibility 게이팅으로 호스트를 숨기는 대신 **Btn_LeaveRoom을 Lobby·S_InGame 컨텍스트 모두에서 호스트/클라이언트 구분 없이 노출**하고, 클릭 시 뜨는 O_Confirm 경고 문구만 호출자 권한에 따라 다르게 표시한다: 호스트에게는 "정말로 방을 나가시겠습니까? 진행 중인 스테이지가 종료되며, 모든 플레이어가 게임에서 나가게 됩니다."를, 클라이언트에게는 기존과 동일한 "정말로 방을 나가시겠습니까?"를 보여준다. 오조작 방지는 Visibility 숨김이 아니라 이 경고 문구 + O_Confirm 재확인 절차가 담당하는 것으로 취지가 바뀐다(4장-12).
> * **세이브 슬롯 시스템 구현 완료 (S_SlotSelect/O_SaveLoad 공용):** 기존에 "미구현"으로 남아 있던 슬롯 카드 UI가 고정 4슬롯(`SaveSlot_0`~`SaveSlot_3`) 구조로 실제 구현됐다. `UTCSessionFlow`에 `FSaveSlotInfo`/`GetAllSaveSlotInfos()`/`DeleteSaveSlot()`/`MakeSaveSlotName()` 신규 추가. 슬롯마다 `Switcher_X` 하위에 `Card_New_X`(빈 슬롯)/`Card_Saved_X`(저장 데이터 있음, 신규 위젯 `UW_GameSlotCard_Saved`) 쌍을 배치해 저장 존재 여부로 자동 전환한다. `ATeamCarryGameMode::SaveGame()`/`LoadGame()`도 고정 슬롯("TCGameSave") 대신 세션이 선택한 슬롯을 사용하도록 변경되고, 인게임 O_SaveLoad에서 다른 슬롯에 저장하면(`SaveGameToSlot()`) 그 슬롯이 세션의 활성 슬롯으로 갱신된다(4장-2, 4장-14, 5장, 7장).
> * **BP_StageSelectBoard 상호작용 확장:** 근접 시 아웃라인 하이라이트(CustomDepth) 추가. 게시판 클릭 모드 중 기존 마우스 클릭(WidgetInteractionComponent)에 더해 **키보드 리스트 탐색**(`IA_BoardListUp`/`IA_BoardListDown` → `W_StageBoardScreen::NavigateStageSelection()`)이 새로 지원된다. 클릭 모드 진입 시 마우스를 따라다니는 전용 커서 위젯도 추가됐다(Slate 기본 소프트웨어 커서 미표시 우회)(4장-5).

---

## 1. UI 전체 구조도 (화면 흐름도)

### 1-1) 레벨(맵)과 State 매핑

UI State는 단일 맵 안에서만 전환되지 않는다. 화면 전환은 두 종류로 구분한다.
* **화면 교체(ReplaceState):** 같은 맵 안에서 UMockUIController가 위젯만 교체.
* **레벨 트래블(ServerTravel/ClientTravel):** UTCSessionFlow(2장)가 맵 자체를 이동. 도착한 맵의 GameMode/PlayerController가 해당 State를 다시 띄운다. **모든 레벨 트래블 구간에는 S_Loading이 표시된다.**

| 맵 | 포함 State | 주요 오버레이 | 진입 방식 |
| :--- | :--- | :--- | :--- |
| **L_Title** | S_Boot, S_MainMenu, S_SlotSelect | O_JoinRoom, O_Settings, O_Confirm | 앱 실행 / LeaveToTitle() |
| **L_Lobby** | S_Lobby | O_CharacterSelect, O_StageSelect, O_Confirm, **O_PauseMenu(Lobby 컨텍스트로 재사용, v3 내부 신규)** | 세션 생성(호스트)·조인(클라) 직후 트래블 / HostReturnToLobby() |
| **L_Tutorial** | S_Tutorial | O_PauseMenu 계열 | HostStartGame() — 새 게임 |
| **스테이지 맵 (L_LevelProto 등)** | S_InGame | O_PauseMenu 계열, **O_Result** | HostStartGame() — 이어하기 (O_StageSelect에서 선택한 맵, 기본 1스테이지) |
| *(전환 구간)* | **S_Loading** | — | 모든 레벨 트래블 시작~완료 사이 표시 |

※ **L_StageSelect 맵은 삭제되었다.** 스테이지 선택은 L_Lobby 내 O_StageSelect 오버레이가 담당한다.

※ **BP_StageSelectBoard(신규, 게시판 액터):** L_Lobby 월드에 배치되는 액터로, State/Overlay 목록에는 포함되지 않는다. 방장이 이 액터에 상호작용하면 O_StageSelect가 열리며, 액터에 부착된 월드 스크린 위젯(W_StageBoardScreen)은 현재 선택된 스테이지를 전원에게 실시간으로 보여준다(4장-5, 6장-9).

맵 경로는 UTCSessionFlow의 Config 프로퍼티(TitleMapPath, LobbyMapPath, TutorialMapPath)와 스테이지 데이터(2장·5장의 FStageInfo)로 DefaultGame.ini에서 덮어쓸 수 있다. (StageSelectMapPath 프로퍼티는 제거.)

### 1-2) 흐름도

범례: `──▶` 화면 교체/오버레이(ReplaceState·Push/Pop) / `══▶` 레벨 트래블(UTCSessionFlow 경유, **구간 중 S_Loading 표시**)

```
[S_Boot] 로고/인트로  ※ 미구현 (전용 C++ 클래스 없음, EE_UIState에만 존재)
   │
   ▼
[S_MainMenu] 타이틀 ──게임 종료(버튼/ESC)──▶ (O_Confirm) ──예──▶ 앱 종료
   ├─ 게임 시작 ───────────────▶ (O_JoinRoom) 통합 접속 팝업
   ├─ 옵션 ────────────────────▶ (O_Settings)
   └─ 크레딧 ──────────────────▶ [크레딧 스크롤] ──Esc──▶ 복귀
        │
(O_JoinRoom) ──취소(ESC)──▶ 팝업 닫기
   ├─ 방 만들기 버튼 선택 ────────▶ [S_SlotSelect] (세이브 슬롯 관리)
   └─ 방 참가 + 코드 입력 ══JoinRoomByCode()══▶ [S_Loading] ══▶ [L_Lobby: S_Lobby]
        │                    (실패 시 OnSessionPhaseChanged(Failed) → 에러 모달)
[S_SlotSelect] 게임 선택 슬롯 ──뒤로(ESC)──▶ [S_MainMenu]
   ├─ 기존 데이터 삭제 버튼 클릭 ─▶ (O_Confirm: 재확인) ─▶ 기존 데이터 삭제
   ├─ 데이터 슬롯 선택(이어하기) ─▶ SetSaveSelection(슬롯, true) ──┐
   └─ 빈 슬롯 선택(새 게임) ─▶ SetSaveSelection(슬롯, false) ─────┤
        │                       │ HostCreateRoom() = 세션 생성 후 로비로 트래블
        ▼                       ▼
                    [S_Loading] ══▶ [L_Lobby: S_Lobby]

[L_Lobby: S_Lobby] 플레이어블 로비 (캐릭터 조작 가능 + 접속 로그 W_SessionLog)
   ├─ [캐릭터 선택] (전원) ──▶ (O_CharacterSelect) ──ESC/닫기──▶ S_Lobby 복귀
   ├─ [스테이지 선택] (방장 전용, Btn_StageSelect) ──▶ 방장 캐릭터를 BP_StageSelectBoard 앞으로 순간이동 (오버레이를 직접 열지 않음)
   ├─ (BP_StageSelectBoard 상호작용, 방장 전용) ──▶ (O_StageSelect)
   │       ├─ 확인 ─▶ SetStageSelection(스테이지) 후 닫기 (보드의 W_StageBoardScreen도 실시간 갱신되어 전원에게 노출)
   │       └─ 취소/ESC ─▶ 선택 변경 없이 닫기 (미선택 시 기본 1스테이지 유지)
   ├─ [준비/취소] (참가자) ─▶ Ready 토글 (Server RPC → ATCPlayerState 복제)
   ├─ [게임 시작] (방장 전용, 전원 준비완료 시 활성) ──HostStartGame()──┐
   │       ├─ [새 게임 방] ══▶ [S_Loading] ══▶ [L_Tutorial: S_Tutorial]  │
   │       └─ [이어하기 방] ══▶ [S_Loading] ══▶ [선택 스테이지 맵: S_InGame]
   │                                            (O_StageSelect 미사용 시 1스테이지)
   │                                            (스테이지 맵 진입은 전원 로딩 완료 후 동시 진입, 2장·4장-9)
   └─ ESC/뒤로 ──▶ (O_PauseMenu, Lobby 컨텍스트) ├─[설정] ─▶ (O_Settings)
                                                └─[나가기] ─▶ (O_Confirm: 방 나가기) ══LeaveToTitle()══▶ [S_Loading] ══▶ [S_MainMenu]

[L_Tutorial: S_Tutorial] ──마지막 Step 완료/건너뛰기──▶ HostReturnToLobby()
                                        ══▶ [S_Loading] ══▶ [L_Lobby: S_Lobby]

[스테이지 맵: S_InGame] 인게임 HUD (+ 접속 로그 W_SessionLog + W_HelpPanel 상시 표시)
   │            ──ESC──▶ (O_PauseMenu) ──[수동 저장](방장 전용)──▶ (O_SaveLoad)
   │                       ├─[설정]──▶ (O_Settings)
   │                       ├─[로비 복귀](방장 전용) ─▶ (O_Confirm: 로비로 복귀) ══HostReturnToLobby()══▶ [S_Loading] ══▶ [L_Lobby: S_Lobby]
   │                       ├─[리셋](방장 전용) ─▶ (O_Confirm: 스테이지 초기화) ══RestartStage()══▶ [S_Loading] ══▶ [선택 스테이지 맵: S_InGame] (재진입)
   │                       └─ESC (O_PauseMenu 닫기)──▶ 인게임 HUD 복귀
   └─ 게임 시간 종료 / 가구 전량 운반 완료
        │
        ▼
(O_Result) 최종 결과 오버레이 — 인게임 기능 정지, O_Result 로직만 수행
   ├─ [로비로 가기] ══HostReturnToLobby() (세션 유지)══▶ [S_Loading] ══▶ [L_Lobby: S_Lobby]
   └─ [메인 화면으로] ══LeaveToTitle() (세션 파기)══▶ [S_Loading] ══▶ [S_MainMenu]
```

※ **튜토리얼 종료 후 목적지 변경:** 기존에는 L_StageSelect로 직행했으나, 해당 맵이 삭제되었으므로 **로비(S_Lobby)로 복귀**한다. 이후 방장이 O_StageSelect로 스테이지를 고르고 다시 [게임 시작]으로 진입한다. (튜토리얼 완료 시 세이브에 완료 플래그를 기록하여, 같은 방에서 다음 [게임 시작]은 이어하기 경로를 타도록 한다.)

---

## 2. 세션 흐름 계층 (UTCSessionFlow)

UI와 Steam 세션(UTCGameInstance) 사이의 단일 바인딩 계층(GameInstance 서브시스템). **UI 위젯은 세션 API나 ServerTravel을 직접 호출하지 않고, 이 서브시스템의 "의도(intent)" 함수만 호출한다.** 내부에서 세션 API 호출 + 레벨 트래블 오케스트레이션 + 단계 통지 + **로딩 화면(S_Loading) 표시 지시**를 수행한다.

### 의도 함수
| 함수 | 호출 주체 | 동작 |
| :--- | :--- | :--- |
| `SetSaveSelection(SlotName, bContinue)` | S_SlotSelect | 세이브 슬롯/이어하기 여부 확정 (세션 수명 동안 유지) |
| `SetStageSelection(StageId)` **(신규)** | O_StageSelect | 플레이할 스테이지 확정 (호스트 전용, 세션 수명 동안 유지). 한 번도 호출되지 않으면 기본 1스테이지 |
| `HostCreateRoom()` | S_SlotSelect | 세션 생성 → 성공 콜백에서 L_Lobby로 ServerTravel |
| `JoinRoomByCode(RoomCode)` | O_JoinRoom | 세션 검색 → 조인. 성공 시 L_Lobby 합류 |
| `HostStartGame()` **(동작 변경)** | S_Lobby | 새 게임=L_Tutorial로 ServerTravel / 이어하기=**선택된 스테이지 맵**(미선택 시 1스테이지)으로 직행 ServerTravel |
| `HostReturnToLobby()` **(역할 확대)** | S_Tutorial 완료, O_Result, **O_PauseMenu(Btn_ToLobby, 신규)** | 세션 유지한 채 L_Lobby(S_Lobby)로 복귀 (호스트 전용). O_PauseMenu 경유 시에는 `bIsGameFinished` 여부와 무관하게 진행 중인 스테이지를 즉시 이탈한다 |
| `RestartStage()` **(신규)** | O_PauseMenu(Btn_Reset, 방장 전용) | 세션 유지한 채 현재 선택된 스테이지 맵으로 재트래블(ServerTravel) — 가구 배치·팀 값어치·타이머 등 인게임 상태 초기화 (호스트 전용) |
| `LeaveToTitle()` | 각 O_Confirm, O_Result | 세션 파기 후 L_Title 복귀 (호스트/클라 공통) |

**제거된 함수:** `HostTravelToStage()`(HostStartGame에 흡수), `HostReturnToStageSelect()`(L_StageSelect 삭제로 폐기).

### 스테이지 데이터 (확장 열어두기 — 현재 스테이지 데이터 미구현)
* `FStageInfo` (USTRUCT, 도입 예정): `int32 StageId`, `FText DisplayName`, `FString MapPath`, (확장: 썸네일, 해금 조건, 별 조건 등).
* 스테이지 목록은 DataTable(`DT_Stages`) 또는 UTCSessionFlow의 Config 배열로 정의하며 DefaultGame.ini로 덮어쓸 수 있다.
* **현재는 스테이지 데이터가 없으므로**, StageId=1 → L_LevelProto 하나만 하드코딩된 기본 항목으로 두고, O_StageSelect UI와 SetStageSelection API는 목록이 늘어나도 수정 없이 동작하도록 StageId 기반으로 설계한다.
* `GetSelectedStageId()` / `GetSelectedStageMapPath()`: 현재 선택 스테이지 조회. 미선택 시 1스테이지 반환.

### 단계 통지 (로딩/에러 UI 연동)
* `ETCSessionPhase`: Idle / Creating(방 생성 중) / Hosting(호스트 성공) / Searching(검색 중) / Joining(조인 시도) / Joined(조인 성공) / Failed(실패, 메시지 동반)
* `OnSessionPhaseChanged(Phase, Message)` 델리게이트를 UI가 구독하여 로딩 스피너·에러 토스트/모달을 표시한다. Failed 시 Message에 실패 사유가 담긴다.
* `OnTravelStarted(TargetMapPath)` **(신규)**: 레벨 트래블 직전에 Broadcast. UMockUIController가 이를 받아 `ReplaceState(EE_UIState::Loading)`으로 S_Loading을 띄운다. 트래블 완료 후에는 도착 맵의 GameMode/PlayerController가 목적지 State를 띄우면서 자연히 교체된다.
  * **알려진 문제 및 수정 방향 (신규, 회의 반영):** 위 브로드캐스트는 `IsHost()`인 프로세스에서만 로컬로 발화되어, 같은 세션의 다른 클라이언트는 엔진 seamless travel로 조용히 맵이 이동될 뿐 S_Loading을 보지 못하고 곧장 목적지 화면으로 순간이동한 것처럼 보이는 문제가 있다. 해결책은 서버 트래블 직전 모든 `ATCPlayerController`에 로딩 화면 표시를 지시하는 Client RPC를 추가하는 것이며, 상세 구현 순서는 `UI_v3_Implementation_Plan.md` Phase 1-A를 따른다.
  * **스테이지 맵(S_InGame) 진입 전원 대기 게이트 (신규, 회의 반영):** 로비/타이틀 등 다른 목적지와 달리, 스테이지 맵 진입만큼은 "내 화면의 로딩이 끝났다"만으로 S_InGame을 띄우지 않고 **세션 내 모든 플레이어가 로딩을 마칠 때까지 대기한 뒤 동시에 S_InGame으로 전환**한다(로비는 참가자가 서로 다른 시점에 합류하므로 "전원 대기" 개념 자체가 적용되지 않는다). 상세 구현 순서는 `UI_v3_Implementation_Plan.md` Phase 1-B를 따른다.

### 조회 함수
* `IsHost()`: 이 인스턴스가 서버 권위(호스트)인지 판정. **UI에서 호스트 여부 분기(스테이지 선택·게임 시작 버튼 노출 등)는 이 함수를 사용한다** (FPlayerInfo.bIsHost 같은 필드는 두지 않는다).
* `IsContinueMode()` / `GetSelectedSlotName()`: 선택된 세이브 슬롯 정보 조회.
* `GetRoomCode()`: 호스트가 광고 중인 방 코드. 단, 클라이언트 UI의 방 코드 표시는 ATCLobbyGameState의 복제 변수 RoomCode를 사용한다(5장).

---

## 3. 화면(State) 목록 및 공통 오버레이 명세

### 화면(State) 목록
| State 명칭 | 종류 | 소속 맵 | 진입 경로 | 일시정지 | 비고 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **S_Boot** | 풀스크린 | L_Title | 앱 실행 | X | 로고/인트로 연출 (※ 미구현) |
| **S_MainMenu** | 풀스크린 | L_Title | Boot 완료 | X | 메인 타이틀 화면 |
| **S_SlotSelect** | 풀스크린 | L_Title | O_JoinRoom에서 방 만들기 선택 | X | 가로형 카드 배치 (이어/새 게임 통합) |
| **S_Lobby** **(신규)** | HUD형 풀스크린 | L_Lobby | 세션 생성/조인/HostReturnToLobby() | X | **플레이어블 로비.** 캐릭터 조작 가능 + 로비 버튼 4종 + 접속 로그 |
| **S_Tutorial** | 풀스크린 | L_Tutorial | 새 게임 로비 시작 직후 | O | 수동 저장 비활성. 완료 시 로비 복귀 |
| **S_InGame** | 풀스크린 | 스테이지 맵 | HostStartGame() 이어하기 | O | 코어 루프 HUD + 접속 로그 |
| **S_Loading** **(신규)** | 풀스크린 | (전환 구간) | 모든 레벨 트래블 | X | 배경 + 로딩 바 + 게이지 텍스트 |

**State에서 제외된 항목:** S_CharacterSelect → **O_CharacterSelect**, S_StageSelect → **O_StageSelect**, S_Result → **O_Result** (4장 참조).

### 공통 컴포넌트 명세 (Overlay)
* **통합 라우팅 규칙:** 풀스크린은 교체(Replace), 오버레이는 누적(Push/Pop) 방식. ESC는 '한 단계 뒤로/닫기' 통일.
* **O_JoinRoom:** 게임 시작 시 호출되는 통합 방 생성/참가 모달 팝업.
* **O_CharacterSelect (신규, 구 S_CharacterSelect):** 캐릭터 **외형 변경 전용** 오버레이. 준비/시작 기능 없음.
* **O_StageSelect (신규, 구 S_StageSelect):** 방장 전용 스테이지 선택 오버레이. 확인/취소/ESC. **(v3 내부 개정)** UI 버튼이 아니라 BP_StageSelectBoard(게시판 액터)와의 월드 상호작용으로 연다(4장-5).
* **O_Result (신규, 구 S_Result):** 게임 종료 시 인게임 레벨 위에 뜨는 전체화면 결과 오버레이.
* **O_Confirm:** 강제 모달 팝업. 기본 포커스는 '아니오'에 위치하여 오조작 방지.
* **O_Settings:** 오디오, 비디오, 키보드/패드 설정. 비디오 변경 시 15초 카운트다운 복구 로직.
* **O_PauseMenu:** S_InGame, S_Tutorial, **S_Lobby(v3 내부 신규)** 에서 ESC로 호출. 호출 컨텍스트(Lobby/Tutorial/InGame)에 따라 노출 항목이 다르다(4장-12).
  * 항목: [계속하기], [설정], [수동 저장], **[로비 복귀]**, **[리셋]**, **[나가기](Lobby·S_InGame 공통, 호스트/클라이언트 모두 노출, v3 내부 신규 — 2026-07-16 재조정)** (v3 개정 — [조작법], [타이틀로 돌아가기] 제거, 4장-12 참고)
  * 권한: 멀티플레이 동기화를 위해 [수동 저장]·[로비 복귀]·[리셋]은 호스트(방장) 전용. 튜토리얼 맵에서는 [수동 저장] 강제 비활성화, [로비 복귀]·[리셋]은 노출하지 않음. **Lobby 컨텍스트에서는 [수동 저장]·[로비 복귀]·[리셋]을 노출하지 않고 [나가기]를 노출한다. S_InGame 컨텍스트에서도 [나가기]는 호스트/클라이언트 구분 없이 노출된다(2026-07-16 재조정, 4장-12) — 오조작 방지는 Visibility 숨김이 아니라 클릭 시 뜨는 O_Confirm의 경고 문구(호스트에게는 세션 파기 경고를 별도 표시)가 담당한다.**
  * [로비 복귀]·[리셋]·**[나가기]** 는 클릭 시 반드시 O_Confirm 재확인 후 실행된다(오조작 방지).
* **O_SaveLoad:** O_PauseMenu에서 [수동 저장] 선택 시 호출. 현재 상태를 슬롯에 덮어쓰거나 빈 슬롯에 기록.

**제거된 오버레이:** O_KeyGuide(구 조작법 안내 팝업) — S_InGame에 상시 노출되는 **W_HelpPanel**(아래) 하위 위젯으로 대체되어 Push/Pop 방식의 팝업 자체가 폐기되었다.

### 공통 하위 위젯 (신규)
* **W_SessionLog (접속 로그 텍스트 칸):** 방에 접속/이탈한 유저 등 세션 이벤트 로그를 표시하는 단순 텍스트 위젯(버튼과 같은 급의 단순 위젯, `UCommonUserWidget` 상속). **S_Lobby와 S_InGame에 동일하게 배치**한다. UMockUIController의 `OnSessionLogAdded` 델리게이트를 구독하여 줄 단위로 누적 표시(최대 N줄 유지, 오래된 줄 제거).
* **W_HelpPanel (인게임 도움말/조작 팁 패널 — 신규, 구 O_KeyGuide 대체):** S_InGame **화면 우측 하단에 가지런히 정렬 배치**되어 상시 노출되는 목록형 위젯(`UCommonUserWidget` 상속) **(v3 내부 개정 — 기존 "우측 중단~하단 세로 패널" 배치안에서 변경)**. 조작 키 안내와 게임 진행에 유용한 팁 문구를 줄 단위로 표시한다. 팝업이 아니므로 게임 입력을 차단하지 않으며 열고 닫는 개념이 없다. 콘텐츠는 정적이므로 델리게이트 구독 없이 위젯 내부(Blueprint)에 직접 목록을 구성한다(4장-15 참고).
* **W_StageBoardScreen (게시판 월드 스크린 — 신규):** BP_StageSelectBoard 액터에 부착되는 월드 스페이스 위젯. 현재 선택된 스테이지를 실시간으로 표시하며, CommonUI 화면 스택(State/Overlay)에 속하지 않는 예외 위젯이다(6장-9, 4장-5 참고).

---

## 4. 화면별 상세 기술 명세

### 1) S_MainMenu (타이틀 화면)
* **역할:** 게임의 시작점.
* **구성:** 배경 루프, 로고, 리스트 버튼(Btn_Start, Btn_Options, Btn_Credits, Btn_Quit).
* **입력 라우팅:** Btn_Start 클릭 시 O_JoinRoom 모달 팝업을 호출(PushOverlay).

### 2) S_SlotSelect (게임 선택 슬롯)
* **역할:** 세이브 데이터 진입 및 관리 (호스트 권한).
* **구성 (구현 완료 — 기존 "가로형 카드 + 임시 테스트 버튼" 프로토타입에서 전환):** 고정 4슬롯(`Switcher_0`~`Switcher_3`), 각 Switcher 하위에 `Card_New_X`(빈 슬롯 카드)/`Card_Saved_X`(저장 데이터 있는 슬롯 카드, `UW_GameSlotCard_Saved`) 쌍이 배치된다. `NativeConstruct()`에서 `UTCSessionFlow::GetAllSaveSlotInfos()`(슬롯 0~3의 `UGameplayStatics::DoesSaveGameExist` 스캔 결과, `FSaveSlotInfo` 배열)를 조회해, 슬롯별로 `bHasSaveData` 값에 따라 해당 Switcher를 Card_New/Card_Saved 중 하나로 전환한다(`RefreshSlotCards()`). 기존 임시 테스트 버튼(`Btn_TempEmptySlot`)은 제거됐다.
  * ※ 남은 미구현 요구사항: 카드 표시용 부가 메타데이터(맵 썸네일, 진행도 요약, 마지막 플레이 **날짜**, CurrentTeamMoney)는 여전히 UTCSaveGame에 없다. 다만 `FSaveSlotInfo.LastPlayedStage`(마지막 플레이 **스테이지**)는 이제 조회 가능하다(5장 참고).
* **선택 로직 (UTCSessionFlow 연동):**
  * **Card_Saved_X 클릭(이어하기):** O_Confirm("이어하기" / "이 게임을 이어하시겠습니까?") 확인 후 `ConfirmSlotAndCreateRoom(UTCSessionFlow::MakeSaveSlotName(SlotIndex), true)` → 내부에서 `SetSaveSelection(슬롯명, true)` 호출 후 `HostCreateRoom()`으로 Continue 모드 방 생성.
  * **Card_New_X 클릭(새 게임):** O_Confirm("새 게임" / "새로운 게임을 생성하시겠습니까?") 확인 후 `ConfirmSlotAndCreateRoom(UTCSessionFlow::MakeSaveSlotName(SlotIndex), false)` → `SetSaveSelection(슬롯명, false)` 호출 후 `HostCreateRoom()`으로 NewGame 모드 방 생성.
  * 방 생성 성공 시 세션 계층이 L_Lobby로 ServerTravel한다 (구간 중 S_Loading 표시).
* **삭제 로직 (구현 완료):** Card_Saved_X 내부의 Btn_Delete 클릭 시 `UW_GameSlotCard_Saved::OnDeleteRequested(SlotName)` 델리게이트가 브로드캐스트되고, S_SlotSelect가 이를 구독해 O_Confirm("저장 데이터 삭제" / "정말로 이 저장 데이터를 삭제하시겠습니까? 되돌릴 수 없습니다.") 확인 후 `UTCSessionFlow::DeleteSaveSlot(SlotName)`(`UGameplayStatics::DeleteGameInSlot`)을 호출한다. 삭제 후 `RefreshSlotCards()`로 해당 슬롯이 Card_New로 즉시 전환된다.

### 3) S_Lobby (플레이어블 로비)
* **역할:** 모든 플레이어의 집결지(L_Lobby)이자 게임 준비의 허브. **S_InGame처럼 캐릭터를 직접 조작하며 돌아다닐 수 있다.**
* **구성:**
  * **로비 버튼 (호스트 = 4개 / 일반 클라이언트 = 2개):**
    | 버튼 | 노출 대상 | 동작 |
    | :--- | :--- | :--- |
    | Btn_CharacterSelect (캐릭터 선택) | 전원 | O_CharacterSelect를 PushOverlay |
    | Btn_StageSelect (스테이지 선택) | **방장 전용** | **(v3 내부 개정)** O_StageSelect를 직접 열지 않고, `GetActorOfClass(ATCStageSelectBoard)`로 씬 유일의 보드 액터(BP_StageSelectBoard)를 찾아 그 액터의 전용 앵커 컴포넌트(TeleportAnchor) 위치로 방장 캐릭터를 순간이동(Teleport)시킨다. 실제 O_StageSelect는 텔레포트 후 보드와의 기존 Interact 상호작용으로 연다(4장-5) |
    | Btn_Ready (준비/취소) | 전원(참가자 토글) | Ready 상태 토글 — 구 S_CharacterSelect의 Btn_Ready 역할 승계 |
    | Btn_Start (게임 시작) | **방장 전용** | HostStartGame() — 구 S_CharacterSelect의 Btn_Start 역할 승계 |
    * 방장 여부 판정은 `UTCSessionFlow::IsHost()`로 분기하여, 일반 클라이언트에게는 Btn_StageSelect·Btn_Start를 숨긴다(Collapsed).
  * **W_SessionLog (접속 로그 텍스트 칸):** 방에 접속한 유저에 대한 로그(입장/퇴장 등)를 표시하는 단순 텍스트 위젯(3장).
  * 방 코드 표시(Txt_RoomCode), 플레이어 준비 상태 표시(월드 내 캐릭터 머리 위 마커 또는 간이 리스트 — 프로토타입 재량).
* **입력 모델:** S_Lobby 자체는 HUD처럼 동작하여 **게임 입력(캐릭터 조작)을 막지 않는다.** 로비 버튼은 지정 키(ESC 메뉴 또는 Tab 등)나 화면 상 커서로 접근한다. O_CharacterSelect / O_StageSelect가 Push되는 순간에만 UI 전용 입력으로 전환된다(GetDesiredInputConfig, 6장).
* **로비 상태 동기화 (복제 모델 — v1의 S_CharacterSelect 모델을 그대로 승계):**
  * 각 플레이어의 준비 상태(`bIsReady`)와 슬롯 번호(`LobbySlotIndex`)는 **ATCPlayerState**에 복제된다. 쓰기는 서버 권위 전용(컨트롤러의 Server RPC 경유), 클라는 복제값을 읽기만 한다.
  * 위젯은 **ATCLobbyGameState**의 `OnLobbyPlayersChanged`를 구독하고, 이벤트 수신 시 PlayerArray를 다시 읽어 표시를 갱신한다. 입퇴장·Ready·복제값 변경이 모두 이 한 경로로 통지된다.
  * 방 코드는 ATCLobbyGameState의 복제 변수 `RoomCode`로 클라이언트에도 표시된다.
  * 규칙: 1P 호스트 고정. 중도 이탈 시 슬롯 번호 유지. 방 코드로 친구 초대.
* **접속 로그 생성 경로:** GameMode의 PostLogin/Logout에서 ATCLobbyGameState(또는 인게임 GameState)의 로그 배열에 항목 추가(복제/RepNotify) → 클라 도착 시 UMockUIController의 `OnSessionLogAdded(FText)`를 Broadcast → W_SessionLog 갱신. (5장·7장 참고)
* **시작 로직:** `ATCLobbyGameState::AreAllPlayersReady()` 충족 시 방장 Btn_Start 활성화. 클릭 시 `HostStartGame()` 호출, 카운트다운 없이 즉시 전환.
  * 새 게임 방 → L_Tutorial. / 이어하기 방 → O_StageSelect에서 선택한 스테이지 맵(미선택 시 기본 1스테이지 = L_LevelProto).
* **뒤로가기:** ESC → **(v3 내부 개정)** O_PauseMenu를 Lobby 컨텍스트로 PushOverlay(4장-12) → [나가기] 선택 시 O_Confirm(방 나가기) → 확인 시 `LeaveToTitle()`. (로비 버튼 4종에는 별도의 "나가기" 버튼이 없다 — 나가기는 ESC 메뉴를 거쳐야만 접근 가능하며, S_InGame이 ESC로만 O_PauseMenu에 접근하는 것과 동일한 규칙이다.)

### 4) O_CharacterSelect (캐릭터 외형 선택 오버레이)
* **역할:** **캐릭터의 외형을 변경하는 기능만** 가진 오버레이. 준비/시작 기능은 S_Lobby로 이관되어 이 오버레이에는 없다.
* **호출:** S_Lobby의 Btn_CharacterSelect 클릭 시 PushOverlay.
* **구성:** 외형(캐릭터/스킨) 목록, 미리보기, Btn_Close(닫기).
* **동작:**
  * 외형 선택 시 CharacterIndex를 Server RPC로 서버에 전달 → ATCPlayerState에 복제 → `OnLobbyPlayersChanged`로 전원에게 반영(로비 월드의 내 캐릭터 외형도 갱신).
  * **닫기:** Btn_Close 또는 ESC → `PopCurrentOverlay()`로 S_Lobby 복귀. (선택은 즉시 적용 방식이므로 별도 확인 버튼 없음.)
  * 활성화 중에는 GetDesiredInputConfig로 게임 입력 차단(UI 전용 입력).

### 5) O_StageSelect (스테이지 선택 오버레이) — 방장 전용, BP_StageSelectBoard 연동
* **역할:** 플레이할 스테이지를 고르는 오버레이. **방장만 열 수 있다.**
* **호출 (v3 내부 개정 — 게시판 오브젝트 도입):** 팝업 버튼 클릭이 아니라 L_Lobby에 배치된 **BP_StageSelectBoard**(게시판 액터)와의 월드 상호작용(Interact)으로 연다.
  * **BP_StageSelectBoard:** L_Lobby 월드 내 고정 위치에 배치되는 액터. **씬에 단 하나만 존재한다고 가정한다**(다중 보드 지원은 확장 개방).
  * **상호작용 시스템 (v3 내부 결정 — 기존 시스템 재사용):** 가구 상호작용에 이미 쓰이는 `IA_Interact` 입력 액션과 `OnInteractTargetChanged(AActor* Target, FString Key)` 델리게이트(4장-7, W_FurnitureStatus와 동일 경로)를 그대로 재사용한다. 별도의 트리거 볼륨·입력 시스템을 새로 만들지 않는다. 근접 시 `OnInteractTargetChanged`가 Broadcast되고, `IA_Interact` 입력 시 `PushOverlay("O_StageSelect")`를 호출한다. 기존 O_StageSelect의 Push/Pop, 입력 차단(GetDesiredInputConfig), 확인/취소 로직은 그대로 유지되며 **호출 경로만** 변경된다.
    * **프롬프트 표시 위젯은 별도 필요:** W_FurnitureStatus는 가구 전용(내구도 게이지 포함) 위젯이라 "좌클릭 - 스테이지 선택"(실제 IMC 기준 상호작용 키는 E가 아니라 좌클릭 — 4장-15 참고) 같은 보드 프롬프트에 그대로 재사용할 수 없다. S_Lobby HUD에 `OnInteractTargetChanged`를 구독하는 경량 프롬프트 텍스트(예: `Txt_InteractPrompt`, Target 타입에 따라 문구만 다르게 표시)를 별도로 둔다 — 재사용되는 것은 입력 액션·델리게이트 경로뿐이며, 표시 위젯 자체는 화면(S_Lobby vs S_InGame)마다 별개다.
  * **방장 판정 (v3 내부 결정):** `UTCSessionFlow::IsHost()`가 false인 플레이어에게는 상호작용 프롬프트 자체가 노출되지 않는다(비활성 표시가 아니라 완전히 숨김) — 오버레이 자체가 열리지 않는다.
  * **포커스 시각 피드백 (신규, 구현 완료):** `OnFocus_Implementation()`/`OnUnfocus_Implementation()`이 보드 액터에 붙은 모든 `UStaticMeshComponent`에 CustomDepth-Stencil 아웃라인(스텐실 값 1)을 켜고 끈다(ATCMapInteractable/InteractableDoor와 동일한 포스트프로세스 아웃라인 패턴). `SetRenderCustomDepth`는 로컬 렌더 플래그라 리플리케이트되지 않으므로, "상호작용 가능한 오브젝트" 시각 표시는 `IsHost()` 게이팅과 무관하게 근접한 모든 플레이어에게 동일하게 보인다(실제 상호작용 자체는 여전히 방장 전용, 위 항목 참고).
  * **게시판 클릭 모드 내 선택 방식 (신규, 구현 완료 — 마우스 + 키보드 병행):** `IA_Interact` 입력으로 `ATCStageSelectBoard::OnInteract_Implementation()`(서버)이 실행되면 `ATCPlayerController::ClientEnterBoardInteractionMode(Board)`가 Client RPC로 호출되어 마우스 커서를 노출하고 캐릭터의 `WidgetInteractionComponent`로 W_StageBoardScreen(월드 스페이스 위젯)의 목록/확인/취소를 클릭할 수 있게 한다. **여기에 더해**, 이 모드 중에는 `IA_BoardListUp`/`IA_BoardListDown` 입력으로도 리스트를 탐색할 수 있다 — `ATCPlayerController::Input_BoardListUp/Down()`이 진입 시 저장해 둔 대상 보드(`ActiveBoard`)의 `GetBoardScreenWidget()`을 찾아 `UW_StageBoardScreen::NavigateStageSelection(±1)`을 호출하며, 마우스 클릭과 동일한 경로(`HandleStageItemClicked`)로 선택을 반영해 하이라이트 테두리까지 동일하게 갱신된다. 클릭 모드 중에는 마우스 위치를 매 프레임 따라다니는 별도 커서 위젯(빨간 점, `PlayerTick()`에서 갱신)도 표시된다 — Slate 기본 소프트웨어 커서가 이 프로젝트 창 설정에서 렌더링되지 않아 대체한 것.
  * **Btn_StageSelect와의 관계 (v3 내부 결정 — 액터 참조/텔레포트 목적지):** S_Lobby의 Btn_StageSelect(방장 전용)는 더 이상 오버레이를 직접 열지 않고, 방장 캐릭터를 BP_StageSelectBoard 앞 지정 위치로 순간이동시키는 편의 기능으로 축소된다(4장-3). BP_StageSelectBoard는 신규 네이티브 베이스 클래스(예: `ATCStageSelectBoard`)의 블루프린트 자식으로 만들고(BP_ 접두는 블루프린트 에셋, 네이티브 클래스는 프로젝트 관례상 A 접두 — TCPlayerState 등과 동일), 클릭 시 `UGameplayStatics::GetActorOfClass(this, ATCStageSelectBoard::StaticClass())`로 씬의 유일한 보드 액터를 검색해 액터에 부착된 전용 `USceneComponent`(예: `TeleportAnchor`)의 위치/회전값으로 캐릭터를 텔레포트한다. 실제 오버레이는 텔레포트 후 보드와의 상호작용(위 `IA_Interact`)으로 연다.
* **W_StageBoardScreen (게시판 월드 스크린, 신규):** BP_StageSelectBoard에 부착되는 월드 스페이스 위젯(3장 참고). 현재 `ATCLobbyGameState::SelectedStageId`를 실시간으로 표시하여, **방장이 아닌 플레이어도 로비를 돌아다니며 어떤 스테이지가 선택되어 있는지 실시간으로 확인**할 수 있다. 표시 내용은 "선택됨" 여부와 스테이지 이름 정도의 경량 텍스트다(FStageInfo에 썸네일 필드가 아직 없으므로 — 7장-3 — 현재는 텍스트만 표시하고 썸네일은 데이터 도입 후 확장). O_StageSelect 오버레이 UI 전체를 복제하지 않는다(가벼운 갱신 유지).
  * 갱신 경로: `SetStageSelection(StageId)` 성공 시 `ATCLobbyGameState`의 `SelectedStageId` RepNotify가 `OnSelectedStageChanged(int32)` 델리게이트를 Broadcast → W_StageBoardScreen과 O_StageSelect 내부 하이라이트가 함께 갱신(7장 참고). CommonUI 화면 스택 경로(ReplaceState/PushOverlay)를 타지 않는 예외 위젯이므로 UMockUIController를 거치지 않는다(6장-9).
* **구성:** 스테이지 목록(FStageInfo 기반 리스트/카드), 선택 하이라이트, Btn_Confirm(확인), Btn_Cancel(취소).
  * **현재 스테이지 데이터가 존재하지 않으므로**, 목록은 기본 1스테이지(L_LevelProto) 단일 항목으로 표시된다. 목록 위젯은 `DT_Stages`(또는 Config 배열)에서 항목을 읽어 동적으로 생성하도록 구현하여, 스테이지 데이터가 추가되면 UI 수정 없이 늘어나게 한다(확장 개방).
* **동작:**
  * **확인(Btn_Confirm):** 하이라이트된 스테이지로 `SetStageSelection(StageId)` 호출 후 PopCurrentOverlay. 이후 [게임 시작] 시 해당 스테이지로 진입.
  * **취소(Btn_Cancel) / ESC:** 선택을 변경하지 않고 PopCurrentOverlay. **한 번도 확인하지 않았다면 기본 1스테이지가 유지**된다.
  * 스테이지 진입 자체는 이 오버레이가 하지 않는다 — 진입은 오직 S_Lobby의 [게임 시작](`HostStartGame()`)이 수행한다.
* **v1의 "진입 시 세이브 갱신" 로직 이관:** 스테이지 맵으로 트래블하기 직전(HostStartGame 내부)에 메모리에 로드된 세이브 데이터를 갱신(저장)한다.
* **`GetPreviousState()` 의존 제거:** v1에서 S_StageSelect가 진입 경로별 뒤로가기 분기를 위해 사용하던 GetPreviousState()는 이 오버레이에서는 불필요하다(항상 S_Lobby로 Pop). API 자체는 라우터에 유지한다(6장).

### 6) S_Tutorial (튜토리얼)
* **역할:** NewGame 방 최초 1회 학습 맵 (L_Tutorial).
* **동작:** 잡기/이동/놓기/적재. 1명만 성공해도 다음 단계 진행.
* **종료 라우팅 (변경):** 마지막 Step 완료/건너뛰기 시 **`HostReturnToLobby()`로 S_Lobby 복귀** (구 L_StageSelect 직행 폐기). 복귀 전에 세이브에 튜토리얼 완료를 기록하여, 같은 방의 다음 [게임 시작]이 이어하기 경로(스테이지 직행)를 타게 한다.

### 7) S_InGame (인게임 HUD)
* **역할:** 가구 운반 코어 루프 진행 및 실시간 정보 제공.
* **구성:** 팀 값어치 게이지(PB_TeamMoney — **개정:** 기존 점수판(HUD_Score) 텍스트 표기를 대체. ProgressBar 형태로 진행도를 보여주며, 내부 텍스트로 "현재 옮긴 값어치 / 레벨 전체 목표 값어치"를 표시), 가구 개수 표시(**Txt_FurnitureCount — v3 내부 개정, 구 Txt_RemainingFurniture 대체**. "이동 가능한 개수 / 전체 상자 개수(파괴된 것 포함)" 분수 표기), **W_SessionLog(접속 로그 텍스트 칸 — S_Lobby와 동일 위젯 재사용)**, **W_HelpPanel(화면 우측 하단 정렬 배치, 상시 노출 조작 키·팁 패널 — v3 내부 개정으로 위치 변경, 3장·4장-15 참고)**.
* **하위 컴포넌트 (Sub-Widgets):**
  * **W_FurnitureStatus (가구 상태창 UI):** 화면 중앙의 크로스헤어 또는 커서가 가구에 올라갔을 때(Hover) 나타나는 툴팁 위젯. 가구 이름, 내구도 게이지를 표시.
  * **W_SessionLog:** 인게임 중 유저 입퇴장 등의 세션 로그 표시(3장).
  * **W_HelpPanel:** 조작 키 안내 + 게임 팁을 상시 표시(3장). 정적 콘텐츠이므로 별도 델리게이트 연동 없음.
* **연동 로직 (이벤트 주도):**
  * 크로스헤어/커서가 상호작용 대상에 올라가거나 벗어날 때, UMockUIController의 `OnInteractTargetChanged(Target, Key)` 델리게이트를 Broadcast하여 W_FurnitureStatus의 표시/숨김을 전환하고, 내구도 게이지는 `OnDurabilityChanged(Current, Max)`로 갱신한다.
  * 가구가 트럭에 실리거나/내려지거나/파괴될 때 GameMode → GameState(RemainingFurniture, RepNotify) → UMockUIController의 `OnRemainingFurnitureUpdated` 델리게이트를 거쳐 `Txt_FurnitureCount`의 분자("이동 가능한 개수" = RemainingFurniture)를 갱신한다. 분모("전체 상자 개수, 파괴된 것 포함" = TotalFurnitureCount)는 스테이지 시작 시 GameMode가 가구 액터를 순회해 1회 산정한 뒤 GameState에 복제하는 기존 필드로, 스테이지 중 불변이므로 위젯은 `NativeConstruct` 시점에 1회 조회해 캐시한다(TotalLevelValue와 동일한 패턴, 5장 참고).
  * 팀 값어치가 갱신될 때 GameMode → GameState(TotalScore, RepNotify) → UMockUIController의 `OnTeamMoneyUpdated(NewTotalMoney)` 델리게이트를 거쳐 PB_TeamMoney의 채움 비율과 내부 텍스트를 갱신한다. 채움 비율 = `NewTotalMoney / TotalLevelValue`. `TotalLevelValue`(레벨 전체 목표 값어치)는 스테이지 시작 시 GameMode가 레벨 내 전체 가구 값어치 합으로 1회 산정해 GameState에 복제하며, 스테이지 중 불변이므로 위젯은 `NativeConstruct` 시점에 1회 조회해 게이지의 Max 값으로 사용한다(5장 참고).
  * 유저 입퇴장 시 GameMode → GameState 로그 복제 → `OnSessionLogAdded`로 W_SessionLog 갱신.
* **저장:** 게임 중 ESC를 눌러 호스트 권한으로 수동 저장 (O_PauseMenu 호출).
* **종료:** 게임 시간 종료/가구 전량 운반 완료 시 **O_Result가 Push된다** (아래 8항).

### 8) O_Result (최종 결과 오버레이)
* **역할:** 게임 종료 시 **인게임 레벨 위에 뜨는 전체화면 오버레이.** 정산 기능은 v1의 S_Result와 동일.
* **정산:** 남은 내구도에 따라 0~5 등급. 점수 산정 후 Team_Money 표기.
* **구성:** 최종 점수(Txt_Score), 세부 통계(Txt_Stats), 획득한 별 개수(Txt_StarCount, 0~3개), 소요 시간, **Btn_ToLobby(로비로 가기), Btn_ToTitle(메인 화면으로 가기)**.
* **연동 로직 (이벤트 주도):**
  * GameMode가 게임 종료를 확정하면(FinishGame) GameState의 `TotalScore`, `StarCount`, `ElapsedTime`, `bIsGameFinished`가 함께 갱신되고, `OnRep_bIsGameFinished`가 UMockUIController의 `TriggerGameResult(FinalScore, StarCount, ElapsedTime)`를 호출한다.
  * `TriggerGameResult`는 값을 컨트롤러 내부에 캐시한 뒤 **`PushOverlay("O_Result")`로 오버레이를 띄운다** (v1의 `ReplaceState(Result)`에서 변경 — S_InGame HUD는 아래에 남아 있되 가려진다). O_Result는 `NativeConstruct` 시점에 `GetLastFinalScore()`/`GetLastStarCount()`/`GetLastElapsedTime()`으로 캐시된 값을 즉시 읽어 반영한다(위젯 생성이 델리게이트 브로드캐스트보다 먼저 동기적으로 일어나기 때문 — 캐시 방식은 v1과 동일).
* **인게임 기능 정지:**
  * **입력:** O_Result는 GetDesiredInputConfig를 오버라이드하여 활성화 중 게임 입력을 완전히 차단(UI 전용 입력)한다. ESC로 닫을 수 없다(강제 모달 — 반드시 두 버튼 중 하나로 이탈).
  * **게임 로직:** 멀티플레이(리슨 서버)이므로 로컬 Pause를 쓰지 않고, 서버 권위에서 `bIsGameFinished=true`를 기준으로 타이머·점수·가구 상호작용 등 코어 루프 로직을 중단한다(GameMode/GameState 게이팅). 이후에는 O_Result 자체의 로직(정산 연출 등)만 수행된다.
* **이탈 라우팅:**
  * **Btn_ToLobby(로비로 가기):** `HostReturnToLobby()` — **세션을 유지한 채 L_Lobby(S_Lobby)로 복귀.** (v1에서 S_StageSelect로 복귀하던 것과 기능적으로 동일한 "세션 유지 복귀"이며, 목적지만 로비로 변경. 호스트 전용 트래블이므로 클라이언트의 버튼 클릭은 Server RPC로 호스트에 위임하거나 호스트만 활성화 — 프로토타입에서는 호스트 전용 활성화를 기본으로 한다.)
  * **Btn_ToTitle(메인 화면으로 가기):** `LeaveToTitle()` — 세션 파기 후 S_MainMenu 복귀 (v1 기능 그대로).

### 9) S_Loading (로딩 화면)
* **역할:** 모든 레벨 트래블(L_Title ↔ L_Lobby ↔ L_Tutorial ↔ 스테이지 맵) 구간에 표시되는 전환 화면.
* **구성 (딱 3개 요소):**
  1. **Img_Background:** 기본 배경 사진 (전체 화면).
  2. **PB_Loading:** 화면 하단의 로딩 바 (ProgressBar).
  3. **Txt_LoadingGauge:** 로딩 게이지를 알려주는 텍스트 (예: "37%", "Loading...").
* **동작:**
  * UTCSessionFlow가 트래블 직전 `OnTravelStarted(TargetMapPath)`를 Broadcast → UMockUIController가 `ReplaceState(EE_UIState::Loading)`으로 표시.
  * 트래블 완료 후 도착 맵의 GameMode/PlayerController가 목적지 State를 ReplaceState하면서 자연히 사라진다. **단, 도착 맵이 스테이지 맵(S_InGame)인 경우는 예외다 — 내 화면의 로딩이 끝나도 즉시 사라지지 않고, 세션 내 모든 플레이어의 로딩이 끝날 때까지 대기한 뒤 전원 동시에 사라진다(2장 "스테이지 맵 진입 전원 대기 게이트" 참고, `UI_v3_Implementation_Plan.md` Phase 1-B).** 그 외 목적지(L_Lobby, L_Title, L_Tutorial)는 기존과 동일하게 각자 로딩이 끝나는 즉시 사라진다.
  * ※ 구현 노트: UE의 맵 로드는 정밀한 진행률 콜백을 기본 제공하지 않으므로, 프로토타입 단계에서는 시간 기반 유사 진행(0→90% 보간 후 완료 시 100%) 또는 무한 애니메이션 바를 허용한다. 정확한 게이지가 필요해지면 레벨 스트리밍/AsyncLoad 진행률 연동으로 교체한다(확장 개방).
* **입력:** 로딩 중 모든 입력 무시.

### 10) O_JoinRoom (통합 접속 팝업)
* **역할:** 호스트의 방 생성과 클라이언트의 방 참가를 분기하는 모달 창.
* **구성:** '방 만들기' 선택 버튼, '방 참가' 선택 버튼, 코드 입력란(EditableTextBox), Btn_Cancel(취소).
* **동작 및 시각적 피드백:**
  * 항목('방 만들기' 또는 '방 참가') 클릭 시, 확대되는 효과를 주며, 버튼이 제대로 눌렸음을 명시.
  * 방 참가 진행 상태(검색/조인 중)와 실패는 `OnSessionPhaseChanged`를 구독하여 로딩 표시·에러 모달로 피드백한다.
* **입력 라우팅:**
  * '방 만들기' 클릭: O_JoinRoom을 닫고 S_SlotSelect로 화면 교체 (같은 맵 내 ReplaceState).
  * '방 참가' 클릭: `JoinRoomByCode(코드)` 호출 → 세션 검색/조인 → 성공 시 L_Lobby로 클라이언트 트래블(구간 중 S_Loading, 도착 후 **S_Lobby** 표시), 실패 시 `OnSessionPhaseChanged(Failed, 사유)` 통지로 에러 안내 모달 팝업 호출.
  * 취소 버튼 또는 ESC: O_JoinRoom을 닫고 S_MainMenu로 복귀.

### 11) O_Confirm (확인 팝업)
* **구성:** 제목, 설명 텍스트, Btn_Yes, Btn_No.
* **동작 로직:** 생성 시 강제 모달(Modal). Btn_No에 기본 포커스. Btn_Yes 클릭 시 전달받은 콜백(Delegate) 실행 후 스택에서 Pop.

### 12) O_PauseMenu (인게임/로비 공통 메뉴 — v3 내부 개정: S_Lobby 컨텍스트 추가)
* **역할:** S_InGame·S_Tutorial의 "일시정지 메뉴"와 S_Lobby의 "ESC 메뉴"를 하나의 클래스로 겸한다. Btn_Resume·Btn_Settings는 세 컨텍스트에서 로직이 동일하므로(둘 다 단순히 Pop / O_Settings Push), 별도 클래스(O_LobbyMenu 등)를 신설하는 대신 **호출 컨텍스트(Lobby / Tutorial / InGame)에 따라 버튼 노출만 다르게** 하는 기존 방식(S_Tutorial 노출 분기, 아래)을 S_Lobby까지 확장한다.
* **컨텍스트 판정 방식 (v3 내부 신규 — 구현 단순화):** 초안에서는 새 `EE_PauseMenuContext` enum과 `SetupPauseMenuContext()` 호출부 배선을 계획했으나, 실제 구현 착수 시 `UMockUIController`에 이미 `GetCurrentState()`(BlueprintPure, EE_UIState 반환)가 존재함을 확인하여 그대로 재사용했다. `PushOverlay()`는 `CurrentState`(Lobby/Tutorial/InGame)를 바꾸지 않으므로, O_PauseMenu는 `NativeConstruct()`에서 `MockController->GetCurrentState()`를 조회하는 것만으로 자신이 어느 화면 위에 떠 있는지 정확히 판별할 수 있다. 호출부(S_Lobby·S_Tutorial·S_InGame) 쪽 변경은 필요 없다 — 세 곳 모두 기존과 동일하게 `PushOverlay("O_PauseMenu")`만 호출한다. (7장-6의 `EE_PauseMenuContext` 계획은 폐기되었다.)
* **구성:** Btn_Resume(계속하기), Btn_Settings(설정), Btn_Save(수동 저장, 방장 전용), **Btn_ToLobby(로비 복귀, 방장 전용, 신규)**, **Btn_Reset(리셋, 방장 전용, 신규)**, **Btn_LeaveRoom(나가기, Lobby·S_InGame 공통, 호스트/클라이언트 모두 노출, v3 내부 신규 — 2026-07-16 재조정)**.
  * **제거:** Btn_KeyGuide(조작법) — S_InGame에 상시 노출되는 W_HelpPanel로 대체(4장-7, 4장-15). Btn_ToTitle(타이틀로) — 인게임 중 **호스트가 세션을 끝내는** 명시적 이탈 경로는 여전히 O_Result의 Btn_ToTitle이 정석이지만(오조작 방지 목적 유지), 2026-07-16 재조정 이후로는 **Btn_LeaveRoom을 호스트가 눌러도** 같은 결과(세션 파기 + 타이틀 복귀)에 도달할 수 있다 — 다만 O_Confirm에 세션 파기를 명시하는 경고 문구가 별도로 뜬다(아래 "S_InGame에서 호출될 때" 참고).
  * **WBP 구현 메모:** 기존 `Btn_KeyGuide`/`Btn_ToTitle` 위젯 인스턴스를 삭제 후 재생성하는 대신 각각 `Btn_ToLobby`/`Btn_Reset`으로 리네임해 기존 슬롯·스타일을 재사용했다. `Btn_LeaveRoom`은 신규 추가(Btn_Save와 동일 버튼 클래스 재사용). 리네임된 두 버튼의 라벨 텍스트("조작법"/"타이틀로")는 아직 새 용도에 맞게 갱신되지 않았다 — 버튼 세부 디자인은 별도 진행 예정(개정 요약 참고).
* **컨텍스트별 노출 (`GetCurrentState()` 값에 따라 적용, v3 내부 신규 — Lobby 행 추가):**
  | 호출 컨텍스트 | Btn_Resume | Btn_Settings | Btn_Save | Btn_ToLobby | Btn_Reset | Btn_LeaveRoom |
  | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
  | S_Lobby (신규) | O | O | 숨김 | 숨김 | 숨김 | O (전원) |
  | S_Tutorial | O | O | 비활성 | 숨김 | 숨김 | 숨김 |
  | S_InGame | O | O | 방장 전용 | 방장 전용 | 방장 전용 | **O (전원, 2026-07-16 재조정 — 호스트도 노출)** |
* **입력 라우팅:**
  * Btn_Settings 클릭 시 O_Settings를 Push.
  * Btn_ToLobby 클릭 시 O_Confirm(로비로 복귀 확인) 호출. 확인 시 `HostReturnToLobby()` — 세션을 유지한 채 진행 중인 스테이지를 즉시 이탈해 [S_Loading]을 거쳐 L_Lobby(S_Lobby)로 복귀한다(O_Result의 [로비로 가기]와 동일한 의도 함수를 게임 진행 중에도 사용 — `bIsGameFinished` 여부와 무관하게 즉시 트래블).
  * Btn_Reset 클릭 시 O_Confirm(스테이지 초기화 확인) 호출. 확인 시 `RestartStage()`(2장 신규 의도 함수) — 현재 선택된 스테이지 맵으로 재트래블([S_Loading] 경유)하여 가구 배치·팀 값어치·타이머 등 인게임 상태를 초기화한다.
  * **Btn_LeaveRoom 클릭 시(Lobby·S_InGame 공통, 호스트/클라이언트 모두 노출):** O_Confirm(방 나가기 확인) 호출. 확인 시 `LeaveToTitle()` — 로컬 세션 참여를 정리하고 [S_Loading]을 거쳐 S_MainMenu로 복귀한다. Lobby 컨텍스트에서는 호스트뿐 아니라 참가자도 누구나 방을 나갈 수 있으므로 `IsHost()` 게이팅을 받지 않는다(S_Lobby 자체의 기존 ESC 동작을 그대로 승계, 4장-3). **S_InGame 컨텍스트에서도 2026-07-16 재조정 이후로는 호스트/클라이언트 구분 없이 노출된다** — `LeaveToTitle()` 자체가 호출자 권한에 따라 이미 다르게 동작하므로(클라이언트가 호출하면 본인 세션 참여만 정리되어 다른 플레이어는 진행 중인 스테이지를 계속 플레이하지만, **호스트가 호출하면 세션 자체가 파기되어 전원이 함께 끊긴다**), Visibility로 호스트를 막는 대신 클릭 직후의 O_Confirm 경고 문구로 그 차이를 알린다: 호스트에게는 "정말로 방을 나가시겠습니까? 진행 중인 스테이지가 종료되며, 모든 플레이어가 게임에서 나가게 됩니다."를, 클라이언트에게는 기존과 동일한 "정말로 방을 나가시겠습니까?"를 보여준다.
* **권한:** Btn_Save·Btn_ToLobby·Btn_Reset은 멀티플레이 동기화를 위해 호스트(방장) 전용이며, `UTCSessionFlow::IsHost()`로 판정해 일반 클라이언트에게는 Collapsed 처리한다(6장-8 규칙과 동일). 노출 분기는 편의일 뿐이므로 실제 트래블/저장/리셋 실행은 서버 측에서 재검증한다. Btn_LeaveRoom은 Lobby·S_InGame 컨텍스트 모두 이 게이팅에서 제외되어 전원에게 노출된다(2026-07-16 재조정) — 호스트/클라이언트 차이는 Visibility가 아니라 `LeaveToTitle()`의 실제 동작 차이(세션 파기 여부) + O_Confirm 경고 문구가 담당한다.
* **S_Tutorial에서 호출될 때:** Btn_Save는 강제 비활성화(기존 규칙 유지), Btn_ToLobby·Btn_Reset·Btn_LeaveRoom은 노출하지 않는다 — 튜토리얼은 `CompleteTutorial()` 단일 경로로만 로비 복귀한다(4장-6 참고).
* **S_Lobby에서 호출될 때 (v3 내부 신규):** Btn_Save·Btn_ToLobby·Btn_Reset은 노출하지 않는다(로비에는 저장할 진행 중인 스테이지도, 복귀할 다른 곳도, 리셋할 스테이지도 없음). Btn_LeaveRoom만 신규 노출(전원).
* **S_InGame에서 호출될 때 (2026-07-16 재조정):** Btn_Save·Btn_ToLobby·Btn_Reset은 기존대로 호스트 전용이다. Btn_LeaveRoom은 이 컨텍스트에서도 **호스트/클라이언트 모두 Visible**이다 — 2026-07-15에는 클라이언트가 진행 중인 스테이지에서 개인적으로 이탈할 수단이 없던 공백을 메우려 호스트를 숨겼으나(Visibility 게이팅), `LeaveToTitle()`의 호출자별 동작 차이(호스트=세션 파기, 클라이언트=본인만 이탈)가 이미 안전장치이므로 재검토 후 게이팅을 없애고 O_Confirm 경고 문구로 대체했다.

### 13) O_Settings (설정 창)
* **구성:** 하위 탭은 별도 서브 위젯 클래스로 분리 — **O_AudioSettings**(오디오), **O_GraphicsSettings**(비디오). 하위 탭은 UCommonUserWidget 상속(6장 규칙).
* **동작 로직:** 값 변경 후 적용 클릭 시 UGameUserSettings 호출. 비디오 설정 시 15초 미확인 시 이전 상태로 원복하는 안전 로직 구현.

### 14) O_SaveLoad (저장/불러오기 창)
* **역할:** O_PauseMenu의 [수동 저장] 선택 시 Push되는 오버레이. S_SlotSelect와 동일한 4슬롯 카드 UI(구성/조회는 4장-2 참고)를 인게임 컨텍스트에서 재사용한다.
* **구성 (구현 완료):** S_SlotSelect와 동일하게 `Switcher_0`~`Switcher_3` + `Card_New_X`/`Card_Saved_X`(`UW_GameSlotCard_Saved`) 4쌍. `NativeConstruct()`에서 `GetAllSaveSlotInfos()`로 채운다(`RefreshSlotCards()`).
* **동작 로직:**
  * **Card_New_X 클릭(빈 슬롯에 신규 기록):** O_Confirm("게임 저장" / "현재 진행 상황을 이 슬롯에 저장하시겠습니까?") 확인 후 실행.
  * **Card_Saved_X 클릭(기존 슬롯 덮어쓰기):** O_Confirm("덮어쓰기" / "정말로 이 슬롯을 덮어쓰시겠습니까? 기존 저장 데이터가 사라집니다.") — 파괴적 액션이므로 Card_New와 다른 경고 문구를 사용한다.
  * 두 경로 모두 확인 시 `ATeamCarryGameMode::SaveGameToSlot(UTCSessionFlow::MakeSaveSlotName(SlotIndex))`를 호출해 현재 진행도를 그 슬롯에 저장한다. `SaveGameToSlot()`은 저장과 함께 `UTCSessionFlow::SetSaveSelection()`으로 이 세션의 활성 슬롯도 갱신하므로, 이후 `RestartStage()`/자동 저장(FinishGame)도 계속 같은 슬롯을 사용하게 된다. 저장 후 `RefreshSlotCards()`로 New→Saved 전환을 즉시 반영한다.
  * **삭제:** Card_Saved_X의 Btn_Delete → O_Confirm("저장 데이터 삭제") 확인 → `DeleteSaveSlot(SlotName)`. S_SlotSelect(4장-2)와 동일 경로.
* **취소/ESC:** 라우터(PopCurrentOverlay)로 닫아 O_PauseMenu로 복귀한다.

### 15) W_HelpPanel (인게임 도움말/조작 팁 패널) — 신규, 구 O_KeyGuide 대체
* **역할:** S_InGame **화면 우측 하단에 가지런히 정렬 배치**되어 **상시 노출**되는 도움말 패널(**v3 내부 개정** — 기존 "우측 중단부터 하단까지 세로 배치"안에서 변경). 기존 O_PauseMenu → O_KeyGuide 팝업 경로를 대체한다. 조작 키 안내뿐 아니라 게임 진행에 유용한 팁 문구도 함께 표시한다.
* **구성:** 우측 하단 정렬 목록(라인 단위) — 조작 키 바인딩 안내 + 게임 팁 텍스트. 팝업이 아니므로 제목/닫기 버튼이 없다.
  * **조작 키 콘텐츠 기준 (v3 내부 개정):** 실제 프로젝트에서 사용 중인 `UInputAction` 목록(이동, 시점 회전, 점프, 달리기, 상호작용, 던지기, 시점 전환, 오브젝트 Z/Y축 회전, 줌, 이모트 1~4, 이모트 취소 — `TCPlayerCharacter`/`TCPlayerController` 기준)을 그대로 나열하지 않고, 실제 게임 플레이에 필요한 핵심 조작만 추려 텍스트로 안내한다. 정확한 키 바인딩은 Input Mapping Context 에셋의 실제 값을 확인해 반영한다(현재 C++에는 액션 이름만 정의되어 있고 구체적 키는 IMC 에셋에 있음).
  * **키보드/마우스 아이콘 (추후 확장, 이번 개정 범위 아님):** 현재는 텍스트 라인만 표시한다. 아이콘 에셋이 준비되면 각 라인 앞에 키/마우스 아이콘 이미지를 붙이는 방식으로 확장한다(레이아웃을 아이콘+텍스트 가로 배치로 미리 고려해 둘 것).
* **동작 로직:**
  * S_InGame 진입과 동시에 항상 노출되며 게임 입력을 차단하지 않는다. `UCommonUserWidget` 하위 위젯이므로 GetDesiredInputConfig 오버라이드가 불필요하다.
  * 콘텐츠는 정적이므로 델리게이트 구독 없이 위젯 내부(Blueprint)에 직접 목록을 구성한다. 추후 상황별 팁(예: 도둑 AI 출현 경고와 연동한 강조 표시 등)이 필요해지면 델리게이트 연동을 확장 포인트로 고려한다.
* **제거된 것:** 기존 O_KeyGuide의 팝업 형태(Push/Pop, Btn_Back, GetDesiredInputConfig 입력 차단)와 O_PauseMenu의 Btn_KeyGuide 항목은 폐기되었다(4장-12).

---

## 5. 통합 데이터 모델 및 저장 스키마

**저장 규칙 확정:** USaveGame 객체에는 플레이어 인원, 접속자 정보 등 유동 데이터 일절 미저장. 세계 상태(맵/진행도)만 기록.

| 데이터 구조 (도메인) | 유지 방식 | 포함되는 핵심 필드 (실제 구현) |
| :--- | :--- | :--- |
| **세이브 슬롯 (디스크, UTCSaveGame)** | 영구 보존, 고정 4슬롯(`UTCSessionFlow::NumSaveSlots`, Config) | `StageRecords: TMap<FString, FStageRecord>` (bIsCleared, BestStar, BestScore), `LastPlayedStage`, **(확장 예정) bTutorialCompleted**. 슬롯 이름은 `MakeSaveSlotName(Index)`("SaveSlot_0".."SaveSlot_3", Index 0..NumSaveSlots-1)로 통일 — S_SlotSelect/O_SaveLoad가 카드 위치와 슬롯을 매칭하는 단일 규칙(4장-2, 4장-14). `ATeamCarryGameMode::SaveGame()`/`LoadGame()`은 `UTCSessionFlow::GetSelectedSlotName()`(세션이 선택한 슬롯)을 사용하며, 미선택 시(구버전 호환)에만 `"TCGameSave"`로 폴백한다 |
| **세이브 슬롯 요약 (런타임 조회 전용, FSaveSlotInfo — 신규, 구현 완료)** | 복제/저장되지 않음, 호출마다 재조회 | `SlotIndex`, `SlotName`, `bHasSaveData`(`UGameplayStatics::DoesSaveGameExist`), `LastPlayedStage`. `UTCSessionFlow::GetAllSaveSlotInfos()`가 NumSaveSlots개를 매번 새로 스캔해 배열로 반환 — S_SlotSelect/O_SaveLoad가 슬롯별 Card_New/Card_Saved 전환에 사용(4장-2, 4장-14) |
| **멀티 로비 (복제)** | 세션 내 유지 | ATCPlayerState: `bIsReady`, `LobbySlotIndex`, `CharacterIndex` / ATCLobbyGameState: `RoomCode`, **(신규) `SelectedStageId`**, **(신규) `SessionLogEntries: TArray<FText>` (RepNotify)** — 준비/외형/입퇴장 변경 알림은 `OnLobbyPlayersChanged` 단일 경로, 로그는 `OnSessionLogAdded` 경로, **`SelectedStageId` 변경 알림은 `OnSelectedStageChanged` 경로(v3 내부 신규 — BP_StageSelectBoard의 W_StageBoardScreen 실시간 갱신용, 4장-5·7장)** |
| **스테이지 정의 (정적, 도입 예정)** | 에셋/Config | `DT_Stages` (FStageInfo: StageId, DisplayName, MapPath, …). 현재는 1스테이지(L_LevelProto) 단일 기본 항목 |
| **가구/운반 (액터)** | 스테이지 내 유지 | MaxHealth, CurrentHealth, RequiredPlayer, CurrentGrabbedPlayer, BaseScore |
| **전역 설정 (로컬)** | 클라이언트별 | MasterVolume, GraphicsQuality, InputBindings |
| **게임 진행 (GameState, 복제)** | 스테이지 내 유지 | TotalScore, RemainingFurniture, ElapsedTime, bIsGameFinished, CurrentPhase, StarCount, SessionLogEntries, TotalFurnitureCount(스테이지 시작 시 GameMode가 가구 액터 순회로 1회 산정, 스테이지 중 불변 — **v3 내부 개정: Txt_FurnitureCount 분모("전체 상자 개수, 파괴된 것 포함")로 UI에 노출**, 4장-7), **(v3 신규) TotalLevelValue** (스테이지 시작 시 GameMode가 전체 가구 값어치 합으로 1회 산정, 스테이지 중 불변 — PB_TeamMoney 게이지의 Max 값, 4장-7) |

**세이브 스키마 미구현 요구사항 (일부 해소):** 4개 고정 슬롯의 존재 여부/삭제/선택 메커니즘 자체는 구현 완료됐다(`FSaveSlotInfo`, `GetAllSaveSlotInfos()`, `DeleteSaveSlot()`, `MakeSaveSlotName()`, 위 표 참고). 다만 카드 표시용 부가 메타데이터(맵 썸네일, 진행도 요약, LastPlayedDate, CurrentTeamMoney)는 여전히 UTCSaveGame에 없다 — `LastPlayedStage`(마지막 플레이 스테이지)만 조회 가능하며, 나머지는 슬롯 카드 UI가 더 풍부한 정보를 요구하게 되면 스키마 확장이 필요하다. **추가로, 튜토리얼 완료 후 로비 복귀 흐름(4장-6)을 위해 bTutorialCompleted 플래그 확장이 필요하다.**

**세이브 슬롯 선택 연동:** 슬롯 확정은 `UTCSessionFlow::SetSaveSelection(SlotName, bContinue)`로, 스테이지 확정은 `SetStageSelection(StageId)`로 세션 계층에 전달되며, 세션 수명 동안 유지된다(2장). SelectedStageId는 클라이언트 표시용으로 ATCLobbyGameState에도 복제한다(방장이 고른 스테이지를 참가자가 로비에서 확인 가능). **(신규, 구현 완료)** 세션 진행 중 O_SaveLoad에서 다른 슬롯에 수동 저장하면(`ATeamCarryGameMode::SaveGameToSlot(SlotName)`), 그 슬롯이 `SetSaveSelection()`을 통해 이 세션의 활성 슬롯으로 갱신되어 이후 `RestartStage()`/자동 저장(FinishGame)도 계속 그 슬롯을 사용한다(4장-14).

---

## 6. 프로토타입 구현 아키텍처 규칙 (AI 코드 생성 가이드)

1. **Common UI 상속의 엄격한 분리:**
   * **최상위 화면 및 팝업:** 화면 전체를 덮는 State(S_ 계열 — S_Lobby, S_Loading 포함)와 Overlay(O_ 계열 — O_CharacterSelect, O_StageSelect, O_Result 포함)의 메인 클래스는 반드시 `UCommonActivatableWidget`을 상속받아야 합니다.
   * **하위 컴포넌트:** 설정 창 내부의 서브 탭(O_AudioSettings, O_GraphicsSettings 등)이나 리스트 내부의 슬롯 아이템, HUD 하위 위젯(W_FurnitureStatus, **W_SessionLog**, **W_HelpPanel**, O_StageSelect의 스테이지 카드 아이템 등)은 UUserWidget이 아닌 `UCommonUserWidget`을 상속받아야 합니다.
2. **입력 라우팅 (Input Config) 관리:**
   * 오버레이 위젯은 GetDesiredInputConfig()를 오버라이드하여, 활성화 시 게임 입력을 막고 UI 전용 입력(Menu)으로 전환되도록 설정해야 합니다.
   * **예외 규정 — S_Lobby / S_InGame:** 이 두 State는 캐릭터 조작이 살아 있어야 하므로 게임 입력을 차단하지 않습니다(Game 또는 GameAndMenu). 그 위에 Push되는 오버레이(O_CharacterSelect, O_StageSelect, O_PauseMenu, **O_Result** 등)가 활성화되는 동안에만 UI 전용 입력으로 전환됩니다.
3. **UMockUIController (중앙 라우터) 통제:**
   * 위젯 내부에서 CreateWidget이나 AddToViewport 직접 호출을 절대 금지합니다.
   * 화면 전환은 전역 서브시스템인 UMockUIController의 ReplaceState(), PushOverlay(), PopCurrentOverlay()를 통해서만 수행합니다.
   * `PushOverlay(const FString& OverlayName)`는 생성된 `UCommonActivatableWidget*`를 반환합니다(실패 시 nullptr, 라우터가 상태를 롤백).
   * `GetPreviousState()`: ReplaceState() 직전의 화면을 반환. GameInstance 서브시스템이므로 레벨 트래블을 넘어서도 유지됩니다. (v1에서 S_StageSelect가 사용하던 주 용도는 사라졌으나, S_Loading 이후 복귀 판단 등 범용 API로 유지합니다.)
4. **IUIHost 위임 (라우터/호스트 분리):**
   * "무엇을/어떤 순서로"(상태·스택)는 UMockUIController가, "실제로 어떻게"(CreateWidget/AddToViewport)는 IUIHost 인터페이스를 구현한 **GameUIPlayerController**가 책임집니다. 라우터는 구체 PC 타입을 모르고 인터페이스만 압니다.
   * IUIHost 인터페이스: `ShowState(NewState)`, `ShowOverlay(OverlayId)` → 생성된 위젯 반환, `HideTopOverlay()`.
   * PlayerController가 BeginPlay/EndPlay에서 `RegisterUIHost()`/`UnregisterUIHost()`로 자신을 등록/해제합니다. 라우터는 호스트를 약참조(TWeakObjectPtr)로 보관하여 PC 파괴 시 dangling을 방지합니다.
5. **세션 동작 위임:**
   * 위젯은 세션 API(UTCGameInstance)나 ServerTravel을 직접 호출하지 않고, UTCSessionFlow의 의도 함수만 호출합니다(2장). O_Result의 [로비로 가기]도 `HostReturnToLobby()` 의도 함수 경유입니다.
6. **이벤트 주도 데이터 바인딩 (Delegate Mapping):**
   * UI 위젯은 외부 액터나 데이터를 매 프레임(Tick) 직접 참조하지 않습니다.
   * S_InGame, S_Lobby 등은 UMockUIController의 델리게이트를 Bind하여 화면을 갱신합니다.
7. **BindWidget 및 매크로 규약:**
   * 모든 UI 구성요소(버튼, 텍스트 등)는 헤더 파일에서 `UPROPERTY(meta = (BindWidget))`로 선언합니다.
   * 버튼 클래스는 `UCommonButtonBase`로 통일하여 선언하십시오.
8. **호스트 전용 UI 분기 (신규 명문화):**
   * 방장 전용 버튼(S_Lobby의 Btn_StageSelect·Btn_Start, O_Result의 Btn_ToLobby, O_PauseMenu의 Btn_Save·**Btn_ToLobby·Btn_Reset**)의 노출/활성 분기는 `UTCSessionFlow::IsHost()`로 판정합니다. 단, 노출 분기는 편의일 뿐 권위가 아니므로, 실제 트래블/저장/리셋 실행은 서버(호스트) 측에서 재검증합니다.
   * BP_StageSelectBoard의 상호작용 프롬프트(O_StageSelect를 여는 트리거) 역시 동일 규칙을 따릅니다 — 방장이 아니면 프롬프트 자체가 노출되지 않으며(비활성 표시가 아닌 완전 숨김), 실제 `SetStageSelection()` 호출도 서버 측에서 재검증합니다(4장-5).
9. **월드 스페이스 위젯 예외 (신규):**
   * BP_StageSelectBoard에 부착되는 **W_StageBoardScreen**은 화면 전체를 뒤덮는 CommonUI 화면 스택(State/Overlay)에 속하지 않는 3D 월드 스페이스 위젯입니다. `UMockUIController`의 `ReplaceState()`/`PushOverlay()`/`PopCurrentOverlay()` 경로를 타지 않고, 액터에 직접 부착된 `UWidgetComponent`를 통해 렌더링됩니다.
   * 규칙 3(중앙 라우터 통제)의 "CreateWidget/AddToViewport 직접 호출 금지"는 화면 전체를 차지하는 State/Overlay에 적용되는 규칙이며, 월드에 배치되는 액터 부착형 위젯(W_StageBoardScreen)에는 적용되지 않습니다. 대신 `ATCLobbyGameState`의 `OnSelectedStageChanged` 델리게이트를 직접 구독해 갱신합니다(7장).

---

## 7. 핵심 데이터 구조체 (Struct) 및 시스템 매크로 명세

UMockUIController 및 이벤트 델리게이트에서 사용할 필수 데이터입니다. AI는 코드 생성 시 헤더 파일에 아래 요소들을 블루프린트 에디터에서 완벽히 접근 가능하도록 리플렉션 매크로를 포함하여 정의해야 합니다.

### 1) EE_UIState (화면 상태 열거형) — 개정
UMockUIController의 ReplaceState()에서 사용할 화면 식별자입니다. (enum class로 선언, UENUM(BlueprintType) 적용)
* None
* Boot
* MainMenu
* SlotSelect
* **Lobby** (신규 — 구 CharacterSelect 대체)
* Tutorial
* InGame
* **Loading** (신규)

**제거된 값:** CharacterSelect, StageSelect, Result — 각각 오버레이 ID(`"O_CharacterSelect"`, `"O_StageSelect"`, `"O_Result"`)로 대체. 기존 코드 호환을 위해 enum 값을 즉시 삭제하지 않고 `Deprecated_` 접두를 붙여 한 단계 유예하는 것을 허용한다.

### 2) FPlayerInfo (로비 및 플레이어 상태 구조체 — 목업/프로토타입용)
USTRUCT(BlueprintType)으로 선언하며, 내부의 모든 멤버 변수는 반드시 `UPROPERTY(EditAnywhere, BlueprintReadWrite)` 매크로를 포함해야 UI 바인딩이 가능합니다.
* int32 CharacterIndex;
* FString PlayerName;
* bool bIsReady;

**주의:** 실제 로비 상태는 ATCPlayerState/ATCLobbyGameState 복제 모델을 사용하며(4장-3), 호스트 여부 판정은 별도 필드 없이 `UTCSessionFlow::IsHost()`를 사용합니다.

### 3) FStageInfo (스테이지 정의 구조체 — 도입 예정, 미구현)
O_StageSelect 목록과 HostStartGame 트래블 대상 해석에 사용할 정적 스테이지 데이터입니다. USTRUCT(BlueprintType), FTableRowBase 상속(DataTable 사용 시).
* int32 StageId;
* FText DisplayName;
* FString MapPath;
* (확장 여지: 썸네일 SoftObjectPtr, 해금 조건, 별 획득 조건 등)

**현재 상태:** 스테이지 데이터가 존재하지 않으므로 StageId=1 / L_LevelProto 단일 기본 항목만 하드코딩 폴백으로 제공하며, DT_Stages가 채워지면 그대로 목록이 확장된다.

### 4) 다이내믹 멀티캐스트 델리게이트 (Dynamic Multicast Delegate) 규칙
UI 위젯의 NativeConstruct에서 `AddDynamic`을 통해 이벤트를 수신할 수 있도록, UMockUIController에 선언되는 모든 델리게이트는 반드시 `DECLARE_DYNAMIC_MULTICAST_DELEGATE` 계열의 매크로를 사용하여 선언해야 합니다.

**UMockUIController 델리게이트 목록 (개정 기준):**
1. `OnTeamMoneyUpdated(int32 NewTotalMoney)`: 팀 보유 값어치 변경. **(v3)** S_InGame의 PB_TeamMoney 게이지 채움 비율(`NewTotalMoney / TotalLevelValue`)과 내부 텍스트("현재/전체") 갱신에 사용(4장-7).
2. `OnFurnitureSettled(int32 AddedMoney, int32 Grade)`: 가구 정착 및 정산.
3. `OnInteractTargetChanged(AActor* Target, FString Key)`: 상호작용 대상 변경. W_FurnitureStatus의 표시/숨김 전환에 사용.
4. `OnDurabilityChanged(float Current, float Max)`: 가구 내구도 변경. W_FurnitureStatus 게이지 갱신에 사용.
5. `OnLobbySlotUpdated(int32 SlotIndex, FPlayerInfo PlayerInfo)`: 프로토타입 잔재. 실제 로비 갱신은 ATCLobbyGameState의 `OnLobbyPlayersChanged`를 사용한다.
6. `OnStateChanged(EE_UIState NewState)`: UI 상태(화면) 전환 통지.
7. `OnRemainingFurnitureUpdated(int32 NewCount)`: GameState의 RemainingFurniture가 갱신될 때 Broadcast. **(v3 내부 개정)** S_InGame의 `Txt_FurnitureCount` 분자("이동 가능한 개수") 갱신에 사용(4장-7).
8. `OnGameResultReady(int32 FinalScore, int32 StarCount, float ElapsedTime)`: `TriggerGameResult()` 호출 시 Broadcast. **O_Result**는 위젯 생성 시점에 이미 캐시된 값을 직접 읽어가므로(`GetLastFinalScore`/`GetLastStarCount`/`GetLastElapsedTime`), 이 델리게이트는 추후 다른 상시 존재 위젯이 결과를 참조해야 할 경우를 위한 확장 포인트다.
9. `OnSessionLogAdded(FText LogMessage)` **(신규)**: 세션 로그(유저 입장/퇴장 등) 항목이 추가될 때 Broadcast. S_Lobby·S_InGame의 W_SessionLog가 구독하여 줄을 누적한다. 원천은 GameMode(PostLogin/Logout) → GameState 복제 배열(RepNotify) → 본 델리게이트 순.

**컨트롤러 외부 델리게이트 (UI가 함께 구독):**
* `OnSessionPhaseChanged(ETCSessionPhase, FString)` — UTCSessionFlow. 세션 로딩/에러 표시(2장).
* `OnTravelStarted(FString TargetMapPath)` — UTCSessionFlow **(신규)**. S_Loading 표시 트리거(2장, 4장-9).
* `OnLobbyPlayersChanged()` — ATCLobbyGameState. 로비 표시 갱신 트리거(4장-3).
* `OnSelectedStageChanged(int32 NewStageId)` — ATCLobbyGameState **(신규)**. `SelectedStageId`의 RepNotify에서 Broadcast. O_StageSelect의 선택 하이라이트와 BP_StageSelectBoard의 **W_StageBoardScreen** 실시간 갱신에 함께 사용(4장-5, 6장-9).

### 5) FFurnitureStatusData (가구 상태 정보 구조체 — 도입 예정, 미구현)
인게임에서 가구를 바라볼 때(Hover) W_FurnitureStatus UI에 전달할 데이터 묶음입니다. USTRUCT(BlueprintType)으로 선언하며, 멤버 변수에 `UPROPERTY(EditAnywhere, BlueprintReadWrite)`를 적용합니다.
* FString FurnitureName;
* float CurrentDurability;
* float MaxDurability;

**현재 상태:** 구조체는 아직 코드에 없으며, `UW_FurnitureStatus::UpdateFurnitureStatus(Name, CurrentDurability, MaxDurability)` 개별 파라미터 임시 인터페이스를 사용 중이다. 백엔드 연동 단계에서 구조체 단일 파라미터로 교체한다.

### 6) O_PauseMenu 컨텍스트 판정 — `GetCurrentState()` 재사용 (v3 내부 신규, 구현 단순화)
O_PauseMenu가 S_Lobby·S_Tutorial·S_InGame 세 곳에서 공용으로 호출되면서(4장-12), 자신이 어느 화면 위에 떠 있는지(=어떤 버튼 조합을 노출해야 하는지)를 알려줄 방법이 필요해졌다. 초안에서는 새 `EE_PauseMenuContext` enum + `SetupPauseMenuContext()` 호출부 배선(O_Confirm의 `SetupConfirm()` 패턴 차용)을 계획했으나, 구현 착수 시 `UMockUIController`에 이미 `GetCurrentState()`(BlueprintPure, `EE_UIState` 반환)가 존재함을 확인했다. `PushOverlay()`는 `CurrentState`를 바꾸지 않으므로 이 조회만으로 충분해, 새 enum·함수·호출부 배선 없이 그대로 재사용했다.

* **`UO_PauseMenu::RefreshContextVisibility()`:** `NativeConstruct()`에서 호출하는 내부 함수(private). `MockController->GetCurrentState()`가 `Lobby`/`Tutorial`/`InGame` 중 무엇인지에 따라 4장-12의 노출 표대로 Btn_Save·Btn_ToLobby·Btn_Reset·Btn_LeaveRoom의 Visibility를 즉시 갱신한다.
* **호출부 변경 없음:** S_Lobby·S_Tutorial·S_InGame 세 호출부는 기존과 동일하게 `PushOverlay("O_PauseMenu")`만 호출한다. Push 자체의 시그니처(`PushOverlay(const FString&)`, 6장)도 변경되지 않았다.

### 7) FSaveSlotInfo (세이브 슬롯 요약 구조체 — 신규, 구현 완료)
S_SlotSelect/O_SaveLoad가 슬롯 카드(Card_New/Card_Saved) 전환에 사용하는 런타임 조회 전용 데이터(5장 참고). USTRUCT(BlueprintType)으로 선언되며, 디스크에 저장되거나 복제되지 않고 `UTCSessionFlow::GetAllSaveSlotInfos()` 호출마다 새로 스캔해 만들어진다.
* `int32 SlotIndex` (BlueprintReadOnly)
* `FString SlotName` (BlueprintReadOnly) — `UGameplayStatics::SaveGameToSlot`/`DoesSaveGameExist` 등에 그대로 넘기는 실제 슬롯 이름. `UTCSessionFlow::MakeSaveSlotName(Index)`(static, BlueprintPure)가 `"SaveSlot_{Index}"` 형식으로 생성하는 이름과 동일 규칙을 따른다.
* `bool bHasSaveData` (BlueprintReadOnly) — 해당 슬롯에 세이브 파일이 존재하는지. S_SlotSelect/O_SaveLoad가 이 값으로 Card_New/Card_Saved 중 무엇을 보여줄지 판단한다.
* `FString LastPlayedStage` (BlueprintReadOnly) — 존재하는 경우 그 세이브의 `UTCSaveGame::LastPlayedStage`(카드 표시용).

---

## 8. 확장 예정 HUD 요소 (신규 게임플레이 시스템 연동, TBD)

코드베이스에 이미 존재하지만 UI 연동이 미정의된 게임플레이 시스템 목록. S_InGame HUD 확장 시 아래 훅을 검토한다.

| 시스템 | 관련 클래스 | 예상 UI 요소 |
| :--- | :--- | :--- |
| 도둑 AI | TCRobberCharacter, TCRobberAIController (훔치기/도주 BT) | 도둑 출현/도난 경보 (W_SessionLog 로그 라인 재사용 검토) |
| 추격 차량 | TCChaserVehicle | 추격 경고 표시 |
| 아이템 | TCBatItem, TC_UsableItem | 획득/사용 프롬프트, 보유 아이템 슬롯 |
| 기절 | TCStunnable | 기절 상태 표시 |
| 파괴 가능 구조물 | BreakableWindow, DamageableWall, InteractableDoor, BreakableProp | 상호작용 프롬프트 (`OnInteractTargetChanged` 재사용) |
| 움직이는 트럭 | TCMovingTruck | 트럭 위치/적재 안내 |

--- 

## 9. v1 → v2 마이그레이션 체크리스트 (구현 순서 가이드)

1. `EE_UIState`에 Lobby, Loading 추가 / CharacterSelect·StageSelect·Result는 Deprecated 처리.
2. UTCSessionFlow: `SetStageSelection`/`GetSelectedStageId` 추가, `HostStartGame` 이어하기 분기를 선택 스테이지 직행으로 수정, `HostTravelToStage`·`HostReturnToStageSelect` 제거, `OnTravelStarted` 델리게이트 추가, `StageSelectMapPath` Config 제거.
3. L_Lobby의 GameMode/PC가 S_CharacterSelect 대신 S_Lobby를 띄우도록 변경. S_Lobby는 캐릭터 스폰/조작이 가능한 로비 GameMode 위에서 HUD형으로 동작.
4. 기존 S_CharacterSelect 위젯의 외형 선택 부분을 O_CharacterSelect로 분리, Ready/Start 로직을 S_Lobby로 이관.
5. O_StageSelect 신규 작성 (DT_Stages 폴백 = 1스테이지 단일 항목).
6. S_Result → O_Result 전환: TriggerGameResult가 ReplaceState 대신 PushOverlay 호출, 버튼 2종([로비로]/[메인으로]) 배선, bIsGameFinished 기반 코어 루프 정지 게이팅 확인.
7. S_Loading 신규 작성 + OnTravelStarted 바인딩.
8. W_SessionLog 신규 작성, GameMode PostLogin/Logout → GameState 로그 복제 → OnSessionLogAdded 경로 구축, S_Lobby·S_InGame에 배치.
9. 튜토리얼 완료 라우팅을 HostReturnToLobby()로 변경 + 세이브에 bTutorialCompleted 기록.
10. L_StageSelect 맵 및 관련 에셋/참조 제거.

---

## 10. v2 → v3 마이그레이션 체크리스트 (회의 반영, 구현 순서 가이드)

1. GameState에 `TotalLevelValue` 필드 추가. 스테이지 시작 시 GameMode가 레벨 내 전체 가구 값어치 합으로 1회 산정해 복제(5장).
2. S_InGame: 기존 HUD_Score 텍스트 위젯을 PB_TeamMoney(ProgressBar) + 내부 텍스트("현재/전체")로 교체. `OnTeamMoneyUpdated` 델리게이트 바인딩을 채움 비율 계산 로직으로 갱신(4장-7).
3. W_HelpPanel 신규 작성(`UCommonUserWidget`). S_InGame 화면 우측 중단~하단에 배치하고 조작 키·게임 팁 콘텐츠 구성(3장, 4장-15).
4. O_KeyGuide 오버레이 클래스 및 Push/Pop 경로 제거, O_PauseMenu의 Btn_KeyGuide 제거.
5. O_PauseMenu에서 Btn_ToTitle 제거. 방장 전용 Btn_ToLobby·Btn_Reset 신규 추가(각각 O_Confirm 경유, `IsHost()` 노출 분기).
6. UTCSessionFlow에 `RestartStage()` 의도 함수 추가 — `GetSelectedStageMapPath()`로 현재 스테이지 맵에 재트래블(2장).

---

## 11. v3 내부 추가 체크리스트 (2026-07-14 회의 반영, 구현 순서 가이드)

1. **BP_StageSelectBoard 신규 제작:** `ATCStageSelectBoard`(신규 네이티브 클래스)의 블루프린트 자식으로 L_Lobby에 씬 유일 인스턴스 배치. 전용 `TeleportAnchor`(USceneComponent) 부착, 기존 `IA_Interact`/`OnInteractTargetChanged` 경로 재사용(별도 트리거 시스템 신설 금지), 방장이 아니면 상호작용 프롬프트 자체를 숨김 처리, 월드 스페이스 위젯(W_StageBoardScreen) 부착. 상호작용 시 `PushOverlay("O_StageSelect")` 호출(4장-5). S_Lobby HUD에 `OnInteractTargetChanged` 구독용 경량 프롬프트 텍스트(Txt_InteractPrompt)도 함께 신설(W_FurnitureStatus 재사용 불가).
2. **ATCLobbyGameState:** `OnSelectedStageChanged(int32)` 델리게이트 추가, `SelectedStageId`의 RepNotify에서 Broadcast(7장). W_StageBoardScreen과 O_StageSelect 하이라이트가 이를 구독.
3. **S_Lobby의 Btn_StageSelect 동작 변경:** `PushOverlay(O_StageSelect)` 직접 호출 → `GetActorOfClass(ATCStageSelectBoard)`로 보드 액터(BP_StageSelectBoard)를 찾아 `TeleportAnchor` 위치/회전으로 방장 캐릭터를 순간이동(Teleport)시키는 로직으로 교체(4장-3).
4. **S_InGame 가구 개수 표기 교체:** `Txt_RemainingFurniture` → `Txt_FurnitureCount`(분수 표기)로 변경. GameState의 기존 `TotalFurnitureCount` 필드를 `NativeConstruct`에서 1회 조회해 분모로 캐시, `RemainingFurniture`를 분자로 사용(4장-7).
5. **로딩 화면 동기화 수정 반영:** `UI_v3_Implementation_Plan.md` Phase 1(모든 클라이언트 로딩 화면 노출 + 스테이지 맵 진입 전원 대기 게이트) 구현 착수 전 해당 문서의 "착수 전 재확인 체크리스트"를 먼저 따른다(2장, 4장-9).
6. **W_HelpPanel 위치/콘텐츠 갱신:** WBP 배치를 화면 우측 하단 정렬로 변경. 실제 IMC(Input Mapping Context) 바인딩 기준으로 조작 키 안내 텍스트 갱신(4장-15).
7. **O_PauseMenu에 Lobby 컨텍스트 추가:** `UO_PauseMenu::RefreshContextVisibility()` 신규 추가 — 새 enum 없이 기존 `MockUIController::GetCurrentState()`를 재사용(구현 단순화, 7장-6). Btn_KeyGuide/Btn_ToTitle 위젯을 각각 Btn_ToLobby/Btn_Reset으로 리네임(기존 스타일 재사용), Btn_LeaveRoom 신규 추가 + 컨텍스트별(Lobby/Tutorial/InGame) 노출 분기 구현(4장-12). 호출부(S_Lobby·S_Tutorial·S_InGame)는 변경 불필요. S_Lobby의 ESC 라우팅(`NativeOnHandleBackAction`)을 O_PauseMenu 경유로 변경, Btn_Back(나가기 버튼)은 기존 O_Confirm 직행 단축 경로를 유지(4장-3). 별도 O_LobbyMenu 클래스는 만들지 않는다. TCSessionFlow::RestartStage() 신규 추가.
8. **범위 제외 확인:** 인게임 폰트, 버튼 세부 디자인(톤앤매너)은 이번 체크리스트에 포함하지 않는다 — 담당자 별도 확정 후 반영.