"""Explicit, guarded editor operations for the first production Landscape layout.

Importing this module never creates or saves content. Call one operation at a time.
"""
import json
from pathlib import Path
import unreal

ROOT = Path('D:/Project/Rogue10m')
MAP = '/Game/Rogue10m/Maps/Production/WorldLayout20261112'
EVIDENCE = ROOT / 'Feature/doc/evidence/landscape-world-blockout-20261112'
TAG = 'Rogue10m.WorldLayout20261112'


def _world():
    assert Path(unreal.Paths.project_dir()).resolve() == ROOT
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE first.'
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _write(name, result):
    (EVIDENCE / (name + '.json')).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))
    return result


def begin_candidate(allow_editor_mutation=False):
    """Create the blank WP world, then import once. Never overwrite/re-import."""
    _world()
    assert allow_editor_mutation is True
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    assert not unreal.EditorAssetLibrary.does_asset_exist(MAP), 'Candidate exists; inspect instead.'
    assert hasattr(unreal, 'Rogue10mWorldLayoutAuthoring'), 'Build the new native helper first.'
    report = json.loads((EVIDENCE / 'layout-report.json').read_text(encoding='utf-8'))
    assert report.get('passed') is True, 'Terrain validation did not pass.'
    material = unreal.load_asset('/Game/Stylized_Village/Assets/Terrain/Material_Landscape/MI_Terrain')
    layer = unreal.load_asset('/Game/Stylized_Village/Assets/Terrain/TerrainLayerWeights/AutoLayer_LayerInfo')
    assert material and layer
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).new_level(MAP, True)
    assert _world().get_path_name().split('.')[0] == MAP
    result = json.loads(unreal.Rogue10mWorldLayoutAuthoring.create_landscape_from_heightmap(
        str(EVIDENCE / 'terrain-height.r16'), material, layer))
    return _write('terrain-import', result)


def inspect_terrain(load_region=False):
    assert _world().get_path_name().split('.')[0] == MAP
    if load_region:
        result = json.loads(unreal.Rogue10mWorldLayoutAuthoring.load_editor_region())
    else:
        result = json.loads(unreal.Rogue10mWorldLayoutAuthoring.inspect_terrain())
    return _write('terrain-inspection', result)


def save_candidate():
    world = _world()
    assert world.get_path_name().split('.')[0] == MAP
    dirty = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
             list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
    allowed = (MAP, '/Game/__ExternalActors__/Rogue10m/Maps/Production/WorldLayout20261112/',
               '/Game/__ExternalObjects__/Rogue10m/Maps/Production/WorldLayout20261112/',
               '/Game/Rogue10m/World/Production/WorldLayout20261112/')
    names = [p.get_name() for p in dirty]
    assert all(n == MAP or any(n.startswith(prefix) for prefix in allowed[1:]) for n in names), names
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    remaining = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                 list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
    assert all(p.get_name() == MAP or any(p.get_name().startswith(prefix) for prefix in allowed[1:]) for p in remaining)
    if remaining:
        assert unreal.EditorLoadingAndSavingUtils.save_packages(remaining, True)
    return _write('save-candidate', dict(success=True, packages=names))


