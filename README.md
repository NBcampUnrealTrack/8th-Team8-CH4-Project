![TeamCarry 키 아트](Docs/images/keyart.png)

# 8th-Team8-CH4-Project TeamCarry (Project C)

> **Moving Out × Overcooked — 오버쿡드 풍 카툰 이사 협동 게임**

친구들과 함께 집 안의 가구를 **제한 시간 안에 옮겨 이사를 완수**하는 캐주얼 협동 게임. 빨리 옮길수록 높은 점수를 받고, 가구를 부수면 점수가 깎인다. 한 끗 어긋났을 때의 폭소(우정 파괴)가 재미의 핵심.

## 시연 영상

[![TeamCarry 전체 플레이 영상 (13분)](https://drive.google.com/thumbnail?id=1kKGwWxuzBhrDyDwL5yJSPS9Cav2z7lMR&sz=w1280)](https://drive.google.com/file/d/1kKGwWxuzBhrDyDwL5yJSPS9Cav2z7lMR/view)

> 이미지를 클릭하면 전체 플레이 영상(13분)이 Google Drive 플레이어에서 재생됩니다.

[![TeamCarry 시연 영상](https://img.youtube.com/vi/zGokNi4wr2c/maxresdefault.jpg)](https://youtu.be/zGokNi4wr2c)

> 이미지를 클릭하면 YouTube에서 재생됩니다.

---

## 문서

- **[기여 가이드 (브랜치·커밋·PR·Revision Control 규칙)](./CONTRIBUTING.md)** — 팀원 필독
- [팀 규칙 (에셋 접두사 등)](Docs/team-rules.md)
- [역할 분담](Docs/roles.md)

---

## 게임 컨셉

| 항목 | 내용 |
|---|---|
| **게임명** | Project C / TeamCarry (2~4인 협동) |
| **장르** | 캐주얼 협동 이사/정리 (Moving Out 계열) |
| **엔진** | Unreal Engine 5.8 |
| **언어** | C++ / Blueprint |
| **플랫폼** | PC (Windows) — Steam 세션 기반 멀티플레이 |
| **레퍼런스** | Moving Out, Overcooked |

### 코어 판타지
- 친구들과 손발을 맞춰 어지러운 집을 순식간에 비워내는 통쾌함
- 한 끗 어긋났을 때의 폭소(우정 파괴) — 밝고 귀여운 톤이 실수마저 웃음으로

### 핵심 메커니즘
- **가구 운반** — 들기/나르기/놓기. 가구별 최소 인원·내구도·충돌 파손이 핵심
- **대상 인지** — 옮길 가구는 **반짝임(하이라이트)** 으로 표시, 남은 개수 UI 표기
- **제한 시간 카운트다운** — 스테이지별 제한 시간(기본 300초) 안에 완수. 남은 2분·1분 시점에 경고 문구와 화면 띠가 뜨고, 마지막 2분은 핫타임 연출로 재촉
- **스테이지(집) 클리어** — 대상 가구를 트럭에 모두 실으면 클리어. 남은 시간이 많을수록 점수↑, 파손 시 차감

---

## 코어 루프

```
Moment : 반짝이는 대상 가구 확인 → 들기 → 운반(동선·협력) → 트럭에 싣기 → 남은 개수 갱신
Stage  : 로비(세션) → 스테이지 선택 → 집 안 가구 전부 트럭에 적재 → 제한 시간 내 완수 → 점수 환산 → 결과
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
├── Network/     세션(Steam)·2~4인 복제·RPC, 서버 권위 동기화
├── Level/       스테이지(여러 집), 스테이지 선택, 트럭 적재 판정, 하늘·시간대
├── UI/          메인메뉴·캐릭터선택·HUD·결과 화면 (CommonUI)
└── Feedback/    사운드·이펙트·발소리 — 기존 클래스를 수정하지 않고 얹는 구조

Plugins/CatchCharacter/   가구 잡기·운반 코어 (다인 잡기·복제·내구도·충돌 데미지)
```

각 폴더의 `OWNER.txt`에 담당·용도·의존 방향 명시. 도메인 간 결합은 `Core/`의 인터페이스로 낮춘다.

### 레벨

| 맵 | 용도 |
|---|---|
| `L_Title` | 타이틀 |
| `L_Lobby` | 세션 로비 (인원 대기·스테이지 선택) |
| `L_Level1` | 스테이지 1 |
| `L_Level2` | 스테이지 2 |

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
| Networking | Replication + RPC (서버 권위 리슨서버) · Steam OSS + SteamSockets |
| UI | UMG + CommonUI (`S_` 화면 / `O_` 오버레이 / `W_` 조각) |
| Rendering | Lumen + 포스트 프로세스 툰 셀 셰이딩 · 아웃라인 |
| VCS | Git (GitHub) + Git LFS (`.uasset`/`.umap`) + 언리얼 에디터 Revision Control |
