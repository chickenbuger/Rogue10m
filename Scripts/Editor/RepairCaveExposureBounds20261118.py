"""Explicit bounded-volume physics repair and restoration from EV0 to EV3.

Import is inert. Only two existing Box collision modes and the two local
PostProcess min/max values change. All Box channels ignore, with no overlaps,
navigation, Pawn/Camera blocking, new objects, camera change, save or retry.
"""
from __future__ import annotations
import argparse
import copy
from datetime import datetime,timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import traceback

ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'Feature/doc/evidence/caves20261118'
SPEC=BASE/'exposure-bounds-spec.json'
SPEC_SHA256='fb2cb4bd5e9108507dee21d16af499c1ef19679ebbe029244705b2d55586fc70'
EXPOSURE_SHA256='89c6924c0f886c4d7b7367d37af1c9c67972973128030c9473b06bcbaf56667a'
VIS_SHA256='ad6cd1efe5e64a29ba3d311df890d861819238379748f4be3d31379c886183f9'
TRI_SHA256='cd3b1e386c2fba6f0d939b515be421bca5227ac0112cdf86c16fcd01f039d65e'
TARGET='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
COMPOSITE='67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc'
ORIGINAL='7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955'


class CaveExposureBoundsError(RuntimeError):
    def __init__(self,report):self.report=report;super().__init__(json.dumps(report,ensure_ascii=False))


def require(value,message):
    if not value:raise RuntimeError(message)


def module(name,relative):
    spec=importlib.util.spec_from_file_location(name,ROOT/relative)
    obj=importlib.util.module_from_spec(spec);spec.loader.exec_module(obj);return obj


def path(value):return value.get_path_name() if value else None


def serialized(value):
    return (json.dumps(value,sort_keys=True,ensure_ascii=False,separators=(',',':'),allow_nan=False)+'\n').encode('utf-8')


def read(name,sha=None):
    raw=(BASE/name).read_bytes()
    if sha:require(hashlib.sha256(raw).hexdigest()==sha,'Immutable input changed: '+name)
    return json.loads(raw)


def build_spec():
    exposure=read('author-live/exposure.json',EXPOSURE_SHA256);vis=read('author-live/visibility.json',VIS_SHA256)
    require(exposure.get('success') and len(exposure['sites'])==2 and len(exposure['dirty_after'])==2,'Exact unsaved EV0 predecessor required')
    sites=[]
    for previous in exposure['sites']:
        original=next(s for s in vis['postprocess_sites'] if s['site_id']==previous['site_id'])
        center=original['box_center_cm'];extent=original['box_extent_cm']
        sites.append({'site_id':previous['site_id'],'actor_path':previous['actor_path'],'package':previous['package'],
            'box_path':previous['box_path'],'post_process_path':previous['post_process_path'],
            'box_center_cm':center,'box_extent_cm':extent,'outside_query_cm':[center[0]+extent[0]+500.,center[1],center[2]],
            'expected_outside_distance_cm':500.,'before_settings_export_text':previous['after_settings_export_text'],
            'after_settings_export_text':original['settings_export_text'],'before_ev100':0.,'after_ev100':3.,
            'before_collision':'NO_COLLISION','after_collision':'QUERY_ONLY','all_collision_responses':'IGNORE'})
    return {'schema':'Rogue10m.CaveExposureBoundsSpec.20261118.v1','map':TARGET,
        'exposure_receipt_sha256':EXPOSURE_SHA256,'visibility_receipt_sha256':VIS_SHA256,'triplanar_receipt_sha256':TRI_SHA256,
        'native_composite_blake3':COMPOSITE,'original_layer_blake3':ORIGINAL,
        'reason':'UE5.8 GetAllPhysicsObjects returns nonempty{nullptr} without physics; PostProcess rejects before Shape fallback. QueryOnly creates a valid Box body.',
        'supersedes_no_collision_fallback_hypothesis':True,'ev0_candidate_visually_accepted':False,
        'expected_dirty_before':exposure['dirty_after'],'expected_dirty_after':exposure['dirty_after'],'sites':sites,
        'box_overlap_events':False,'box_navigation':False,'box_tick':False,'global_ev100_unchanged':11.5,
        'local_bounds_priority_blend_unchanged':True,'new_actors':0,'new_components':0,'new_assets':0,
        'runtime_visual_acceptance':'Pending actual fixed-pose game viewport and full route; source and collision-distance proof alone are not acceptance.'}


