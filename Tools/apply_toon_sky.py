# -*- coding: utf-8 -*-
"""
툰 스카이 원클릭 적용 스크립트 (TeamCarry)

현재 에디터에 열려 있는 레벨에 툰 스카이 시스템을 적용한다:
  1. SM_SkySphere 돔 머티리얼 → MI_ToonSky (절차식 툰 하늘)
  2. VolumetricCloud 가시성 off (실사 구름 제거 — 삭제 아님, 체크 한 번으로 복구 가능)
  3. SkyLight Real Time Capture 켜고 재캡처 (하늘 따라 앰비언트 자동)
  4. TC_ToonSkySync 액터 배치 — DirectionalLight를 돌리면 하늘이 실시간으로
     낮 → 황혼(남색+태양 글로우) → 밤(짙은 남색)으로 전환된다

사용법 (레벨을 연 상태에서):
  에디터 메뉴 Tools > Execute Python Script... 로 이 파일 선택
  또는 Output Log 의 Cmd를 Python으로 바꾸고:
      exec(open(r'D:/Unreal/8th-Team8-CH4-Project/Tools/apply_toon_sky.py', encoding='utf-8').read())

전제: 레벨에 SM_SkySphere(라벨), DirectionalLight, SkyLight가 있어야 한다.
저장은 하지 않는다 — 결과 확인 후 직접 저장(Ctrl+S)할 것.
"""
import unreal

MI_TOONSKY = '/Game/Developers/goldb/Shading/MI_ToonSky'
MPC_TOONSKY = '/Game/Developers/goldb/Shading/MPC_ToonSky'
SYNC_CLASS = '/Script/TeamCarry.TCToonSkySync'


def apply_toon_sky():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = ues.get_editor_world()
    if world is None:
        unreal.log_error('[ToonSky] 에디터 월드 없음 (PIE 중이면 종료 후 실행)')
        return

    mi = unreal.load_asset(MI_TOONSKY)
    mpc = unreal.load_asset(MPC_TOONSKY)
    if not mi or not mpc:
        unreal.log_error('[ToonSky] MI_ToonSky/MPC_ToonSky 에셋을 찾을 수 없음')
        return

    done = []
    sun = None
    dome = None
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == 'SM_SkySphere' and isinstance(a, unreal.StaticMeshActor):
            dome = a
        elif isinstance(a, unreal.VolumetricCloud):
            a.root_component.set_visibility(False, True)
            done.append('VolumetricCloud 숨김')
        elif isinstance(a, unreal.SkyLight):
            sc = a.get_components_by_class(unreal.SkyLightComponent)[0]
            sc.set_editor_property('real_time_capture', True)
            sc.recapture_sky()
            done.append('SkyLight RTC+재캡처')
        elif isinstance(a, unreal.DirectionalLight):
            sun = a

    if dome:
        dome.static_mesh_component.set_material(0, mi)
        done.append('돔 → MI_ToonSky')
    else:
        unreal.log_warning('[ToonSky] SM_SkySphere 라벨의 돔을 찾지 못함 — 돔 교체 생략')

    has_sync = any(a.get_class().get_name() == 'TCToonSkySync' for a in eas.get_all_level_actors())
    if not has_sync:
        cls = unreal.load_class(None, SYNC_CLASS)
        if cls:
            s = eas.spawn_actor_from_class(cls, unreal.Vector(0, 0, 500), unreal.Rotator(0, 0, 0))
            s.set_actor_label('TC_ToonSkySync')
            s.set_folder_path('Lighting')
            s.set_editor_property('SkyCollection', mpc)
            s.set_editor_property('Sun', sun)
            done.append('TC_ToonSkySync 배치')
        else:
            unreal.log_warning('[ToonSky] TCToonSkySync 클래스 없음 — C++ 빌드 필요')

    unreal.log('[ToonSky] 적용 완료: ' + ', '.join(done) if done else '[ToonSky] 적용할 항목 없음')


apply_toon_sky()
