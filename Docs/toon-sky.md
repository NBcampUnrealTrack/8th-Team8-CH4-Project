# 툰 스카이 (M_ToonSky) — 적용 방식과 교체 가이드

레벨 전체가 툰 룩인데 하늘만 사실적 볼류메트릭 구름이라 이질감이 있어, 스카이돔 머티리얼을 절차식 툰 스카이로 교체했다. 텍스처 0장, 신규 에셋 3개, 공용 레벨 무변경.

| Before | After |
|---|---|
| ![before](images/toon-sky/before.jpg) | ![after](images/toon-sky/after.jpg) |

![sky detail](images/toon-sky/sky-detail.jpg)

## 에셋

| 에셋 | 종류 | 역할 |
|---|---|---|
| `/Game/Developers/goldb/Shading/M_ToonSky` | Material | 툰 스카이 마스터 — Unlit 이미시브, 그라데이션 + 절차식 구름 |
| `/Game/Developers/goldb/Shading/MI_ToonSky` | Material Instance | 파라미터 조절용. 스카이돔에 실제로 꽂혀 있는 것 |
| `/Game/Developers/goldb/Maps/L_LevelProto_ToonSky` | Level | `L_LevelProto`의 개인 작업 사본 (원본 무변경) |

## 적용 방식

하늘은 라이팅을 받는 표면이 아니라 배경 그림이므로, 포스트 프로세스 셀 셰이딩이 아니라 스카이돔(`SM_SkySphere`) 머티리얼을 **Unlit 이미시브**로 교체하는 것이 정석.

- 돔 머티리얼 슬롯 0에 `MI_ToonSky` 할당 — 다른 시스템 수정 없음
- `VolumetricCloud`는 삭제 대신 컴포넌트 가시성만 꺼서 복구 가능하게 처리
- `SkyAtmosphere`·`ExponentialHeightFog`는 유지 (원거리 공기원근 + SkyLight 소스). 돔 머티리얼에 **Is Sky** 플래그를 켜서 돔은 안개 영향 제외
- 교체 직후 `SkyLight` 재캡처 — 앰비언트가 새 하늘 색을 반영
- 화면 공간이 아닌 표면 머티리얼이라 이후 PP 셀 셰이딩/아웃라인과 충돌하지 않음

## 머티리얼 그래프 (평가 순서)

1. **시선 방향**: `normalize(AbsoluteWorldPosition − CameraPosition)`
2. **고도 h**: `saturate(dir.z)` — 수평선 0, 천정 1
3. **하늘 3색 밴딩**: `saturate((h − BandPos) / BandFeather)` ×2 → Horizon → Mid → Zenith Lerp 체인
4. **구름 UV**: `dir.xy / (h + 0.3) × CloudScale` — 상공 평면 투영. `Time × CloudSpeed`로 흐름
5. **Noise**: 터뷸런스 3옥타브, 0~1 출력 (유일한 비용 지점, 하늘 픽셀만)
6. **구름 마스크 + 투톤**: `saturate((noise − CloudCover) / CloudSoftness)` — 딱딱한 에지. `CloudCoreThreshold`로 밝은 중심/그늘진 가장자리
7. **수평선 페이드**: h 0.03~0.13 구간에서 구름 감쇠
8. **합성**: `lerp(하늘, 구름, 마스크) × SkyBrightness` → Emissive

## 파라미터 (MI_ToonSky에서 실시간 조절)

| 파라미터 | 기본값 | 설명 |
|---|---|---|
| `HorizonColor / MidColor / ZenithColor` | 밝은 낮 3색 | 수평선 → 중간 → 천정 |
| `Band1Pos / Band2Pos` | 0.04 / 0.32 | 색 경계 높이 |
| `Band1Feather / Band2Feather` | 0.06 / 0.18 | 경계 폭. 작을수록 계단식 |
| `CloudCover` | 0.56 | 높일수록 구름 감소 |
| `CloudSoftness` | 0.05 | 구름 에지 경도 |
| `CloudCoreThreshold` | 0.68 | 투톤 중심부 임계값 |
| `CloudColor / CloudShadowColor` | 흰색 / 연파랑 | 구름 중심 / 밑면 그늘 |
| `CloudScale` | 2.0 | 구름 패턴 크기 |
| `CloudSpeed` | 0.005 | 흐르는 속도 |
| `SkyBrightness` | 1.0 | 전체 밝기 |

## 다른 하늘로 바꾸려면

1. **색만 변경 (1분)** — `MI_ToonSky` 파라미터 오버라이드. 노을 예시: Horizon `(1.0, 0.62, 0.35)`, Mid `(0.85, 0.45, 0.55)`, Zenith `(0.25, 0.20, 0.50)`, 구름 `(1.0, 0.9, 0.8)` / `(0.75, 0.5, 0.55)`. DirectionalLight·안개 색도 같이 보정할 것
2. **프리셋 운용** — MI 복제(`MI_ToonSky_Sunset` 등) 후 돔 슬롯만 교체. 부모 그래프 수정은 전 프리셋 반영
3. **그림(텍스처) 하늘** — Unlit + Is Sky 새 머티리얼에 `TextureSample → Emissive`. 돔 구면 UV 그대로 사용 가능
4. **원복** — 돔 슬롯 0 = `/Engine/EngineSky/M_SimpleSkyDome`, VolumetricCloud Visible 켜기, SkyLight 재캡처

### 다른 레벨에 적용 (에디터 Python)

```python
import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = eas.get_all_level_actors()

# 1) 스카이돔에 툰 스카이 할당
dome = next(a for a in actors if a.get_actor_label() == "SM_SkySphere")
mi = unreal.EditorAssetLibrary.load_asset("/Game/Developers/goldb/Shading/MI_ToonSky")
dome.static_mesh_component.set_material(0, mi)

# 2) 볼류메트릭 구름 숨김 (있다면)
cloud = next((a for a in actors if a.get_actor_label() == "VolumetricCloud"), None)
if cloud:
    cloud.root_component.set_visibility(False, True)
    cloud.set_actor_hidden_in_game(True)

# 3) 스카이라이트 재캡처
sl = next(a for a in actors if a.get_actor_label() == "SkyLight")
sl.light_component.recapture_sky()
```
