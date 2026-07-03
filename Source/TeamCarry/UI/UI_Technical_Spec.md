# UI_Technical_Spec.md
# 이사 협동 게임 UI 최종 기획 및 기술 명세서

## 1. UI 전체 구조도 (화면 흐름도)
[S_Boot] 로고/인트로
   │
   ▼
[S_MainMenu] 타이틀 ──게임 종료(버튼/ESC)──▶ (O_Confirm) ──예──▶ 앱 종료
   ├─ 게임 시작 ───────────────▶ (O_JoinRoom) 통합 접속 팝업
   ├─ 옵션 ────────────────────▶ (O_Settings)
   └─ 크레딧 ──────────────────▶ [크레딧 스크롤] ──Esc──▶ 복귀
        │
(O_JoinRoom) ──취소(ESC)──▶ 팝업 닫기
   ├─ 방 만들기 버튼 선택 ────────▶ [S_SlotSelect] (세이브 슬롯 관리)
   └─ 방 참가 버튼 선택 + 코드 입력 ───▶ [S_CharacterSelect] (게스트 로비 진입)
        │
[S_SlotSelect] 게임 선택 슬롯 ──뒤로(ESC)──▶ [S_MainMenu]
   ├─ 기존 데이터 삭제 버튼 클릭 ─▶ (O_Confirm: 재확인) ─▶ 기존 데이터 삭제
   ├─ 데이터 슬롯 선택(이어하기) ─▶ (맵·진행도 로드) ─────┐
   └─ 빈 슬롯 선택(새 게임) ─▶ (새 세이브 할당) ─────────┤
        │                                            │ (두 경우 모두 로비로 집결)
        ▼                                            ▼
[S_CharacterSelect] 캐릭터 선택/로비 ──뒤로──▶ (O_Confirm: 방 종료) ──▶ [S_MainMenu]
   └─ 전원 준비완료 → 호스트 '시작' 버튼 클릭 → 즉시 전환 (카운트다운 없음)
        │
        ├─ [새 게임 방] ──▶ [S_Tutorial] ──마지막 Step 완료/건너뛰기─┐
        │                                                        │
        └─ [이어하기 방] ─────────────────────────────────────────┤
                                                                ▼
[S_StageSelect] 스테이지 선택 ──뒤로──▶ (O_Confirm) ──▶ [S_MainMenu]
   └─ 스테이지 선택 및 진입
        │
        ▼
[S_InGame] 인게임 HUD ──ESC──▶ (O_PauseMenu) ──[수동 저장]──▶ (O_SaveLoad)
   │                                 ├─[설정]──▶ (O_Settings)
   │                                 ├─[조작법]──▶ (O_KeyGuide)
   │                                 ├─[게임 종료] ──▶ (O_Confirm: 방 종료) ──▶ [S_MainMenu]
   │                                 └─ESC (O_PauseMenu 닫기)──▶ 인게임 HUD
   └─ 게임 시간 종료
        │
        ▼
[S_Result] 최종 결과 ──확인──▶ [S_StageSelect] 또는 [S_MainMenu]

---

## 2. 화면(State) 목록 및 공통 오버레이 명세

### 화면(State) 목록
| State 명칭 | 종류 | 진입 경로 | 일시정지 | 비고 |
| :--- | :--- | :--- | :--- | :--- |
| **S_Boot** | 풀스크린 | 앱 실행 | X | 로고/인트로 연출 |
| **S_MainMenu** | 풀스크린 | Boot 완료 | X | 메인 타이틀 화면 |
| **S_SlotSelect** | 풀스크린 | O_JoinRoom에서 방 만들기 선택 | X | 가로형 카드 배치 (이어/새 게임 통합) |
| **S_CharacterSelect**| 풀스크린 | 슬롯 선택 또는 방 참가 | X | 멀티 로비 겸 캐릭터 선택 |
| **S_Tutorial** | 풀스크린 | 새 게임 로비 시작 직후 | O | 수동 저장 비활성 |
| **S_StageSelect** | 풀스크린 | 튜토리얼 종료 / 이어하기 | X | 스테이지 목록 및 퀘스트 수주 |
| **S_InGame** | 풀스크린 | 스테이지 선택 진입 | O | 코어 루프 HUD |
| **S_Result** | 풀스크린 | 가구 전량 운반 완료 | X | 점수 정산 및 통계 |

