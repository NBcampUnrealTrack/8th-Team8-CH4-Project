# Contributing Guide — TeamCarry (Project A)

2~4인 협동 "가구 옮기기" 게임. 팀원 전원이 동일한 규칙으로 일해서 머지 충돌·코드 스타일 충돌을 최소화하는 것이 목표.

---

## 1. 브랜치 전략 — Git Flow (develop 완충)

```
main    ──●───────────────────●──   (제출/발표용 안정판 — release 시점에만 머지)
           ↑ release            ↑
develop ──●──●──●──●──●──●──────●──   (기본 브랜치 · 통합 완충 — feat가 모이는 곳)
           │   │   │   │
           │ feat/* feat/* ...       (짧은 생명주기 · develop에서 분기, develop으로 PR)
```

`develop`이 feat와 main 사이의 **완충 지대**다. 매일의 통합·머지·QA는 develop에서 일어나고, main은 발표·제출처럼 안정 시점에만 develop을 받는다. → main이 깨질 위험을 develop이 흡수.

- **`main`** — 제출/발표용 안정판. **release 시점에만 develop → main 머지.** 직접 커밋·push 금지
- **`develop`** — **기본 브랜치.** 모든 feat가 모이는 통합 완충 지대. 여기서 멀티 QA 후 main으로 올림
- **`feat/*`** — 새 기능. **develop에서 분기, develop으로 PR.** 하루~3일 내 완료
- **`fix/*`** — 버그 수정 (develop 기준)
- **`refactor/*`** — 동작 변경 없는 구조 개선 (develop 기준)
- **`chore/*`** — 빌드·설정·의존성 (develop 기준)

### 브랜치 네이밍

```
<type>/<scope>-<short-description>
```

**예시**:
```
feat/furniture-grab-multiplayer
feat/player-interact-grab
feat/net-replicate-combined-velocity
fix/furniture-health-not-decreasing
refactor/core-datatypes-split
chore/gitignore-asset-packs
```

규칙:
- 영문 소문자 + 하이픈
- scope는 담당 시스템 (`core` / `player` / `furniture` / `net` / `level` / `ui`)
- 한글 금지 (터미널·CI 깨짐)

---

## 2. 커밋 메시지 — Conventional Commits

```
<type>(<scope>): <subject>

[optional body]
```

### type

| type | 의미 |
|---|---|
| `feat` | 새 기능 |
| `fix` | 버그 수정 |
| `refactor` | 구조만 변경 (동작 동일) |
| `chore` | 빌드·설정·의존성·에셋팩 |
| `docs` | 문서 |
| `style` | 포맷팅만 (로직 변경 없음) |
| `test` | 테스트 |

### scope

`core` / `player` / `furniture` / `net` / `level` / `ui`

### 예시

```
feat(furniture): 다인 잡기 시 CombinedVelocity 합산 로직 구현
feat(player): 가구 근처 상호작용으로 잡기/놓기 트리거
feat(net): 가구 bIsGrabbed·내구도 서버 권위 복제
fix(furniture): 충돌해도 내구도가 차감되지 않던 문제 수정
refactor(core): 공용 열거형을 TC_DataTypes로 분리
docs(readme): 코어 루프 흐름도 업데이트
```

### 규칙

- **subject는 한국어 OK** (내부 팀 프로젝트이므로)
- 명령형으로 작성 ("추가함" 아닌 "추가")
- 50자 이내 권장
- 상세 설명 필요 시 한 줄 띄우고 본문 작성

---

## 3. Pull Request 규칙

### PR 만들 때

1. **로컬에서 빌드 성공 확인** 후 push
2. GitHub에서 PR 생성 → **base 브랜치 = `develop`** 확인(기본값) → 템플릿 채우기 (자동)
3. **리뷰어 1명 이상 지정** (담당 시스템에 가까운 팀원)
4. 관련 Issue 있으면 `Closes #N` 본문에 명시

### PR 제목

커밋 메시지와 동일 형식:
```
feat(furniture): 가구 파손 판정 + 점수 차감
```

### 병합 전 체크

- [ ] 로컬 빌드 성공
- [ ] PIE 검증 완료 (멀티 관련은 2인 이상 Net PIE)
- [ ] 리뷰어 1명 이상 Approve
- [ ] 머지 충돌 해결
- [ ] 브랜치·커밋 컨벤션 준수

---

## 4. 병합 전략

| 상황 | 전략 |
|---|---|
| `feat/*` → `develop` | **Squash Merge** (여러 커밋을 1개로 압축, develop 히스토리 깔끔) |
| `fix/*` → `develop` | Squash Merge |
| `develop` → `main` | **Merge Commit** (릴리스 — 발표·제출 등 안정 시점) |
| 큰 기능 (3일 이상 작업) | Merge Commit 가능 (병합 시점 명시) |

