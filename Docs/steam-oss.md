# Steam OnlineSubsystem (OSS) 적용 가이드 — TeamCarry

> 2~4인 협동 멀티플레이를 **Steam 매치메이킹**으로 붙이기 위한 구조·구현·운영 문서.
> LAN/직접 IP 세션 골격(`UTCGameInstance::HostListenServer` / `JoinByAddress`) 위에 Steam 세션 계층을 얹는다.

---

## 1. OnlineSubsystem(OSS) 구조 한눈에

```
                 ┌─────────────────────────────────────────────┐
   게임 코드      │  UTCGameInstance (TeamCarry|Session|Steam)  │
  (BP/C++)       │  Host / Find / Join / Destroy SteamSession  │
                 └───────────────────┬─────────────────────────┘
                                     │  IOnlineSession 인터페이스
                 ┌───────────────────▼─────────────────────────┐
   추상 계층      │  IOnlineSubsystem  (플랫폼 독립 API)         │
                 │  GetSessionInterface() / Identity / Friends  │
                 └───────────────────┬─────────────────────────┘
                                     │  DefaultPlatformService=Steam
                 ┌───────────────────▼─────────────────────────┐
   플랫폼 구현     │  OnlineSubsystemSteam                        │
                 │  SteamNetDriver / SteamSockets / Lobby API   │
                 └───────────────────┬─────────────────────────┘
                                     │
                 ┌───────────────────▼─────────────────────────┐
   네이티브       │  Steamworks SDK (steam_api64.dll)            │
                 │  AppID 인증 · P2P 릴레이 · 친구/초대          │
                 └─────────────────────────────────────────────┘
```

핵심 개념:

| 용어 | 의미 |
|---|---|
| **IOnlineSubsystem** | 플랫폼 독립 진입점. `Online::GetSubsystem(World)`로 획득 |
| **IOnlineSession** | 세션 생성/검색/접속/파기 + presence·초대 |
| **FOnlineSessionSettings** | 호스트가 만드는 세션의 속성(인원/공개여부/로비/광고 키) |
| **FOnlineSessionSearch** | 클라이언트 검색 쿼리 + 결과(`SearchResults`) |
| **NAME_GameSession** | 표준 세션 식별자(엔진 상수). 한 클라이언트당 1개 활성 |
| **SteamNetDriver** | UE 리플리케이션을 Steam 소켓 위로 전송(직접 IP 대신) |
| **AppID 480** | Spacewar — Steam 공개 테스트 AppID. 실제 출시 전까지 사용 |

OSS 세션 호출은 **전부 비동기**다. `CreateSession()` 호출은 "요청"이고, 실제 결과는 `OnCreateSessionComplete` 델리게이트로 돌아온다. 그래서 본 구현은 콜백에서 BP 이벤트를 broadcast하고 ServerTravel/ClientTravel을 그 안에서 수행한다.

---

## 2. TeamCarry에 적용한 변경 (이 커밋)

### 2.1 플러그인 (`TeamCarry.uproject`)
```jsonc
"Plugins": [
  { "Name": "OnlineSubsystem",      "Enabled": true },
  { "Name": "OnlineSubsystemSteam", "Enabled": true },
  { "Name": "OnlineSubsystemUtils", "Enabled": true }
]
```

### 2.2 모듈 의존성 (`Source/TeamCarry/TeamCarry.Build.cs`)
```csharp
PublicDependencyModuleNames  += "OnlineSubsystem";        // 헤더에서 IOnlineSessionPtr 사용
PrivateDependencyModuleNames += "OnlineSubsystemUtils";   // Online::GetSubsystem (.cpp 전용)
```

### 2.3 엔진 설정 (`Config/DefaultEngine.ini`)
```ini
[/Script/Engine.GameEngine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="OnlineSubsystemSteam.SteamNetDriver",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")

[OnlineSubsystem]
DefaultPlatformService=Steam

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480
bInitServerOnClient=true

[/Script/OnlineSubsystemSteam.SteamNetDriver]
NetConnectionClassName="OnlineSubsystemSteam.SteamNetConnection"
```

### 2.4 `steam_appid.txt` (프로젝트 루트)
- 내용: `480`
- 에디터/Standalone 실행 시 Steamworks가 이 파일로 AppID를 인식한다. 패키징 빌드에선 불필요(런처가 주입).

### 2.5 세션 코드 (`Source/TeamCarry/Network/Session/TCGameInstance`)
`UTCGameInstance`에 Steam 세션 API를 확장.

| 함수 | 역할 |
|---|---|
| `HostSteamSession(Map, MaxPlayers=4, bLAN=false)` | 세션 생성 → 성공 시 `?listen`으로 ServerTravel. OSS 없으면 LAN 폴백 |
| `FindSteamSessions(bLAN=false, Max=20)` | TeamCarry 세션 검색 → `OnFindSessionsComplete(bSuccess, Num)` |
| `JoinFoundSession(Index)` | 검색결과 접속 → ConnectString 해석 → ClientTravel |
| `DestroySteamSession()` | 현재 세션 파기 |
| `GetFoundSessionCount()` / `GetFoundSessionName(i)` | UI 목록 표기용 |

