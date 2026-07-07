# UI_Technical_Spec_v2.md
# 이사 협동 게임 UI 최종 기획 및 기술 명세서 (개정판: S_Lobby 중심 구조)

> **개정 요약 (v1 → v2)**
> * **S_Lobby 신설:** L_Lobby의 메인 State. S_InGame처럼 캐릭터 조작이 가능한 "플레이어블 로비".
> * **S_CharacterSelect → O_CharacterSelect:** 캐릭터 외형 변경 전용 오버레이로 축소. 준비/시작 기능은 S_Lobby로 이관.
> * **S_StageSelect → O_StageSelect:** 스테이지 선택 오버레이(방장 전용)로 전환. **L_StageSelect 맵 삭제.** 미선택 시 기본 1스테이지.
> * **S_Result → O_Result:** 인게임 레벨 위의 전체화면 오버레이로 전환. [로비로 가기]/[메인 화면으로] 버튼 제공.
> * **S_Loading 신설:** 모든 레벨 트래블 구간에 표시되는 로딩 화면.
> * **접속 로그 위젯(W_SessionLog):** S_Lobby와 S_InGame에 공통 배치.
> * **레벨 구성 확정:** L_Title, L_Lobby, L_Tutorial, 스테이지 맵(L_LevelProto 등)만 레벨로 유지. 그 외 화면은 전부 오버레이.

---

## 1. UI 전체 구조도 (화면 흐름도)

### 1-1) 레벨(맵)과 State 매핑

UI State는 단일 맵 안에서만 전환되지 않는다. 화면 전환은 두 종류로 구분한다.
* **화면 교체(ReplaceState):** 같은 맵 안에서 UMockUIController가 위젯만 교체.
* **레벨 트래블(ServerTravel/ClientTravel):** UTCSessionFlow(2장)가 맵 자체를 이동. 도착한 맵의 GameMode/PlayerController가 해당 State를 다시 띄운다. **모든 레벨 트래블 구간에는 S_Loading이 표시된다.**

| 맵 | 포함 State | 주요 오버레이 | 진입 방식 |
| :--- | :--- | :--- | :--- |
| **L_Title** | S_Boot, S_MainMenu, S_SlotSelect | O_JoinRoom, O_Settings, O_Confirm | 앱 실행 / LeaveToTitle() |
| **L_Lobby** | S_Lobby | O_CharacterSelect, O_StageSelect, O_Confirm | 세션 생성(호스트)·조인(클라) 직후 트래블 / HostReturnToLobby() |
| **L_Tutorial** | S_Tutorial | O_PauseMenu 계열 | HostStartGame() — 새 게임 |
| **스테이지 맵 (L_LevelProto 등)** | S_InGame | O_PauseMenu 계열, **O_Result** | HostStartGame() — 이어하기 (O_StageSelect에서 선택한 맵, 기본 1스테이지) |
| *(전환 구간)* | **S_Loading** | — | 모든 레벨 트래블 시작~완료 사이 표시 |

※ **L_StageSelect 맵은 삭제되었다.** 스테이지 선택은 L_Lobby 내 O_StageSelect 오버레이가 담당한다.

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
   ├─ [스테이지 선택] (방장 전용) ──▶ (O_StageSelect)
   │       ├─ 확인 ─▶ SetStageSelection(스테이지) 후 닫기
   │       └─ 취소/ESC ─▶ 선택 변경 없이 닫기 (미선택 시 기본 1스테이지 유지)
   ├─ [준비/취소] (참가자) ─▶ Ready 토글 (Server RPC → ATCPlayerState 복제)
   ├─ [게임 시작] (방장 전용, 전원 준비완료 시 활성) ──HostStartGame()──┐
   │       ├─ [새 게임 방] ══▶ [S_Loading] ══▶ [L_Tutorial: S_Tutorial]  │
   │       └─ [이어하기 방] ══▶ [S_Loading] ══▶ [선택 스테이지 맵: S_InGame]
   │                                            (O_StageSelect 미사용 시 1스테이지)
   └─ ESC/뒤로 ──▶ (O_Confirm: 방 나가기) ══LeaveToTitle()══▶ [S_Loading] ══▶ [S_MainMenu]

[L_Tutorial: S_Tutorial] ──마지막 Step 완료/건너뛰기──▶ HostReturnToLobby()
                                        ══▶ [S_Loading] ══▶ [L_Lobby: S_Lobby]

