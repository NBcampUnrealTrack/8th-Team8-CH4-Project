# 상호작용 하이라이트 링 (PP 아웃라인 확장)

튜터 발표 피드백 반영: **상호작용 가능한 오브젝트에 포커스하면 노란 하이라이트 링**이 표시됩니다.

![하이라이트 링](images/interact-highlight/highlight-ring.jpg)

*포커스된 박스에만 노란 링 — 나머지는 기본 뎁스 아웃라인(세피아)만.*

## 동작 원리

```
GrabComponent::ScanBestTarget (박스 트레이스 + 스코어링)
  → ITCInteractable::Execute_OnFocus / OnUnfocus
    → SetCustomDepthStencilValue(1) + SetRenderCustomDepth(true/false)
      → M_PP_OutlineHLSL이 CustomStencil==1 실루엣 둘레에 링 렌더
```

- 셰이더는 기존 뎁스 아웃라인 PP(`M_PP_OutlineHLSL` v7)에 통합 — 추가 패스 없이 CustomStencil(SceneTexture id 25)을 샘플링해 실루엣 바깥 1링을 그림.
- 화면 경계에서 이웃 샘플 클램프로 생기던 점선 아티팩트는 `GetViewportUV` 기반 경계 가드로 차단.
- `r.CustomDepth=3` (Enabled with Stencil) 필요 — DefaultEngine.ini에 반영됨.

## 적용된 상호작용 액터

| 클래스 | 위치 | 비고 |
|---|---|---|
| `ATCCarriableFurniture` | Network/Carry | 로비 가구 전체 (빈 스텁 → 구현) |
| `AFurnitureActor` | CatchCharacter 플러그인 | `SetHighlight` 더미 → 구현 |
| `AInteractableDoor` | Level/Struct | 스텐실 값 1 지정 추가 |
| `ATCBatItem` | Level/Item | 스텐실 값 1 지정 추가 |
| `ATCMapInteractable` | Level/Struct | 스텐실 값 1 지정 추가 |

## MIC 파라미터 (MI_PP_OutlineHLSL)

| 파라미터 | 기본값 | 설명 |
|---|---|---|
| HighlightWidth | 3.0 | 링 두께(px) |
| HighlightColor | (1.0, 0.75, 0.1) | 링 색 (노랑) |
| HighlightOpacity | 1.0 | 링 불투명도 |
| HighlightFill | 0.15 | 실루엣 내부 은은한 밝기 보정 |

새 액터에 하이라이트를 붙이려면 `OnFocus`에서 스텐실 1 + 커스텀뎁스 ON, `OnUnfocus`에서 OFF만 해주면 됩니다.