### 공통 컴포넌트 명세 (Overlay)
* **통합 라우팅 규칙:** 풀스크린은 교체(Replace), 오버레이는 누적(Push/Pop) 방식. ESC는 '한 단계 뒤로/닫기' 통일.
* **O_JoinRoom:** 게임 시작 시 호출되는 통합 방 생성/참가 모달 팝업.
* **O_Confirm:** 강제 모달 팝업. 기본 포커스는 '아니오'에 위치하여 오조작 방지.
* **O_Settings:** 오디오, 비디오, 키보드/패드 설정. 비디오 변경 시 15초 카운트다운 복구 로직.
* **O_PauseMenu:** S_InGame, S_Tutorial에서 ESC로 호출.
  * 항목: [계속하기], [설정], [수동 저장], [타이틀로 돌아가기]
  * 권한: 멀티플레이 동기화를 위해 [수동 저장]은 호스트(방장) 전용. 튜토리얼 맵에서는 강제 비활성화.
* **O_SaveLoad:** O_PauseMenu에서 [수동 저장] 선택 시 호출. 현재 상태를 슬롯에 덮어쓰거나 빈 슬롯에 기록.

---

## 3. 화면별 상세 기술 명세

### 1) S_MainMenu (타이틀 화면)
* **역할:** 게임의 시작점.
* **구성:** 배경 루프, 로고, 리스트 버튼(Btn_Start, Btn_Options, Btn_Credits, Btn_Quit).
* **입력 라우팅:** Btn_Start 클릭 시 O_JoinRoom 모달 팝업을 호출(PushOverlay).

### 2) S_SlotSelect (게임 선택 슬롯)
* **역할:** 세이브 데이터 진입 및 관리 (호스트 권한).
* **구성:** 맵 썸네일과 진행도가 포함된 가로형 카드 슬롯. 내부에 명시적 삭제 버튼 [ X ] 존재.
* **선택 로직:**
  * **데이터 슬롯:** 이어하기. 데이터 로드 후 Continue 모드 방 생성.
  * **빈 슬롯:** O_Confirm 확인 후 새 게임 할당, NewGame 모드 방 생성.
* **삭제 로직:** [ X ] 클릭 시 O_Confirm 호출 후 데이터 삭제.

### 3) S_CharacterSelect (캐릭터 선택 및 멀티 로비)
* **역할:** 모든 플레이어 집결지.
* **구성:** 1P~4P 가로 슬롯, 방 코드, 호스트 전용 Btn_Start, 참가자 전용 Btn_Ready.
* **규칙:** 1P 호스트 고정. 중도 이탈 시 슬롯 번호 유지. 방 코드로 친구 초대.
* **시작 로직:** 모든 점유 슬롯 bIsReady = true 시 호스트 시작 버튼 활성화. 클릭 시 카운트다운 없이 즉시 전환.

### 4) S_Tutorial (튜토리얼)
* **역할:** NewGame 방 최초 1회 학습 맵.
* **동작:** 잡기/이동/놓기/적재. 1명만 성공해도 다음 단계 진행. 달성 시 S_StageSelect 직행.

### 5) S_StageSelect (스테이지 선택)
* **역할:** 플레이할 맵 선택 화면.
* **연동:** 맵 선택 후 진입 시 메모리에 로드된 세이브 데이터 갱신(저장) 후 S_InGame 씬 전환.

### 6) S_InGame (인게임 HUD)
* **역할:** 가구 운반 코어 루프 진행 및 실시간 정보 제공.
* **구성:** 점수판(HUD_Score), 남은 가구 개수 표시(Txt_RemainingFurniture).
* **하위 컴포넌트 (Sub-Widgets):**
  * **W_FurnitureStatus (가구 상태창 UI):** 화면 중앙의 크로스헤어 또는 커서가 가구에 올라갔을 때(Hover) 나타나는 툴팁 위젯. 가구 이름, 내구도 게이지를 표시.
* **연동 로직 (이벤트 주도):**
  * 가구 액터에 마우스를 올리거나 벗어날 때, UMockUIController의 `OnFurnitureHovered` 델리게이트를 Broadcast하여 W_FurnitureStatus 갱신.
  * (신규) 가구가 트럭에 실리거나/내려지거나/파괴될 때 GameMode → GameState(RemainingFurniture, RepNotify) → UMockUIController의 `OnRemainingFurnitureUpdated` 델리게이트를 거쳐 `Txt_RemainingFurniture`를 갱신.
* **저장:** 게임 중 ESC를 눌러 호스트 권한으로 수동 저장 (O_PauseMenu 호출).

### 7) S_Result (최종 결과)
* **정산:** 남은 내구도에 따라 0~5 등급. 점수 산정 후 Team_Money 표기. 확인 누르면 화면 이탈.
* **구성:** 최종 점수(Txt_Score), 세부 통계(Txt_Stats), (신규) 획득한 별 개수(Txt_StarCount, 0~3개).
* **연동 로직 (이벤트 주도):**
  * GameMode가 게임 종료를 확정하면(FinishGame) GameState의 `TotalScore`, `StarCount`, `bIsGameFinished`가 함께 갱신되고, `OnRep_bIsGameFinished`가 UMockUIController의 `TriggerGameResult(FinalScore, StarCount)`를 호출한다.
  * `TriggerGameResult`는 값을 컨트롤러 내부에 캐시한 뒤 `ReplaceState(EE_UIState::Result)`로 화면을 전환한다. S_Result는 `NativeConstruct` 시점에 `GetLastFinalScore()`/`GetLastStarCount()`로 캐시된 값을 즉시 읽어 `Txt_Score`/`Txt_StarCount`에 반영한다(위젯 생성이 델리게이트 브로드캐스트보다 먼저 동기적으로 일어나기 때문).