[스테이지 맵: S_InGame] 인게임 HUD (+ 접속 로그 W_SessionLog)
   │            ──ESC──▶ (O_PauseMenu) ──[수동 저장]──▶ (O_SaveLoad)
   │                       ├─[설정]──▶ (O_Settings)
   │                       ├─[조작법]──▶ (O_KeyGuide)
   │                       ├─[게임 종료] ─▶ (O_Confirm: 방 종료) ══LeaveToTitle()══▶ [S_MainMenu]
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
| `HostReturnToLobby()` **(역할 확대)** | S_Tutorial 완료, O_Result | 세션 유지한 채 L_Lobby(S_Lobby)로 복귀 (호스트 전용) |
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
* **O_StageSelect (신규, 구 S_StageSelect):** 방장 전용 스테이지 선택 오버레이. 확인/취소/ESC.
* **O_Result (신규, 구 S_Result):** 게임 종료 시 인게임 레벨 위에 뜨는 전체화면 결과 오버레이.
* **O_Confirm:** 강제 모달 팝업. 기본 포커스는 '아니오'에 위치하여 오조작 방지.
* **O_Settings:** 오디오, 비디오, 키보드/패드 설정. 비디오 변경 시 15초 카운트다운 복구 로직.
* **O_PauseMenu:** S_InGame, S_Tutorial에서 ESC로 호출.
  * 항목: [계속하기], [설정], [수동 저장], [타이틀로 돌아가기]
  * 권한: 멀티플레이 동기화를 위해 [수동 저장]은 호스트(방장) 전용. 튜토리얼 맵에서는 강제 비활성화.
* **O_SaveLoad:** O_PauseMenu에서 [수동 저장] 선택 시 호출. 현재 상태를 슬롯에 덮어쓰거나 빈 슬롯에 기록.
* **O_KeyGuide:** 조작법 안내 오버레이.

### 공통 하위 위젯 (신규)
* **W_SessionLog (접속 로그 텍스트 칸):** 방에 접속/이탈한 유저 등 세션 이벤트 로그를 표시하는 단순 텍스트 위젯(버튼과 같은 급의 단순 위젯, `UCommonUserWidget` 상속). **S_Lobby와 S_InGame에 동일하게 배치**한다. UMockUIController의 `OnSessionLogAdded` 델리게이트를 구독하여 줄 단위로 누적 표시(최대 N줄 유지, 오래된 줄 제거).

---

## 4. 화면별 상세 기술 명세

### 1) S_MainMenu (타이틀 화면)
* **역할:** 게임의 시작점.
* **구성:** 배경 루프, 로고, 리스트 버튼(Btn_Start, Btn_Options, Btn_Credits, Btn_Quit).
* **입력 라우팅:** Btn_Start 클릭 시 O_JoinRoom 모달 팝업을 호출(PushOverlay).

### 2) S_SlotSelect (게임 선택 슬롯)
* **역할:** 세이브 데이터 진입 및 관리 (호스트 권한). **v1과 동일.**
* **구성:** 맵 썸네일과 진행도가 포함된 가로형 카드 슬롯. 내부에 명시적 삭제 버튼 [ X ] 존재.
  * ※ 미구현 요구사항: 슬롯 카드에 필요한 메타데이터(썸네일, 진행도 요약, 마지막 플레이 날짜)는 현재 UTCSaveGame 스키마에 없다. 슬롯 UI 완성 시 스키마 확장 필요(5장 참고).
* **선택 로직 (UTCSessionFlow 연동):**
  * **데이터 슬롯:** 이어하기. `SetSaveSelection(슬롯명, true)` 호출 후 `HostCreateRoom()`으로 Continue 모드 방 생성.
  * **빈 슬롯:** O_Confirm 확인 후 `SetSaveSelection(슬롯명, false)` 호출, `HostCreateRoom()`으로 NewGame 모드 방 생성.
  * 방 생성 성공 시 세션 계층이 L_Lobby로 ServerTravel한다 (구간 중 S_Loading 표시).
* **삭제 로직:** [ X ] 클릭 시 O_Confirm 호출 후 데이터 삭제.

### 3) S_Lobby (플레이어블 로비) — 신규
* **역할:** 모든 플레이어의 집결지(L_Lobby)이자 게임 준비의 허브. **S_InGame처럼 캐릭터를 직접 조작하며 돌아다닐 수 있다.**
* **구성:**
  * **로비 버튼 (호스트 = 4개 / 일반 클라이언트 = 2개):**
    | 버튼 | 노출 대상 | 동작 |
    | :--- | :--- | :--- |
    | Btn_CharacterSelect (캐릭터 선택) | 전원 | O_CharacterSelect를 PushOverlay |
    | Btn_StageSelect (스테이지 선택) | **방장 전용** | O_StageSelect를 PushOverlay |
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
* **뒤로가기:** ESC(또는 나가기 버튼) → O_Confirm(방 나가기) → 확인 시 `LeaveToTitle()`.