def closest(u,box,point,api):
    result=box.get_closest_point_on_collision(u.Vector(*point))
    require(isinstance(result,tuple) and len(result)==2,'Expected public distance/closest-point tuple')
    distance,location=result
    require(isinstance(distance,(float,int)) and isinstance(location,u.Vector),'Unexpected public closest-point types')
    return {'query_cm':list(point),'distance_cm':float(distance),'closest_point_cm':api._xyz(location)}


def channels(u):
    names=['ECC_WORLD_STATIC','ECC_WORLD_DYNAMIC','ECC_PAWN','ECC_VISIBILITY','ECC_CAMERA','ECC_PHYSICS_BODY','ECC_VEHICLE','ECC_DESTRUCTIBLE']
    names += ['ECC_ENGINE_TRACE_CHANNEL'+str(i) for i in range(1,7)]
    names += ['ECC_GAME_TRACE_CHANNEL'+str(i) for i in range(1,51)]
    return [getattr(u.CollisionChannel,n) for n in names if getattr(u.CollisionChannel,n,None) is not None]


def repair_cave_exposure_bounds(*,allow_editor_mutation=False,expected_spec_sha256,
                                expected_composite_blake3,expected_original_layer_blake3,expected_dirty_packages):
    report={'schema':'Rogue10m.CaveExposureBoundsRepair.20261118.v1','success':False,
        'started_utc':datetime.now(timezone.utc).isoformat(),'mutation_started':False,'assets_saved':False,'save_called':False,
        'created_actors':[],'created_components':[],'created_assets':[],'sites':[],'automatic_retry':False,
        'automatic_rollback':False,'partial_work_deleted':False}
    try:
        require(allow_editor_mutation is True and expected_spec_sha256==SPEC_SHA256,'Explicit invocation and reviewed bounds spec required')
        spec=read('exposure-bounds-spec.json',SPEC_SHA256);read('author-live/exposure.json',EXPOSURE_SHA256)
        tri=read('author-live/triplanar.json',TRI_SHA256)
        require(spec['schema']=='Rogue10m.CaveExposureBoundsSpec.20261118.v1' and spec['map']==TARGET and
                expected_composite_blake3==COMPOSITE and expected_original_layer_blake3==ORIGINAL and
                len(expected_dirty_packages)==len(set(expected_dirty_packages))==2 and
                set(expected_dirty_packages)==set(spec['expected_dirty_before']),'Exact unsaved two-package EV0 source required')
        import unreal as u
        api=module('bounds_component_api','Scripts/Editor/NaturalWorldDressing20261117.py')
        inspector=module('bounds_inspector','Scripts/Editor/InspectCaveSites20261118.py')
        reader=module('bounds_graph_reader','Scripts/Editor/NaturalRoadMaterial20261117.py')
        actor_reader=module('bounds_actor_reader','Scripts/Editor/AuthorNaturalBossThreshold20261118.py')
        visibility=module('bounds_visibility_state','Scripts/Editor/RefineCaveVisibility20261118.py')
        walls=module('bounds_effect_state','Scripts/Editor/RefineCaveRockWalls20261118.py')
        api._guard(u);require(set(api._dirty(u))==set(expected_dirty_packages),'Dirty set differs')
        require(callable(getattr(u.PrimitiveComponent,'get_closest_point_on_collision',None)),'Public closest-point query binding required')
        native=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(native.get('success') and native.get('landscape_reports_up_to_date') and native['composite_blake3']==COMPOSITE and
                native['original_layer_blake3']==ORIGINAL,'Settled collar terrain required')
        subsystem=u.get_editor_subsystem(u.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors())
        require(len(actors)==1408,'Exact1408 actors required')
        before=actor_reader._snapshot_actors(u,actors,api);before_components=inspector.persistent_components(u,actors)
        baseline=read('baseline-live/baseline-actors.json')
        require({p:before[p] for p in baseline['actors']}==baseline['actors'] and
                {p:before_components[p] for p in baseline['actors']}==baseline['persistent_components'],'Original1404 state changed')
        materials_before={p:visibility.material_state(u,u.load_asset(p),reader) for p in tri['expected_material_states_after']}
        require(materials_before==tri['expected_material_states_after'],'Current43-node seven-material state differs')
        sources=read('baseline-live/baseline-materials.json')['materials']
        source_before={p:inspector.material_state(u,u.load_asset(p),reader) for p in sources}
        require(source_before==sources and len(source_before)==40,'Source40 material states changed')
        effects_before=walls.effect_state(u,actors,visibility,inspector,api)
        components={path(c):c for a in actors for c in a.get_components_by_class(u.ActorComponent)}
        prepared=[]
        for site in spec['sites']:
            box=components.get(site['box_path']);pp=components.get(site['post_process_path'])
            require(isinstance(box,u.BoxComponent) and isinstance(pp,u.PostProcessComponent) and pp.get_attach_parent()==box and
                    path(pp.get_owner())==site['actor_path'] and box.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION and
                    not box.get_editor_property('generate_overlap_events') and not box.get_editor_property('can_ever_affect_navigation') and
                    not pp.get_editor_property('unbound') and pp.get_editor_property('enabled') and
                    pp.get_editor_property('priority')==100. and pp.get_editor_property('blend_radius')==200. and pp.get_editor_property('blend_weight')==1.,
                    'Exact existing NoCollision bounds/EV0 volume required')
            origin,extent,_=u.SystemLibrary.get_component_bounds(box)
            require(max(abs(a-b) for a,b in zip(api._xyz(origin),site['box_center_cm']))<.01 and
                    max(abs(a-b) for a,b in zip(api._xyz(extent),site['box_extent_cm']))<.01,'Existing bounds changed')
            settings=pp.get_editor_property('settings')
            require(settings.export_text()==site['before_settings_export_text'] and
                    settings.get_editor_property('auto_exposure_min_brightness')==settings.get_editor_property('auto_exposure_max_brightness')==0.,'Exact prior EV0 settings required')
            changed=settings.copy();changed.set_editor_property('auto_exposure_min_brightness',3.);changed.set_editor_property('auto_exposure_max_brightness',3.)
            require(changed.export_text()==site['after_settings_export_text'],'Restore must equal complete original EV3 settings')
            prepared.append((site,box,pp,changed,closest(u,box,site['box_center_cm'],api)))
        require(set(api._dirty(u))==set(expected_dirty_packages),'Read-only preflight dirtied packages')
        with u.ScopedEditorTransaction('Valid nonblocking cave exposure bounds and original localEV3 20261118'):
            report['mutation_started']=True
            for site,box,pp,changed,before_distance in prepared:
                pp.get_owner().modify();box.modify();pp.modify()
                box.set_collision_profile_name('Custom',False)
                box.set_collision_response_to_all_channels(u.CollisionResponseType.ECR_IGNORE)
                box.set_collision_enabled(u.CollisionEnabled.QUERY_ONLY)
                box.set_editor_property('generate_overlap_events',False);box.set_editor_property('can_ever_affect_navigation',False)
                box.set_component_tick_enabled(False)
                pp.set_editor_property('settings',changed)
                inside=closest(u,box,site['box_center_cm'],api);outside=closest(u,box,site['outside_query_cm'],api)
                require(abs(inside['distance_cm'])<=.01 and outside['distance_cm']>0 and
                        abs(outside['distance_cm']-site['expected_outside_distance_cm'])<=1.,'Box lacks the expected valid bounded query shape')
                require(box.get_collision_enabled()==u.CollisionEnabled.QUERY_ONLY and
                        all(box.get_collision_response_to_channel(c)==u.CollisionResponseType.ECR_IGNORE for c in channels(u)) and
                        not box.get_editor_property('generate_overlap_events') and not box.get_editor_property('can_ever_affect_navigation') and
                        not box.is_component_tick_enabled() and pp.get_editor_property('settings').export_text()==site['after_settings_export_text'],
                        'Nonblocking query-only bounds or exactEV3 readback differs')
                report['sites'].append({**site,'box_profile_after':str(box.get_collision_profile_name()),
                    'box_collision_after':str(box.get_collision_enabled()),'all_channels_ignore':True,
                    'before_center_query':before_distance,'after_center_query':inside,'after_outside_query':outside,
                    'actual_settings_export_text_after':pp.get_editor_property('settings').export_text(),
                    'new_overrides':0,'overlap_events':False,'navigation':False,'tick_enabled':False})
        after_actors=list(subsystem.get_all_level_actors());after=actor_reader._snapshot_actors(u,after_actors,api)
        normalized=copy.deepcopy(after);box_paths={s['box_path'] for s in spec['sites']}
        for actor_path,state in normalized.items():
            for primitive in state['primitives']:
                if primitive['path'] in box_paths:
                    old=next(p for p in before[actor_path]['primitives'] if p['path']==primitive['path'])
                    primitive['collision_profile']=old['collision_profile'];primitive['collision_enabled']=old['collision_enabled']
        require(normalized==before and inspector.persistent_components(u,after_actors)==before_components,'Geometry/other actor/component state changed')
        effects_after=walls.effect_state(u,after_actors,visibility,inspector,api);normalized_effects=copy.deepcopy(effects_after)
        for site in spec['sites']:normalized_effects['local_pp'][site['post_process_path']]['settings']=effects_before['local_pp'][site['post_process_path']]['settings']
        require(normalized_effects==effects_before,'Global/camera/lights/bounds/other local settings changed')
        require({p:visibility.material_state(u,u.load_asset(p),reader) for p in materials_before}==materials_before and
                {p:inspector.material_state(u,u.load_asset(p),reader) for p in source_before}==source_before,'Material states changed')
        final_native=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(final_native.get('success') and final_native['composite_blake3']==COMPOSITE and final_native['original_layer_blake3']==ORIGINAL,'Terrain changed')
        require(set(api._dirty(u))==set(expected_dirty_packages),'Only original two dressing packages may remain dirty')
        report.update(success=True,spec_sha256=SPEC_SHA256,exposure_receipt_sha256=EXPOSURE_SHA256,actor_count=1408,
            original1404_preserved=True,all_geometry_lights_camera_global_exposure_materials_preserved=True,
            only_two_box_query_modes_and_local_min_max_changed=True,public_inside_outside_distance_proof_passed=True,
            no_pawn_camera_visibility_blocking_added=True,material_states_unchanged=materials_before,
            effects_before=effects_before,effects_after=effects_after,dirty_before=list(expected_dirty_packages),dirty_after=api._dirty(u),
            finished_utc=datetime.now(timezone.utc).isoformat(),
            validation_pending=['actual fixed-pose game-viewport brightness and outside exposure','full camera routes','exact two-package save and independent cold QA'])
        return report
    except Exception:
        report['traceback']=traceback.format_exc();raise CaveExposureBoundsError(report) from None


def main():
    parser=argparse.ArgumentParser(description='Pure exposure-bounds repair spec generation only.');parser.add_argument('--write-spec',action='store_true')
    args=parser.parse_args();require(args.write_spec,'Explicit --write-spec required');value=build_spec()
    with SPEC.open('xb') as stream:stream.write(serialized(value))
    print(json.dumps({'spec_sha256':hashlib.sha256(serialized(value)).hexdigest(),'boxes':2,'boxCollision':'QueryOnly/IgnoreAll','localEV100':'0->3','newActorsComponentsAssets':0},indent=2))


if __name__=='__main__':main()