### 8) O_JoinRoom (통합 접속 팝업)
* **역할:** 호스트의 방 생성과 클라이언트의 방 참가를 분기하는 모달 창.
* **구성:** '방 만들기' 선택 버튼, '방 참가' 선택 버튼, 코드 입력란(EditableTextBox), Btn_Cancel(취소).
* **동작 및 시각적 피드백:**
  * 항목('방 만들기' 또는 '방 참가') 클릭 시, 확대되는 효과를 주며, 버튼이 제대로 눌렸음을 명시.
  * 방 참가 버튼 클릭 시, 코드 입력란의 문자열이 유효한 경우에만 참가 로직을 실행하며, 아닌 경우 에러 팝업 모달 출력.
* **입력 라우팅:**
  * '방 만들기' 클릭: O_JoinRoom을 닫고 S_SlotSelect로 화면 교체.
  * '방 참가' 클릭: 코드를 검증하여 성공 시 S_CharacterSelect로 화면 교체, 실패 시 에러 안내 모달 팝업 호출.
  * 취소 버튼 또는 ESC: O_JoinRoom을 닫고 S_MainMenu로 복귀.

### 9) O_Confirm (확인 팝업)
* **구성:** 제목, 설명 텍스트, Btn_Yes, Btn_No.
* **동작 로직:** 생성 시 강제 모달(Modal). Btn_No에 기본 포커스. Btn_Yes 클릭 시 전달받은 콜백(Delegate) 실행 후 스택에서 Pop.

### 10) O_PauseMenu (인게임 메뉴)
* **구성:** Btn_Resume(계속하기), Btn_KeyGuide(조작법), Btn_Settings(설정), Btn_Save(수동 저장), Btn_ToTitle(타이틀로).
* **입력 라우팅:**
  * Btn_KeyGuide 클릭 시 O_KeyGuide를 Push.
  * Btn_Settings 클릭 시 O_Settings를 Push.
  * Btn_ToTitle 클릭 시 O_Confirm 호출.

### 11) O_Settings (설정 창)
* **동작 로직:** 값 변경 후 적용 클릭 시 UGameUserSettings 호출. 비디오 설정 시 15초 미확인 시 이전 상태로 원복하는 안전 로직 구현.

### 12) O_SaveLoad (저장/불러오기 창)
* **동작 로직:** 인게임 메뉴에서 호출되며, 현재 진행도의 덮어쓰기 및 빈 슬롯 기록 역할만 수행.

### 13) O_KeyGuide (조작법 가이드 창)
* **역할:** 플레이어의 기본 조작법(키보드/마우스/패드) 및 상황별 단축키를 안내하는 오버레이 팝업.
* **구성:** 조작법 안내 그래픽 또는 텍스트 리스트, Btn_Back(닫기).
* **동작 로직:**
  * O_PauseMenu에서 호출되며, 생성 시 강제로 게임 입력을 차단하고 UI 전용 입력으로 전환합니다(GetDesiredInputConfig 오버라이드).
  * Btn_Back 버튼 또는 ESC 키 입력 시 UMockUIController의 PopCurrentOverlay()를 호출하여 O_PauseMenu로 복귀합니다.

---

## 4. 통합 데이터 모델 및 저장 스키마

**저장 규칙 확정:** USaveGame 객체에는 플레이어 인원, 접속자 정보 등 유동 데이터 일절 미저장. 세계 상태(맵/진행도)만 기록.

| 데이터 구조 (도메인) | 유지 방식 | 포함되는 핵심 필드 (예시) |
| :--- | :--- | :--- |
| **세이브 슬롯 (디스크)** | 영구 보존 | UnlockedStages, ClearedStages, CurrentTeamMoney, LastPlayedDate, SaveSlotIndex |
| **멀티 로비 (메모리)** | 세션 내 유지 | SlotIndex, PlayerName, SelectedCharacterID, bIsReady, RoomCode |
| **가구/운반 (액터)** | 스테이지 내 유지 | MaxHealth, CurrentHealth, RequiredPlayer, CurrentGrabbedPlayer, BaseScore |
| **전역 설정 (로컬)** | 클라이언트별 | MasterVolume, GraphicsQuality, InputBindings |
| **게임 진행 (GameState, 복제)** | 스테이지 내 유지 | TotalScore, `RemainingFurniture`(신규), ElapsedTime, bIsGameFinished, CurrentPhase, `StarCount`(신규) |

