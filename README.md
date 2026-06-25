# TeamCarry (Project A)

## 문서

- **[기여 가이드 (브랜치·커밋·PR·Revision Control 규칙)](./CONTRIBUTING.md)** — 팀원 필독
- [팀 규칙 (에셋 접두사 등)](Docs/team-rules.md)
- [역할 분담](Docs/roles.md)

---

> **Moving Out × Overcooked — 오버쿡드 풍 카툰 이사 협동 게임**

친구들과 함께 집 안의 가구를 **최대한 빠르게 옮겨 이사를 완수**하는 캐주얼 협동 게임. 시간이 적게 걸릴수록 높은 점수를 받고, 가구를 부수면 점수가 깎인다. 한 끗 어긋났을 때의 폭소(우정 파괴)가 재미의 핵심.

---

## 게임 컨셉

| 항목 | 내용 |
|---|---|
| **게임명** | Project A / TeamCarry (2~4인 협동) |
| **장르** | 캐주얼 협동 이사/정리 (Moving Out 계열) |
| **엔진** | Unreal Engine (버전 핀 미정) |
| **언어** | C++ / Blueprint |
| **플랫폼** | PC (확인 필요) |
| **레퍼런스** | Moving Out, Overcooked |

### 코어 판타지
- 친구들과 손발을 맞춰 어지러운 집을 순식간에 비워내는 통쾌함
- 한 끗 어긋났을 때의 폭소(우정 파괴) — 밝고 귀여운 톤이 실수마저 웃음으로

### 핵심 메커니즘
- **가구 운반** — 들기/나르기/놓기. 가구별 최소 인원·내구도·충돌 파손이 핵심
- **대상 인지** — 옮길 가구는 **반짝임(하이라이트)** 으로 표시, 남은 개수 UI 표기
- **StopWatch 점수** — 카운트다운이 아니라 **경과 시간 측정**. 빨리 옮길수록 점수↑, 파손 시 차감
- **스테이지(집) 클리어** — 대상 가구를 모두 옮기면 클리어 (가정집·가게 등 다양한 집)

---

## 코어 루프

```
Moment : 반짝이는 대상 가구 확인 → 들기 → 운반(동선·협력) → 지정 위치/트럭에 놓기 → 남은 개수 갱신
Stage  : 집 앞 스폰 → 집 안 가구 전부 운반 → StopWatch 정지 → 소요 시간 → 점수 환산 → 결과
```

---

## 가구 옮기기 시스템 (게임 정체성)

| 변수 | 타입 | 의미 |
|---|---|---|
| `Furniture_MaxHealth` / `CurrentHealth` | Float | 내구도 (충돌 시 차감) |
| `Furniture_RequiredPlayer` | Int | 안정적으로 들 최소 인원 |
| `Furniture_CurrentGrabbedPlayer` | Int | 현재 잡은 인원 |
| `Furniture_CombinedVelocity` | Vector | 플레이어 입력 합산 이동 벡터 |
| `Furniture_bIsGrabbed` | Bool | 잡힘 여부 |

> 멀티플레이 게임이므로 위 상태는 **서버 권위로 판정 후 복제**한다 (`Source/TeamCarry/Network/`).

---

## 소스 구조 (도메인 모듈)

```
Source/TeamCarry/
├── Core/        게임모드·게임스테이트, 공용 인터페이스/데이터, 결과·점수 판정
├── Player/      캐릭터, 이동, 상호작용(잡기) 입력
├── Furniture/   가구 본체, 다인 잡기, 내구도·파손  ← 게임 정체성
├── Network/     2~4인 복제·RPC, 서버 권위 동기화
├── Level/       스테이지(여러 집), 스테이지 선택, 트럭 도착 판정
└── UI/          메인메뉴·캐릭터선택·HUD·결과 화면
```

각 폴더의 `OWNER.txt`에 담당·용도·의존 방향 명시.

---

## 브랜치 / 협업

전략: **Git Flow (develop 완충)** — `feat/*` → `develop`(통합 완충) → `main`(발표/제출 안정판). `develop`이 main 깨짐 위험을 흡수한다. 상세 규칙(커밋·PR·머지)은 [CONTRIBUTING.md](./CONTRIBUTING.md).

| 브랜치 | 담당 | 도메인 |
|---|---|---|
| `main` | — | 발표/제출용 안정판 (release 시점에만 develop→main) |
| `develop` | — | **기본 브랜치 · 통합 완충** (feat가 모이는 곳) |
| `feat/core-gamemode-setup` | 윤준학 | Core / GameMode |
| `feat/player-character-setup` | 김민성 | Player |
| `feat/furniture-datatable-setup` | 홍민기 | Furniture |
| `feat/level-stage-greybox` | 이경환 | Level |
| `feat/ui-hud-setup` | 조민기 | UI |
| `feat/net-session-skeleton` | 신장식 | Network 세션 골격 |

> 각자 자기 `feat/*`에서 작업 → `develop`으로 PR(리뷰 1명↑) → Squash Merge. 발표·제출 시점에 `develop` → `main` 머지.
> 담당자 전체 표는 [Docs/roles.md](Docs/roles.md), 폴더별 상세는 각 `OWNER.txt`.

---

## 기술 스택

| 항목 | 내용 |
|---|---|
| Engine | Unreal Engine 5.8 |
| Language | C++ / Blueprint |
| Networking | Replication + RPC (서버 권위) |
| UI | UMG (Unreal Motion Graphics) |
| VCS | Git (GitHub) + 언리얼 에디터 Revision Control |