def add_daylight(allow_editor_mutation=False):
    """Candidate-only clear daylight; existing/vendor assets are never edited."""
    world = _world()
    assert allow_editor_mutation is True and world.get_path_name().split('.')[0] == MAP
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assert not any(isinstance(a, (unreal.DirectionalLight, unreal.SkyLight, unreal.SkyAtmosphere,
                   unreal.ExponentialHeightFog, unreal.PostProcessVolume)) for a in sub.get_all_level_actors())
    made = []

    def actor(cls, label):
        a = sub.spawn_actor_from_class(cls, unreal.Vector(15000, 45000, 15000))
        a.set_actor_label('WL20261112_' + label)
        a.tags = [unreal.Name(TAG)]
        a.set_editor_property('is_spatially_loaded', False)
        a.set_folder_path('WorldLayout20261112/Lighting')
        made.append(a)
        return a

    sun = actor(unreal.DirectionalLight, 'Sun')
    sun.set_actor_rotation(unreal.Rotator(pitch=-42, yaw=-35, roll=0), False)
    c = sun.get_component_by_class(unreal.DirectionalLightComponent)
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity', 16000.0)
    c.set_editor_property('atmosphere_sun_light', True)
    c.set_editor_property('light_source_angle', 1.0)
    actor(unreal.SkyAtmosphere, 'SkyAtmosphere')
    sky = actor(unreal.SkyLight, 'SkyLight')
    c = sky.get_component_by_class(unreal.SkyLightComponent)
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity', 1.1)
    c.set_editor_property('real_time_capture', True)
    fog = actor(unreal.ExponentialHeightFog, 'Fog')
    fog.set_actor_location(unreal.Vector(0, 0, 3500), False, False)
    c = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    c.set_editor_property('fog_density', 0.0003)
    c.set_editor_property('start_distance', 5000.0)
    pp = actor(unreal.PostProcessVolume, 'Exposure')
    pp.set_editor_property('unbound', True)
    settings = pp.get_editor_property('settings')
    for key, value in [('override_auto_exposure_min_brightness', True),
                       ('override_auto_exposure_max_brightness', True),
                       ('auto_exposure_min_brightness', 11.5), ('auto_exposure_max_brightness', 11.5),
                       ('override_auto_exposure_bias', True), ('auto_exposure_bias', 0.0)]:
        settings.set_editor_property(key, value)
    pp.set_editor_property('settings', settings)
    return _write('lighting', dict(success=True, sun_lux=16000, exposure_ev100=11.5,
                  sky_intensity=1.1, clouds='none', actors=[a.get_path_name() for a in made]))


def reserve_boss_sites(allow_editor_mutation=False):
    """Reserve 4 midboss sites and the final arena separately from 15 species."""
    world = _world()
    assert allow_editor_mutation is True and world.get_path_name().split('.')[0] == MAP
    import sys
    sys.path.insert(0, str(ROOT / 'Scripts'))
    import WorldLayoutTerrain20261112 as terrain
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assert not any(a.get_actor_label().startswith('WL26_PendingBoss_') for a in sub.get_all_level_actors())
    nodes = terrain.nodes_world()
    made = []
    for node_id, roster_id in [('BM','SporeMatriarch'), ('BW','ThornWarden'),
                               ('BE','AbyssEye'), ('BG','GoldenMimic'), ('B','AbyssOverlordAzathor')]:
        n = nodes[node_id]
        position = [n['x'], n['y'], n['z'] + 100]
        if node_id == 'B':
            position = [15000, 69800, 9780]
        a = sub.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(*position))
        a.set_actor_label('WL26_PendingBoss_' + node_id + '_' + roster_id)
        a.set_folder_path('WorldLayout20261112/Encounters/Reserved')
        rank = 'FinalBoss' if node_id == 'B' else 'MidBoss'
        a.tags = [unreal.Name(TAG), unreal.Name(TAG + '.Pending.' + rank)]
        a.set_editor_property('is_editor_only_actor', True)
        a.set_actor_hidden_in_game(True)
        made.append(dict(node=node_id, roster=roster_id, rank=rank, position=position, actor=a.get_path_name()))
    return _write('reserved-boss-sites', dict(success=True, midboss_count=4, finalboss_count=1,
                   gameplay='editor-only reservations, no boss actors or AI', sites=made))


def refine_boss_floor(allow_editor_mutation=False):
    """Match the circular terrain pad and separate coplanar visual surfaces."""
    world = _world()
    assert allow_editor_mutation is True and world.get_path_name().split('.')[0] == MAP
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    matches = [a for a in actors if a.get_actor_label() == 'WL26_PersistentBossGround']
    assert len(matches) == 1
    a = matches[0]
    assert TAG in [str(t) for t in a.tags]
    cylinder = unreal.load_asset('/Engine/BasicShapes/Cylinder')
    assert cylinder
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    assert c and str(c.get_collision_profile_name()) == 'BlockAll'
    before = dict(mesh=c.get_editor_property('static_mesh').get_path_name(),
                  location=[a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z])
    assert c.set_static_mesh(cylinder)
    a.set_actor_location(unreal.Vector(15000, 68000, 9552), False, False)
    a.set_actor_scale3d(unreal.Vector(110, 110, 1))
    return _write('boss-floor-refinement', dict(success=True, before=before,
                  shape='cylinder', diameter_cm=11000, top_cm=9602, landscape_pad_height_cm=9600,
                  visual_separation_cm=2, actor=a.get_path_name(), mesh=cylinder.get_path_name()))


