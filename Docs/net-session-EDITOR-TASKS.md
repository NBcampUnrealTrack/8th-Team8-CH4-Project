# 세션 UI 바인딩 — 에디터/수동 작업 체크리스트

> 코드(`feat/net-session-ui-binding` 브랜치)로 **자동화할 수 없는** 항목.
> 사람이 언리얼 에디터에서 하거나, 환경/계정을 결정해야 한다.
> 코드 설계 근거는 [`net-session-ui-flow.md`](net-session-ui-flow.md) 참고.

체크박스 형식: `[ ]` = 해야 할 일.

---

## 1. GameInstance / SessionFlow 설정

- [ ] **GameInstanceClass = `TCGameInstance`** 확인
  - Project Settings → Maps & Modes → Game Instance Class.
  - 이미 `Config/DefaultEngine.ini` 에 `GameInstanceClass=/Script/TeamCarry.TCGameInstance` 설정됨. BP GameInstance 를 쓴다면 그 부모를 `TCGameInstance` 로.
- [ ] **레벨 경로를 실제 맵으로 지정** — `UTCSessionFlow` 의 기본 경로는 **플레이스홀더**다.
  `Config/DefaultGame.ini` 에 아래 섹션을 추가하고 **실제 맵 경로**로 채운다:
  ```ini
  [/Script/TeamCarry.TCSessionFlow]
  TitleMapPath=/Game/Developers/MinkiCho/Level/Title
  LobbyMapPath=/Game/Maps/L_Lobby
  TutorialMapPath=/Game/Maps/L_Tutorial
  StageSelectMapPath=/Game/Maps/L_StageSelect
  MaxPlayers=4
  ```
  > 맵이 아직 없으면 **빈 레벨이라도 생성**해야 ServerTravel 대상이 된다(§4).

---

## 2. PlayerController / GameMode 클래스 배정 (레벨별)

코드가 만든 C++ 클래스를 각 레벨의 GameMode 에 연결해야 복제가 동작한다.

- [ ] **로비 레벨 GameMode** → `ATCLobbyGameMode` (또는 그 BP 서브클래스 `BP_LobbyGameMode`)
  - 이 GameMode 가 자동으로 `ATCLobbyGameState` / `ATCPlayerState` / `ATCPlayerController` 를 사용한다.
  - World Settings → GameMode Override = `BP_LobbyGameMode`.
- [ ] **나머지 인게임/튜토리얼/스테이지 레벨**의 PlayerControllerClass 도 `ATCPlayerController` 로
  통일(또는 BP 서브클래스). UI 호스팅(`AGameUIPlayerController`) 능력을 그대로 상속하므로 교체해도 UI 정상.
  - 기존 `BP_TeamCarryGameMode` / `BP_MainMenuGameMode` 의 PlayerControllerClass 를 `ATCPlayerController` 기반으로 변경.
- [ ] **메인메뉴 레벨**은 복제가 필요 없으므로 기존 `AGameUIPlayerController` 유지 가능
  (단, 통일하고 싶으면 `ATCPlayerController` 로 바꿔도 무방).

---

## 3. 각 레벨 PlayerController BP 의 `InitialState` / 위젯 맵 지정

`AGameUIPlayerController` 는 `InitialState` 와 `ScreenWidgetClasses`(State→WBP) 를 에디터에서 받는다.
**레벨마다 다른 PC BP** 를 두거나, 같은 BP 를 쓰되 레벨별 GameMode 에서 다른 PC BP 를 지정한다.

- [ ] 로비 레벨 PC BP: `InitialState = CharacterSelect`
- [ ] 튜토리얼 레벨 PC BP: `InitialState = Tutorial`
- [ ] 스테이지선택 레벨 PC BP: `InitialState = StageSelect`
- [ ] 인게임 레벨 PC BP: `InitialState = InGame`
- [ ] 각 PC BP 의 `ScreenWidgetClasses` 맵에 해당 State→WBP_* 위젯, `RootLayoutClass` 지정

> 이렇게 하면 호스트가 ServerTravel 하면 클라가 추종 → 새 레벨의 PC.BeginPlay 가
> 그 레벨의 InitialState 화면을 자동으로 띄운다. (코드는 트래블만, 화면은 레벨 설정이 결정)

---

## 4. 맵(레벨) 생성

- [ ] **L_Lobby** 신규 레벨 — World Settings GameMode = `BP_LobbyGameMode`.
  - 캐릭터 프리뷰가 필요하면 `ATCLobbyGameMode::DefaultPawnClass` 를 BP 에서 지정(기본 nullptr=UI만).
