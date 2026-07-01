# 세션 UI ↔ Steam 네트워크 바인딩 설계/구현 계획 — TeamCarry

> 네트워크 팀원 전달용. 명세(호스트/클라이언트 세션 흐름)를 **기존 코드 구조** 위에 어떻게
> 얹는지 정의한다. 이 문서는 **코드 바인딩 계층(seam)** 의 기준 문서이고,
> 사람이 에디터에서 해야 하는 일은 [`net-session-EDITOR-TASKS.md`](net-session-EDITOR-TASKS.md) 에 분리했다.
>
> 브랜치: `feat/net-session-ui-binding` (develop 분기)

---

## 0. 현재 구조 진단 (왜 그대로는 안 붙나)

| 레이어 | 클래스 | 현재 상태 |
|---|---|---|
| 세션(Steam) | `UTCGameInstance` | Host/Find/Join/Destroy + BP 델리게이트 **완성**. 단, 아무도 호출 안 함 |
| UI 상태기계 | `UMockUIController` (GI Subsystem) | `ReplaceState`/`Push/Pop` 으로 **로컬 화면만** 교체. 로비 데이터는 **목(mock)** |
| UI 호스트 | `AGameUIPlayerController` (`IUIHost`) | 위젯 생성/뷰포트 추가 담당 |
| 위젯 | `O_JoinRoom`, `S_SlotSelect`, `S_CharacterSelect` … | 전부 `MockUIController->ReplaceState(...)` 만 호출 |
| 복제 | `ATCPlayerState`, `ATCPlayerController` | **빈 껍데기** (복제 변수/RPC 없음) |

**핵심 간극 3가지**
1. **세션 미연결** — UI 버튼이 `UTCGameInstance` 세션 API를 한 번도 안 부른다.
2. **레벨 트래블 부재** — 명세는 Tutorial/StageSelect/InGame/Result 를 **별 레벨 + ServerTravel** 로 보는데,
   프로토타입은 한 레벨 안에서 풀스크린 위젯만 갈아끼운다(`ReplaceState`).
3. **로비 복제 부재** — `bIsReady`/`CharacterIndex` 가 mock. 실제 `PlayerState` 복제가 없다.

이 셋을 메우는 **얇은 바인딩 계층**을 추가한다. 기존 mock 경로는 **싱글/에디터 폴백**으로 보존한다.

---

## 1. 추가하는 바인딩 계층 (이 브랜치 산출물)

```
        UI 위젯 (O_JoinRoom / S_SlotSelect / S_CharacterSelect …)
              │ 의도(intent) 호출                  ▲ 상태/로비 갱신 구독
              ▼                                    │
   ┌─────────────────────────────┐      ┌────────────────────────────┐
   │  UTCSessionFlow             │      │  ATCLobbyGameState         │
   │  (GameInstanceSubsystem)    │      │  OnLobbyPlayersChanged     │
   │  - 세이브슬롯/이어하기 보관  │      │  AreAllPlayersReady()      │
   │  - 레벨경로 설정             │      └──────────▲─────────────────┘
   │  - Host/Join/Start/Leave    │                 │ OnRep 알림
   │  - ServerTravel/ClientTravel│      ┌──────────┴─────────────────┐
   └───────────┬─────────────────┘      │  ATCPlayerState            │
               │ 위임                    │  bIsReady/CharacterIndex   │
               ▼                         │  LobbySlotIndex (복제)     │
   ┌─────────────────────────────┐      └──────────▲─────────────────┘
   │  UTCGameInstance (기존)     │                 │ Server RPC
   │  HostSteamSession 등        │      ┌──────────┴─────────────────┐
   └─────────────────────────────┘      │  ATCPlayerController        │
                                        │  : AGameUIPlayerController   │
                                        │  ServerSetReady / …Start     │
                                        └──────────────────────────────┘
```

신규/수정 파일:

| 파일 | 종류 | 역할 |
|---|---|---|
| `Network/Session/TCSessionFlow.{h,cpp}` | **신규** | UI↔세션 단일 seam. 의도 API + 레벨 트래블 오케스트레이션 |
| `Network/Session/TCLobbyGameMode.{h,cpp}` | **신규** | 로비 레벨 GameMode. 슬롯 배정·시작 게이팅·ServerTravel |
| `Network/Session/TCLobbyGameState.{h,cpp}` | **신규** | 로비 복제 상태. `OnLobbyPlayersChanged`, `AreAllPlayersReady` |
| `Player/PlayerState/TCPlayerState.{h,cpp}` | 수정 | `bIsReady`/`CharacterIndex`/`LobbySlotIndex` 복제 + OnRep |
| `Player/PlayerController/TCPlayerController.{h,cpp}` | 수정 | `AGameUIPlayerController` 상속 + 로비 Server RPC |
| `UI/S_CharacterSelect.cpp` | 수정 | Ready/Start 를 RPC/GameState 로 라우팅(네트워크 시) + mock 폴백 |
| `UI/S_SlotSelect.{h,cpp}` | 수정 | 슬롯 확정 시 `UTCSessionFlow::SetSaveSelection` + `HostCreateRoom` |
| `UI/O_JoinRoom.cpp` | 수정 | 만들기→SlotSelect, 참가→`JoinRoomByCode` |

---

## 2. 명세 흐름 → 코드 매핑

### 2.1 호스트(Listen Server)