### 4) O_CharacterSelect (캐릭터 외형 선택 오버레이) — 구 S_CharacterSelect
* **역할:** **캐릭터의 외형을 변경하는 기능만** 가진 오버레이. 준비/시작 기능은 S_Lobby로 이관되어 이 오버레이에는 없다.
* **호출:** S_Lobby의 Btn_CharacterSelect 클릭 시 PushOverlay.
* **구성:** 외형(캐릭터/스킨) 목록, 미리보기, Btn_Close(닫기).
* **동작:**
  * 외형 선택 시 CharacterIndex를 Server RPC로 서버에 전달 → ATCPlayerState에 복제 → `OnLobbyPlayersChanged`로 전원에게 반영(로비 월드의 내 캐릭터 외형도 갱신).
  * **닫기:** Btn_Close 또는 ESC → `PopCurrentOverlay()`로 S_Lobby 복귀. (선택은 즉시 적용 방식이므로 별도 확인 버튼 없음.)
  * 활성화 중에는 GetDesiredInputConfig로 게임 입력 차단(UI 전용 입력).

### 5) O_StageSelect (스테이지 선택 오버레이) — 구 S_StageSelect, 방장 전용
* **역할:** 플레이할 스테이지를 고르는 오버레이. **방장만 열 수 있다.**
* **호출:** S_Lobby의 Btn_StageSelect(방장 전용) 클릭 시 PushOverlay.
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
* **구성:** 점수판(HUD_Score), 남은 가구 개수 표시(Txt_RemainingFurniture), **W_SessionLog(접속 로그 텍스트 칸 — S_Lobby와 동일 위젯 재사용)**.
* **하위 컴포넌트 (Sub-Widgets):**
  * **W_FurnitureStatus (가구 상태창 UI):** 화면 중앙의 크로스헤어 또는 커서가 가구에 올라갔을 때(Hover) 나타나는 툴팁 위젯. 가구 이름, 내구도 게이지를 표시.
  * **W_SessionLog:** 인게임 중 유저 입퇴장 등의 세션 로그 표시(3장).
* **연동 로직 (이벤트 주도):**
  * 크로스헤어/커서가 상호작용 대상에 올라가거나 벗어날 때, UMockUIController의 `OnInteractTargetChanged(Target, Key)` 델리게이트를 Broadcast하여 W_FurnitureStatus의 표시/숨김을 전환하고, 내구도 게이지는 `OnDurabilityChanged(Current, Max)`로 갱신한다.
  * 가구가 트럭에 실리거나/내려지거나/파괴될 때 GameMode → GameState(RemainingFurniture, RepNotify) → UMockUIController의 `OnRemainingFurnitureUpdated` 델리게이트를 거쳐 `Txt_RemainingFurniture`를 갱신.
  * 유저 입퇴장 시 GameMode → GameState 로그 복제 → `OnSessionLogAdded`로 W_SessionLog 갱신.
* **저장:** 게임 중 ESC를 눌러 호스트 권한으로 수동 저장 (O_PauseMenu 호출).
* **종료:** 게임 시간 종료/가구 전량 운반 완료 시 **O_Result가 Push된다** (아래 8항).

### 8) O_Result (최종 결과 오버레이) — 구 S_Result
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

### 9) S_Loading (로딩 화면) — 신규
* **역할:** 모든 레벨 트래블(L_Title ↔ L_Lobby ↔ L_Tutorial ↔ 스테이지 맵) 구간에 표시되는 전환 화면.
* **구성 (딱 3개 요소):**
  1. **Img_Background:** 기본 배경 사진 (전체 화면).
  2. **PB_Loading:** 화면 하단의 로딩 바 (ProgressBar).
  3. **Txt_LoadingGauge:** 로딩 게이지를 알려주는 텍스트 (예: "37%", "Loading...").
* **동작:**
  * UTCSessionFlow가 트래블 직전 `OnTravelStarted(TargetMapPath)`를 Broadcast → UMockUIController가 `ReplaceState(EE_UIState::Loading)`으로 표시.
  * 트래블 완료 후 도착 맵의 GameMode/PlayerController가 목적지 State를 ReplaceState하면서 자연히 사라진다.
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

### 12) O_PauseMenu (인게임 메뉴)
* **구성:** Btn_Resume(계속하기), Btn_KeyGuide(조작법), Btn_Settings(설정), Btn_Save(수동 저장), Btn_ToTitle(타이틀로).
* **입력 라우팅:**
  * Btn_KeyGuide 클릭 시 O_KeyGuide를 Push.
  * Btn_Settings 클릭 시 O_Settings를 Push.
  * Btn_ToTitle 클릭 시 O_Confirm 호출. 확인 시 `LeaveToTitle()`로 세션 파기 후 타이틀 복귀.