**금지**:
- `main` · `develop` 직접 커밋·push (둘 다 PR로만)
- Force push (특히 main·develop)
- 자기 PR 리뷰 없이 merge

---

## 5. 담당 영역 (파일/폴더 소유)

충돌 회피를 위해 **각자 주로 작업하는 영역**을 분명히 정합니다. 각 폴더의 `OWNER.txt` 참고.

| 역할 | 담당 | 주 작업 영역 |
|---|---|---|
| GameMode / Core | 윤준학 | `Source/TeamCarry/Core/`, `Config/` |
| 캐릭터 (Player) | 김민성 | `Source/TeamCarry/Player/`, `Content/Player/` |
| 가구 Prop (Furniture) | 홍민기 | `Source/TeamCarry/Furniture/`, `Content/Furniture/` |
| 레벨 디자인 (Level) | 이경환 | `Source/TeamCarry/Level/`, `Content/Maps/` (강도 AI·방해물 포함) |
| UI | 조민기 | `Source/TeamCarry/UI/`, `Content/UI/` |
| 네트워크 + 물리 | 각자 (분산) | `Source/TeamCarry/Network/` — 각 도메인이 자기 복제 책임 |
| 통합·빌드 + 게임필 | 신장식 | 전역 — 세션 골격·표준·빌드 자동화·통합 QA·교차 검증 |

> 교차 작업이 필요하면 **사전 공유** (디스코드·카톡). 같은 파일에 2명 이상 동시 수정 금지.
> 멀티플레이 특성상 가구·플레이어 상태는 Net 담당과 반드시 협의 후 복제 변수 추가.

---

## 6. 코드 스타일

### C++ (UE5.8)

- 클래스 접두사: `A`(Actor), `U`(UObject/Component), `F`(Struct), `I`(Interface) — UE 표준
- 프로젝트 접두사: `TC` 또는 게임 클래스는 `TeamCarry...` 사용
- `TObjectPtr<T>` 우선 (UE 5.4+ 권장)
- `UPROPERTY()` / `UFUNCTION()` 리플렉션 매크로 정확히 사용 (복제 변수는 `Replicated` 지정 + `GetLifetimeReplicatedProps`)
- 헤더 전방 선언 선호, CPP에서 include

### Blueprint

- 파일명: `BP_<Class>`, `WBP_<Widget>`, `ABP_<AnimBP>`, `BS_<BlendSpace>`
- 노드 정리: 선 꼬임 금지, Reroute 적극 활용
- 100노드 이상이 되면 C++ 분리 고려

### 에셋 네이밍 (팀 표준 — 필수)

에셋 접두사(`SM_`/`SK_`/`T_`/`M_`/`MI_`/`BP_`/`WBP_`/`DA_`/`DT_`/`ABP_`/`AM_`)와 Content 폴더 규칙은
**[Docs/team-rules.md](Docs/team-rules.md)** 가 기준 문서다. (코드 클래스 접두사는 위 C++ 항목 참고)

---

## 7. Git / Revision Control 세부 규칙

### 언리얼 에디터 Revision Control (Git) 연동

이번 프로젝트는 **언리얼 에디터의 Revision Control 기능**을 사용한다.

1. 에디터 우하단 **Revision Control → Connect to Revision Control** → Provider: **Git**
2. Git path는 설치된 `git.exe` 자동 인식 (안 되면 직접 지정)
3. `.uasset`/`.umap`은 **바이너리** → 같은 에셋을 둘이 동시에 수정하면 머지 불가.
   작업 전 **디스코드로 "OO 맵/에셋 만진다" 공유**가 사실상의 잠금 역할.
4. 에셋 저장 후 에디터에서 직접 **Submit Content**(커밋) 하거나, 코드와 함께 외부 git으로 커밋.

### `.gitignore` 정책

- `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/` 금지
- IDE 찌꺼기 (`.vs/`, `.idea/`, `*.DotSettings.user`) 금지
- 외부 마켓팩(FAB) 처리·Content 네임스페이스 규칙은 [Docs/team-rules.md](Docs/team-rules.md) 참고

### LFS
- 현재 **미사용**. 단, 개별 `.uasset`이 100MB 초과하면 LFS 도입 검토 필요 (가구 메시·텍스처가 커질 수 있어 초기 모니터링)

### 금지 행동
- `main`에 직접 push
- Force push (특히 main, 리뷰 중인 feat 브랜치)
- `Binaries/` / `Intermediate/` / `DerivedDataCache/` / `Saved/` 커밋
- 라이선스 불명 에셋 커밋

---

## 8. 리뷰 문화

- **작게 자주** PR — 500줄 이상 PR은 지양
- 리뷰어는 **하루 내 응답** — 못 하면 미리 공유
- 의견 충돌 시 디스코드 채널에서 토론 후 결론

---

## 문의

규칙 변경 제안은 디스코드 채널에서 논의 후 PR로 반영.