def add_start_hub_entry(allow_editor_mutation=False):
    """Add separate OFPA actors. Existing preparation/portal actors stay untouched."""
    world = _world()
    assert world.get_path_name().split('.')[0] == '/Game/Rogue10m/Maps/StartHub'
    assert allow_editor_mutation is True
    assert unreal.EditorAssetLibrary.does_asset_exist(MAP)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    old = list(sub.get_all_level_actors())
    assert not any(str(t) == TAG for a in old for t in a.tags), 'Entry exists; inspect instead.'
    originals = {a.get_actor_label(): a.get_path_name() for a in old if
                 a.get_actor_label() in ('SV_RunPortal_ToOpenWorld', 'SV_FiveVillageRunPortal', 'SV_TutorialGuide_Arin')}
    assert len(originals) == 3, originals
    cube = unreal.load_asset('/Engine/BasicShapes/Cube')
    stone = unreal.load_asset('/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_WarmStone')
    glow = unreal.load_asset('/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_PortalGlow')
    assert cube and stone and glow
    made = []

    def owned(actor, label):
        actor.set_actor_label(label)
        actor.tags = [unreal.Name(TAG)]
        actor.set_editor_property('is_spatially_loaded', False)
        actor.set_folder_path('WorldLayout20261112/StartHubEntry')
        made.append(actor)
        return actor

    def mesh(label, position, size, material):
        a = owned(sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*position)), label)
        c = a.static_mesh_component
        c.set_static_mesh(cube)
        c.set_material(0, material)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        a.set_actor_scale3d(unreal.Vector(*(s / 100.0 for s in size)))
        return a

    x, y = -720.0, 4270.0
    portal = owned(sub.spawn_actor_from_class(unreal.Rogue10mRunPortal, unreal.Vector(x, y, 180)), 'WL20261112_RunPortal')
    portal.set_editor_property('target_level_name', unreal.Name(MAP))
    portal.set_editor_property('start_run_timer_on_travel', True)
    portal.set_editor_property('additional_travel_options', '')
    for dx in (-175, 175):
        mesh('WL20261112_EntryPillar_' + str(dx), (x + dx, y, 220), (65, 90, 440), stone)
    mesh('WL20261112_EntryLintel', (x, y, 465), (410, 100, 75), stone)
    mesh('WL20261112_EntryGlow', (x, y, 250), (250, 8, 390), glow)
    text = owned(sub.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(x, y - 90, 560),
                 unreal.Rotator(pitch=0, yaw=-90, roll=0)), 'WL20261112_EntrySign')
    component = text.get_component_by_class(unreal.TextRenderComponent)
    component.set_text('EXPEDITION ROAD')
    component.set_world_size(32)
    component.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    component.set_text_render_color(unreal.Color(120, 235, 245, 255))
    # Every new actor is external in this WP map. Save those packages only; do
    # not serialize a pre-existing dirty StartHub binary as part of this feature.
    dirty = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
             list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
    assert dirty and all(p.get_name().startswith((
        '/Game/__ExternalActors__/Rogue10m/Maps/StartHub/',
        '/Game/__ExternalObjects__/Rogue10m/Maps/StartHub/')) for p in dirty), [p.get_name() for p in dirty]
    assert unreal.EditorLoadingAndSavingUtils.save_packages(dirty, True)
    return _write('start-hub-entry', dict(success=True, originals=originals,
                  target=MAP, actor=portal.get_path_name(), location=[x,y,180],
                  new_actors=[a.get_path_name() for a in made], packages=[p.get_name() for p in dirty]))