BP 바인딩 이벤트: `OnCreateSessionComplete(bool)`, `OnFindSessionsComplete(bool, int32)`, `OnJoinSessionComplete(bool)`.

---

## 3. 게임 흐름에 연결하는 법 (UI/BP)

`GameInstanceClass`는 `UTCGameInstance`여야 한다(Project Settings → Maps & Modes → Game Instance Class). 메인메뉴 위젯에서:

```
[방 만들기 버튼]
  → GameInstance->HostSteamSession("/Game/Network/Maps/NetTest", 4, false)
  → (OnCreateSessionComplete 구독해 로딩 표시) → 자동으로 맵 진입

[방 찾기 버튼]
  → GameInstance->FindSteamSessions(false)
  → OnFindSessionsComplete(bSuccess, Num) 수신
  → Num만큼 GetFoundSessionName(i)로 목록 위젯 생성

[목록 항목 클릭]
  → GameInstance->JoinFoundSession(i)
  → OnJoinSessionComplete(true) → 자동 ClientTravel로 호스트 합류

[나가기/타이틀]
  → GameInstance->DestroySteamSession()
```

> 기존 `HostListenServer` / `JoinByAddress`는 그대로 남겨둠 — Steam이 없는 환경(에디터 빠른 테스트, NULL 서브시스템)에서 폴백·디버그용으로 계속 사용 가능.

---

## 4. 테스트 방법

### 4.1 에디터 PIE (빠른 확인, Steam 없이)
- PIE는 기본적으로 NULL 서브시스템처럼 동작 → 세션 생성/검색은 LAN 경로로만 의미 있음.
- `Number of Players` 2+, `Net Mode = Play As Listen Server`로 리플리케이션 자체는 검증 가능.
- **주의:** 레벨에 캐릭터 직접 배치 금지(빙의 시 BeginPlay 크래시) — GameMode `DefaultPawnClass`로 스폰. (`net-standard` 참고)

### 4.2 Steam 실접속 (Standalone, AppID 480)
1. 두 PC 각각 Steam 클라이언트 **로그인** 상태.
2. 각 PC에서 프로젝트 루트에 `steam_appid.txt`(480) 존재 확인.
3. `Standalone Game`으로 실행(에디터 PIE 아님).
4. A: 방 만들기 → B: 방 찾기 → 목록에서 A 선택 → 접속.

---

## 5. 직접 적용해야 하는 문제점 (수동 확인 — 별도 피드백 예정)

> 코드/설정으로 자동화할 수 없고 **사람이 환경·계정·정책을 결정**해야 하는 항목.

1. **GameInstance 클래스 지정** — Project Settings에서 `UTCGameInstance`로 설정돼 있어야 세션 API가 동작. (BP GameInstance를 쓰면 그 부모를 TCGameInstance로)
2. **같은 PC 2-인스턴스 테스트 불가** — Steam은 PC당 1계정. 실접속 검증엔 **PC 2대(또는 2계정)** 필요. 한쪽을 `?bIsLanMatch`/NULL로 돌리는 우회는 별도 설정.
3. **AppID 480 공유성** — Spacewar는 전 세계 공용 테스트 AppID라, presence 검색 시 **무관한 세션이 섞일 수 있음**. `TCGameName` 키로 필터링하지만 완벽하지 않음. 실제 AppID 발급 시 해소.
4. **Steamworks SDK / steam_api64.dll** — UE 5.8 OnlineSubsystemSteam에 번들됨. 누락 시 OSS 초기화 실패 → 코드가 LAN 폴백하므로 "Steam인 줄 알았는데 LAN"인 상황 주의(로그 `SessionInterface 없음` 확인).
5. **방화벽 / NAT** — 직접 IP가 아닌 Steam P2P 릴레이를 쓰므로 대개 통과하지만, 사내망·학교망에서 Steam P2P가 막히면 접속 실패 가능.
6. **패키징 빌드 AppID 주입** — 배포 시 `steam_appid.txt` 대신 Steam 런처가 AppID를 주입. 테스트 빌드를 런처 밖에서 돌리면 `steam_appid.txt`가 여전히 필요.
7. **세션 정리** — 비정상 종료 시 세션이 Steam에 남을 수 있음. `DestroySteamSession` 호출 보장(앱 종료/타이틀 복귀 시) 필요.

---

## 6. 참고
- 기존 네트워크 표준: [`net-standard.md`](net-standard.md) (있는 경우)
- 역할/오너십: [`roles.md`](roles.md), `Source/TeamCarry/Network/OWNER.txt`
- 세션 코드: `Source/TeamCarry/Network/Session/TCGameInstance.{h,cpp}`