### 13) O_Settings (설정 창)
* **구성:** 하위 탭은 별도 서브 위젯 클래스로 분리 — **O_AudioSettings**(오디오), **O_GraphicsSettings**(비디오). 하위 탭은 UCommonUserWidget 상속(6장 규칙).
* **동작 로직:** 값 변경 후 적용 클릭 시 UGameUserSettings 호출. 비디오 설정 시 15초 미확인 시 이전 상태로 원복하는 안전 로직 구현.

### 14) O_SaveLoad (저장/불러오기 창)
* **동작 로직:** 인게임 메뉴에서 호출되며, 현재 진행도의 덮어쓰기 및 빈 슬롯 기록 역할만 수행.

### 15) O_KeyGuide (조작법 가이드 창)
* **역할:** 플레이어의 기본 조작법(키보드/마우스/패드) 및 상황별 단축키를 안내하는 오버레이 팝업.
* **구성:** 조작법 안내 그래픽 또는 텍스트 리스트, Btn_Back(닫기).
* **동작 로직:**
  * O_PauseMenu에서 호출되며, 생성 시 강제로 게임 입력을 차단하고 UI 전용 입력으로 전환합니다(GetDesiredInputConfig 오버라이드).
  * Btn_Back 버튼 또는 ESC 키 입력 시 UMockUIController의 PopCurrentOverlay()를 호출하여 O_PauseMenu로 복귀합니다.

---

## 5. 통합 데이터 모델 및 저장 스키마

**저장 규칙 확정:** USaveGame 객체에는 플레이어 인원, 접속자 정보 등 유동 데이터 일절 미저장. 세계 상태(맵/진행도)만 기록.

| 데이터 구조 (도메인) | 유지 방식 | 포함되는 핵심 필드 (실제 구현) |
| :--- | :--- | :--- |
| **세이브 슬롯 (디스크, UTCSaveGame)** | 영구 보존 | `StageRecords: TMap<FString, FStageRecord>` (bIsCleared, BestStar, BestScore), `LastPlayedStage`, **(확장 예정) bTutorialCompleted** |
| **멀티 로비 (복제)** | 세션 내 유지 | ATCPlayerState: `bIsReady`, `LobbySlotIndex`, `CharacterIndex` / ATCLobbyGameState: `RoomCode`, **(신규) `SelectedStageId`**, **(신규) `SessionLogEntries: TArray<FText>` (RepNotify)** — 준비/외형/입퇴장 변경 알림은 `OnLobbyPlayersChanged` 단일 경로, 로그는 `OnSessionLogAdded` 경로 |
| **스테이지 정의 (정적, 도입 예정)** | 에셋/Config | `DT_Stages` (FStageInfo: StageId, DisplayName, MapPath, …). 현재는 1스테이지(L_LevelProto) 단일 기본 항목 |
| **가구/운반 (액터)** | 스테이지 내 유지 | MaxHealth, CurrentHealth, RequiredPlayer, CurrentGrabbedPlayer, BaseScore |
| **전역 설정 (로컬)** | 클라이언트별 | MasterVolume, GraphicsQuality, InputBindings |
| **게임 진행 (GameState, 복제)** | 스테이지 내 유지 | TotalScore, RemainingFurniture, ElapsedTime, bIsGameFinished, CurrentPhase, StarCount, **(신규) SessionLogEntries** |

**세이브 스키마 미구현 요구사항:** S_SlotSelect의 슬롯 카드 UI가 요구하는 메타데이터(맵 썸네일, 진행도 요약, LastPlayedDate, CurrentTeamMoney, SaveSlotIndex)는 현재 UTCSaveGame에 없다. 슬롯 UI 완성 단계에서 스키마 확장이 필요하다. **추가로, 튜토리얼 완료 후 로비 복귀 흐름(4장-6)을 위해 bTutorialCompleted 플래그 확장이 필요하다.**

**세이브 슬롯 선택 연동:** 슬롯 확정은 `UTCSessionFlow::SetSaveSelection(SlotName, bContinue)`로, 스테이지 확정은 `SetStageSelection(StageId)`로 세션 계층에 전달되며, 세션 수명 동안 유지된다(2장). SelectedStageId는 클라이언트 표시용으로 ATCLobbyGameState에도 복제한다(방장이 고른 스테이지를 참가자가 로비에서 확인 가능).

---