---

## 5. 프로토타입 구현 아키텍처 규칙 (AI 코드 생성 가이드)

1. **Common UI 상속의 엄격한 분리:**
   * **최상위 화면 및 팝업:** 화면 전체를 덮는 State(S_ 계열)와 Overlay(O_ 계열)의 메인 클래스는 반드시 `UCommonActivatableWidget`을 상속받아야 합니다.
   * **하위 컴포넌트:** 설정 창 내부의 서브 탭(UO_GraphicsSettings 등)이나 리스트 내부의 슬롯 아이템 등은 UUserWidget이 아닌 `UCommonUserWidget`을 상속받아야 합니다.
2. **입력 라우팅 (Input Config) 관리:**
   * 오버레이 위젯은 GetDesiredInputConfig()를 오버라이드하여, 활성화 시 게임 입력을 막고 UI 전용 입력(Menu)으로 전환되도록 설정해야 합니다.
3. **UMockUIController (중앙 라우터) 통제:**
   * 위젯 내부에서 CreateWidget이나 AddToViewport 직접 호출을 절대 금지합니다.
   * 화면 전환은 전역 서브시스템인 UMockUIController의 ReplaceState(), PushOverlay(), PopCurrentOverlay()를 통해서만 수행합니다.
4. **이벤트 주도 데이터 바인딩 (Delegate Mapping):**
   * UI 위젯은 외부 액터나 데이터를 매 프레임(Tick) 직접 참조하지 않습니다.
   * S_InGame 등은 UMockUIController의 델리게이트를 Bind하여 화면을 갱신합니다.
5. **BindWidget 및 매크로 규약:**
   * 모든 UI 구성요소(버튼, 텍스트 등)는 헤더 파일에서 `UPROPERTY(meta = (BindWidget))`로 선언합니다.
   * 버튼 클래스는 `UCommonButtonBase`로 통일하여 선언하십시오.

---

## 6. 핵심 데이터 구조체 (Struct) 및 시스템 매크로 명세

UMockUIController 및 이벤트 델리게이트에서 사용할 필수 데이터입니다. AI는 코드 생성 시 헤더 파일에 아래 요소들을 블루프린트 에디터에서 완벽히 접근 가능하도록 리플렉션 매크로를 포함하여 정의해야 합니다.

### 1) EE_UIState (화면 상태 열거형)
UMockUIController의 ReplaceState()에서 사용할 화면 식별자입니다. (enum class로 선언, UENUM(BlueprintType) 적용)
* None
* Boot
* MainMenu
* SlotSelect
* CharacterSelect
* Tutorial
* StageSelect
* InGame
* Result

### 2) FPlayerInfo (로비 및 플레이어 상태 구조체)
USTRUCT(BlueprintType)으로 선언하며, 내부의 모든 멤버 변수는 반드시 `UPROPERTY(EditAnywhere, BlueprintReadWrite)` 매크로를 포함해야 UI 바인딩이 가능합니다.
* int32 CharacterIndex;
* FString PlayerName;
* bool bIsReady;
* bool bIsHost;

### 3) 다이내믹 멀티캐스트 델리게이트 (Dynamic Multicast Delegate) 규칙
UI 위젯의 NativeConstruct에서 `AddDynamic`을 통해 이벤트를 수신할 수 있도록, UMockUIController에 선언되는 모든 델리게이트(OnTeamMoneyUpdated, OnDurabilityChanged 등)는 반드시 `DECLARE_DYNAMIC_MULTICAST_DELEGATE` 계열의 매크로를 사용하여 선언해야 합니다.

* (신규) `OnRemainingFurnitureUpdated(int32 NewCount)`: GameState의 RemainingFurniture가 갱신될 때 Broadcast. S_InGame의 `Txt_RemainingFurniture` 갱신에 사용.
* (신규) `OnGameResultReady(int32 FinalScore, int32 StarCount)`: `TriggerGameResult()` 호출 시 Broadcast. S_Result는 위젯 생성 시점에 이미 캐시된 값을 직접 읽어가므로(`GetLastFinalScore`/`GetLastStarCount`), 이 델리게이트는 추후 다른 상시 존재 위젯(예: 로비 통계 패널)이 결과를 참조해야 할 경우를 위한 확장 포인트다.

### 4) FFurnitureStatusData (가구 상태 정보 구조체)
인게임에서 가구를 바라볼 때(Hover) W_FurnitureStatus UI에 전달할 데이터 묶음입니다. USTRUCT(BlueprintType)으로 선언하며, 멤버 변수에 `UPROPERTY(EditAnywhere, BlueprintReadWrite)`를 적용합니다.
* FString FurnitureName;
* float CurrentDurability;
* float MaxDurability;