# 팀 규칙 — TeamCarry (Project A)

협업 충돌·탐색 비용을 줄이기 위한 공통 규칙. **에셋 접두사**는 강제(필수)이며,
신장식(통합·빌드)이 프로젝트 표준으로 관리한다.

---

## 언리얼 에셋 접두사 규칙 (필수)

| 접두사 | 에셋 종류 | 예시 |
|---|---|---|
| `SM_` | Static Mesh (정적 3D 오브젝트) | `SM_Tree_Oak_01` |
| `SK_` | Skeletal Mesh (움직이는 캐릭터 메시) | `SK_Enemy_Goblin` |
| `T_` | Texture (텍스처 이미지) | `T_Tree_Oak_D`  (D=Diffuse) |
| `M_` | Material (재질) | `M_Stone_Wet` |
| `MI_` | Material Instance | `MI_Stone_Wet_Dark` |
| `BP_` | Blueprint (블루프린트 클래스) | `BP_Enemy_Boss` |
| `WBP_` | Widget Blueprint (UI) | `WBP_HUD_Main` |
| `DA_` | Data Asset | `DA_WeaponConfig` |
| `DT_` | Data Table | `DT_EnemyStats` |
| `ABP_` | Animation Blueprint | `ABP_Player_Locomotion` |
| `AM_` | Animation Montage | `AM_Attack_Heavy` |

### 이 프로젝트(가구 옮기기) 적용 예시
- `SM_Sofa_01`, `SM_TV_CRT`, `SK_Mover_Default`
- `T_Sofa_D` / `T_Sofa_N`, `M_Cardboard`, `MI_Cardboard_Worn`
- `BP_Furniture_Sofa`, `WBP_InGameHUD`, `WBP_Result`
- `DA_FurnitureConfig`, `DT_FurnitureStats` (가구별 내구도·필요 인원)
- `ABP_Mover_Locomotion`, `AM_Grab`, `AM_Drop`

> 접두사 뒤는 `<대상>_<세부>` 순으로 의미가 큰 것부터. 텍스처 채널은 끝에
> `_D`(Diffuse) `_N`(Normal) `_ORM` 등으로 표기.

---

## 폴더 / 소유권

- 코드 도메인 폴더와 담당은 [`roles.md`](roles.md) 및 각 `Source/TeamCarry/<도메인>/OWNER.txt` 참고
- `Content/`는 도메인별 네임스페이스로 분리: `Content/Player/`, `Content/Furniture/`, `Content/Maps/`, `Content/UI/`
- 외부 마켓팩(FAB)은 git에 올리지 않고 각자 받아 `Content/TeamCarry/`로 필요한 것만 마이그레이트

---

## 협업 워크플로우

브랜치·커밋·PR·머지·Git/Revision Control 정책은 **[../CONTRIBUTING.md](../CONTRIBUTING.md)** 가 기준 문서.
이 문서(team-rules)는 **네이밍·에셋·콘텐츠 규칙**의 기준 문서다.