## 6. 프로토타입 구현 아키텍처 규칙 (AI 코드 생성 가이드)

1. **Common UI 상속의 엄격한 분리:**
   * **최상위 화면 및 팝업:** 화면 전체를 덮는 State(S_ 계열 — S_Lobby, S_Loading 포함)와 Overlay(O_ 계열 — O_CharacterSelect, O_StageSelect, O_Result 포함)의 메인 클래스는 반드시 `UCommonActivatableWidget`을 상속받아야 합니다.
   * **하위 컴포넌트:** 설정 창 내부의 서브 탭(O_AudioSettings, O_GraphicsSettings 등)이나 리스트 내부의 슬롯 아이템, HUD 하위 위젯(W_FurnitureStatus, **W_SessionLog**, O_StageSelect의 스테이지 카드 아이템 등)은 UUserWidget이 아닌 `UCommonUserWidget`을 상속받아야 합니다.
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
   * 방장 전용 버튼(S_Lobby의 Btn_StageSelect·Btn_Start, O_Result의 Btn_ToLobby, O_PauseMenu의 Btn_Save)의 노출/활성 분기는 `UTCSessionFlow::IsHost()`로 판정합니다. 단, 노출 분기는 편의일 뿐 권위가 아니므로, 실제 트래블/저장 실행은 서버(호스트) 측에서 재검증합니다.

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
1. `OnTeamMoneyUpdated(int32 NewTotalMoney)`: 팀 보유 금액 변경.
2. `OnFurnitureSettled(int32 AddedMoney, int32 Grade)`: 가구 정착 및 정산.
3. `OnInteractTargetChanged(AActor* Target, FString Key)`: 상호작용 대상 변경. W_FurnitureStatus의 표시/숨김 전환에 사용.
4. `OnDurabilityChanged(float Current, float Max)`: 가구 내구도 변경. W_FurnitureStatus 게이지 갱신에 사용.
5. `OnLobbySlotUpdated(int32 SlotIndex, FPlayerInfo PlayerInfo)`: 프로토타입 잔재. 실제 로비 갱신은 ATCLobbyGameState의 `OnLobbyPlayersChanged`를 사용한다.
6. `OnStateChanged(EE_UIState NewState)`: UI 상태(화면) 전환 통지.
7. `OnRemainingFurnitureUpdated(int32 NewCount)`: GameState의 RemainingFurniture가 갱신될 때 Broadcast. S_InGame의 `Txt_RemainingFurniture` 갱신에 사용.
8. `OnGameResultReady(int32 FinalScore, int32 StarCount, float ElapsedTime)`: `TriggerGameResult()` 호출 시 Broadcast. **O_Result**는 위젯 생성 시점에 이미 캐시된 값을 직접 읽어가므로(`GetLastFinalScore`/`GetLastStarCount`/`GetLastElapsedTime`), 이 델리게이트는 추후 다른 상시 존재 위젯이 결과를 참조해야 할 경우를 위한 확장 포인트다.
9. `OnSessionLogAdded(FText LogMessage)` **(신규)**: 세션 로그(유저 입장/퇴장 등) 항목이 추가될 때 Broadcast. S_Lobby·S_InGame의 W_SessionLog가 구독하여 줄을 누적한다. 원천은 GameMode(PostLogin/Logout) → GameState 복제 배열(RepNotify) → 본 델리게이트 순.

**컨트롤러 외부 델리게이트 (UI가 함께 구독):**
* `OnSessionPhaseChanged(ETCSessionPhase, FString)` — UTCSessionFlow. 세션 로딩/에러 표시(2장).
* `OnTravelStarted(FString TargetMapPath)` — UTCSessionFlow **(신규)**. S_Loading 표시 트리거(2장, 4장-9).
* `OnLobbyPlayersChanged()` — ATCLobbyGameState. 로비 표시 갱신 트리거(4장-3).

### 5) FFurnitureStatusData (가구 상태 정보 구조체 — 도입 예정, 미구현)
인게임에서 가구를 바라볼 때(Hover) W_FurnitureStatus UI에 전달할 데이터 묶음입니다. USTRUCT(BlueprintType)으로 선언하며, 멤버 변수에 `UPROPERTY(EditAnywhere, BlueprintReadWrite)`를 적용합니다.
* FString FurnitureName;
* float CurrentDurability;
* float MaxDurability;

**현재 상태:** 구조체는 아직 코드에 없으며, `UW_FurnitureStatus::UpdateFurnitureStatus(Name, CurrentDurability, MaxDurability)` 개별 파라미터 임시 인터페이스를 사용 중이다. 백엔드 연동 단계에서 구조체 단일 파라미터로 교체한다.

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