| 명세 단계 | 코드 |
|---|---|
| ① O_JoinRoom '방 만들기' | `O_JoinRoom::HandleCreateRoomClicked` → `MockUIController::ReplaceState(SlotSelect)` (로컬, 레벨 동일) |
| ② S_SlotSelect 데이터 선택 | 슬롯 카드 확정 → `UTCSessionFlow::SetSaveSelection(Slot, bContinue)` |
| ③ 세션 생성 + 로비로 ServerTravel | `UTCSessionFlow::HostCreateRoom()` → `GI->HostSteamSession(LobbyMap, 4)` → 콜백서 `?listen` ServerTravel **이미 구현됨** |
| ④ 로비 진입·캐릭터·Ready 복제 | 로비 레벨 GameMode=`ATCLobbyGameMode`. `S_CharacterSelect` 는 GameUIPlayerController.InitialState 로 자동 표시. Ready→`ServerSetReady` |
| 시작 조건 | `ATCLobbyGameState::AreAllPlayersReady()` true → 호스트 `Btn_Start` 활성 |
| 시작 클릭 | `ServerRequestStartGame` → `ATCLobbyGameMode::TravelToGameplay()` |
| ⑤ 새 게임/이어하기 분기 | `bContinue==false` → `TutorialMap` ServerTravel / `true` → `StageSelectMap` |
| ⑥ S_StageSelect 스테이지 확정 | (호스트 독점) `UTCSessionFlow::HostTravelToStage(StageMap)` → ServerTravel |
| ⑦ S_InGameHUD | 인게임 레벨. 가구/점수 복제는 기존 `Network/Carry` 계열 담당 |
| ⑧ S_Result | 스테이지 클리어 판정(서버) → 결과 위젯 |
| 타이틀로 | `UTCSessionFlow::LeaveToTitle()` → `DestroySteamSession` → 타이틀 맵 오픈 |
| 로비로 | `UTCSessionFlow::HostReturnToLobby()` → 세션 유지 + `LobbyMap` ServerTravel |

### 2.2 클라이언트

| 명세 단계 | 코드 |
|---|---|
| ① O_JoinRoom 코드 입력+접속 | `O_JoinRoom::HandleJoinRoomClicked` → `UTCSessionFlow::JoinRoomByCode(Code)` |
| ② 세션 검색→조인 | `GI->FindSteamSessions` → 완료 콜백서 매칭 세션 `GI->JoinFoundSession(i)` |
| ③ 로비로 ClientTravel | `JoinFoundSession` 성공 콜백서 `GetResolvedConnectString`→`ClientTravel` **이미 구현됨** |
| ④ 캐릭터 선택 + Ready | 호스트와 동일 위젯/RPC. `Btn_Start` 는 클라에선 항상 숨김/비활성 |
| ⑤ 이후 강제 이동 | 호스트 ServerTravel → 클라 자동 추종(엔진 기본). 별도 코드 불필요 |

> **방 코드(Room Code)**: 구현됨. 호스트는 `HostSteamSession` 에서 6자리 코드(혼동문자 제외
> `A-Z2-9`)를 생성해 `TCRoomCode` 키로 광고하고 `GetHostRoomCode()`(=`UTCSessionFlow::GetRoomCode()`)
> 로 노출한다. 클라는 `JoinRoomByCode(Code)` → 검색 결과 중 광고 코드가 일치하는 세션을 조인한다
> (대소문자·공백 정규화 후 비교, 불일치 시 실패 통지).

---

## 3. 레벨/트래블 정책

- **레벨 내 화면**(MainMenu↔SlotSelect↔JoinRoom 팝업): `MockUIController` 로컬 라우팅 유지.
- **레벨 경계**(Lobby→Tutorial→StageSelect→InGame→Result): **호스트 ServerTravel** 만 사용.
  클라는 추종. `UTCSessionFlow` 가 `EE_UIState`→맵경로 매핑을 보관(에디터 설정 가능).
- 각 레벨의 PlayerController(BP) 는 `InitialState` 를 그 레벨에 맞는 화면으로 지정
  (로비=CharacterSelect, 튜토리얼=Tutorial …). → 에디터 작업(EDITOR-TASKS §3).

---

## 4. 확장 지점 (후속 작업, 이 브랜치 범위 밖)

1. ~~**방 코드 광고/검색**~~ — ✅ 구현 완료(§2.2 참고).
2. **캐릭터 중복 선택 방지** — `ATCLobbyGameMode` 에서 서버 검증.
3. **세이브 데이터 직렬화** — `SetSaveSelection` 의 SlotName 을 실제 SaveGame 로드와 연결.
4. **스테이지별 InGame 맵 테이블** — `UTCSessionFlow` 에 `TMap<FName,TSoftObjectPtr<UWorld>>`.

---

## 5. 폴백/호환

- OSS(Steam) 미초기화 시 `HostSteamSession` 이 LAN 리슨서버로 폴백(기존 동작).
- 네트워크 레이어(`ATCLobbyGameState`) 부재(=프로토타입 단일레벨)면 `S_CharacterSelect` 는
  기존 `MockUIController` 목 데이터 경로로 동작 → **프로토타입 깨지지 않음**.
- 모든 신규 복제 변수는 서버 권위에서만 set. RPC 는 `WithValidation` 없이 `Server, Reliable`.

---

## 6. 참고
- 세션 코드: `Source/TeamCarry/Network/Session/TCGameInstance.{h,cpp}`
- Steam 가이드: [`steam-oss.md`](steam-oss.md)
- 에디터 수동 작업: [`net-session-EDITOR-TASKS.md`](net-session-EDITOR-TASKS.md)