- [ ] **L_Tutorial / L_StageSelect** 신규 레벨(또는 기존 프로토 레벨 재사용).
- [ ] 인게임 스테이지 맵들 — 기존 `L_FurnitureProto` 등 활용.
- [ ] §1 의 ini 경로를 위 맵 경로와 정확히 일치시킬 것.

---

## 5. WBP 위젯 ↔ C++ seam 연결

코드가 호출 지점(seam)을 열어뒀다. WBP 에서 아래를 연결한다.

- [ ] **WBP_SlotSelect**: 슬롯 카드 "선택/확정" 버튼 → C++ `ConfirmSlotAndCreateRoom(SlotName, bContinue)` 호출
  - `bContinue`: 저장 데이터가 있는 슬롯이면 true(이어하기), 새 슬롯이면 false(새 게임).
- [ ] **WBP_JoinRoom**: 코드 입력 후 '방 참가' 버튼은 이미 `JoinRoomByCode` 를 부른다(C++).
  - '방 만들기' 버튼은 `S_SlotSelect`(SlotSelect 상태) 로 이동하도록 BP 라우팅 확인.
- [ ] **WBP_CharacterSelect**: `Btn_Ready` / `Btn_Start` 는 C++ 가 자동 분기(네트워크/mock).
  - 캐릭터 선택 UI 가 생기면 선택 시 `ATCPlayerController::RequestSetCharacterIndex(Index)` 호출 연결.
  - 슬롯 표시용 `Slot1~4_Name/Status` 위젯명을 WBP 에 맞춰 바인딩(BindWidgetOptional).
  - **방 코드 표시**: 로비(호스트) 화면에 `UTCSessionFlow::GetRoomCode()` 텍스트 바인딩 →
    호스트가 코드를 보고 클라에게 공유. (값은 방 생성 직후 채워짐)
- [ ] **WBP_JoinRoom**: 코드 입력칸(`Txt_Code`)에 호스트가 공유한 코드 입력 후 '방 참가'.
  - 대소문자·공백은 C++ 가 정규화하므로 사용자가 아무렇게 입력해도 됨.
  - 일치 방이 없으면 `OnSessionPhaseChanged(Failed, "코드 ... 와 일치하는 방이 없습니다")` 통지.
- [ ] (선택) **WBP** 에 `UTCSessionFlow::OnSessionPhaseChanged` 구독 → 로딩/에러 토스트 표시.

---

## 6. Steam 실접속 환경 (사람이 결정)

> [`steam-oss.md`](steam-oss.md) §5 와 동일. 핵심만 재기재.

- [ ] **PC 2대(또는 2계정)** — Steam 은 PC당 1계정이라 같은 PC 2인스턴스 실접속 불가.
- [ ] **`steam_appid.txt`(480)** 프로젝트 루트 존재 — Standalone 실행 시 필요.
- [ ] **Standalone Game** 으로 실행(에디터 PIE 아님)해야 Steam 세션 의미 있음.
- [ ] PIE 빠른 검증: Net Mode = Play As Listen Server, Players 2+ → **복제(Ready/슬롯)** 만 검증
  (세션 검색/조인은 LAN/NULL 경로).

---

## 7. 확장(후속, 코드 TODO 와 연결)

- [x] ~~**방 코드 매칭**~~ — ✅ 구현 완료. 호스트 6자리 코드 광고 + 클라 코드 일치 조인.
  남은 건 WBP 표시/입력 연결뿐(§5).
- [ ] **캐릭터 중복 방지** — `ATCLobbyGameMode` 서버에서 동일 CharacterIndex 거부.
- [ ] **세이브 직렬화** — `SetSaveSelection` 의 SlotName 을 실제 SaveGame 로드/생성과 연결.
- [ ] **S_StageSelect → 스테이지 맵 트래블** — 스테이지 리스트 항목이 맵 경로를 들고,
  호스트가 `UTCSessionFlow::HostTravelToStage(MapPath)` 를 부르도록 WBP/데이터 연결.

---

## 8. 빌드 확인

- [ ] 솔루션 리제너레이트 후 **에디터에서 C++ 컴파일**(신규 클래스 5종 + 수정 위젯).
  - 신규: `TCSessionFlow`, `TCLobbyGameMode`, `TCLobbyGameState` / 수정: `TCPlayerState`, `TCPlayerController`, UI 위젯들.
- [ ] 컴파일 후 위 §2~§5 의 BP 배정/연결 진행.
