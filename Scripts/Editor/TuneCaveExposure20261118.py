"""One explicit unsaved EV3->EV0 change on the two bounded cave components.

Import is inert. Original global exposure/cameras, bounds, material graphs,
lighting, actors, geometry and collision remain unchanged. No save or retry.
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
SPEC=BASE/'exposure-spec.json'
SPEC_SHA256='bc58e611db30ae46e604be7b3c72a3af02a94a57a6eef288faf23eb5feea8627'
VIS_SHA256='ad6cd1efe5e64a29ba3d311df890d861819238379748f4be3d31379c886183f9'
TRI_SHA256='cd3b1e386c2fba6f0d939b515be421bca5227ac0112cdf86c16fcd01f039d65e'
TARGET='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
COMPOSITE='67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc'
ORIGINAL='7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955'


class CaveExposureError(RuntimeError):
    def __init__(self,report):self.report=report;super().__init__(json.dumps(report,ensure_ascii=False))


def require(v,message):
    if not v:raise RuntimeError(message)


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
    vis=read('author-live/visibility.json',VIS_SHA256);tri=read('author-live/triplanar.json',TRI_SHA256)
    require(vis.get('success') and tri.get('success') and len(tri['surface_graph_after']['nodes'])==43,'Successful43-node visibility/material state required')
    return {'schema':'Rogue10m.CaveExposureSpec.20261118.v1','map':TARGET,
        'visibility_receipt_sha256':VIS_SHA256,'triplanar_receipt_sha256':TRI_SHA256,
        'native_composite_blake3':COMPOSITE,'original_layer_blake3':ORIGINAL,
        'reason':'First actual game-viewport entrance capture was visually rejected as too dark; numeric camera state is not visual acceptance.',
        'before_ev100':3.,'after_ev100':0.,'nominal_exposure_multiplier':8.,
        'sites':[{k:s[k] for k in ('site_id','actor_path','box_path','post_process_path','box_center_cm','box_extent_cm',
                                 'priority','blend_radius_cm','blend_weight','settings_export_text')}
                 for s in vis['postprocess_sites']],
        'global_ev100_unchanged':11.5,'new_overrides':0,'new_actors':0,'new_components':0,'new_assets':0,
        'require_clean_before':True,'dirty_after_exactly_two_dressing_packages':True,
        'runtime_visual_acceptance':'Pending actual fixed-pose game-viewport capture; EV adjustment alone is not acceptance.'}


def tune_cave_exposure(*,allow_editor_mutation=False,expected_spec_sha256,
                      expected_composite_blake3,expected_original_layer_blake3,expected_dirty_packages):
    report={'schema':'Rogue10m.CaveExposureTune.20261118.v1','success':False,
        'started_utc':datetime.now(timezone.utc).isoformat(),'mutation_started':False,'assets_saved':False,'save_called':False,
        'created_actors':[],'created_components':[],'created_assets':[],'automatic_retry':False,'automatic_rollback':False,
        'partial_work_deleted':False,'sites':[]}
    try:
        require(allow_editor_mutation is True and expected_spec_sha256==SPEC_SHA256,'Explicit invocation and reviewed exposure spec required')
        spec=read('exposure-spec.json',SPEC_SHA256);read('author-live/visibility.json',VIS_SHA256)
        tri=read('author-live/triplanar.json',TRI_SHA256)
        require(spec['schema']=='Rogue10m.CaveExposureSpec.20261118.v1' and spec['map']==TARGET and
                expected_composite_blake3==COMPOSITE and expected_original_layer_blake3==ORIGINAL and
                list(expected_dirty_packages)==[],'Exact collar hash and clean source required')
        import unreal as u
        api=module('exposure_component_api','Scripts/Editor/NaturalWorldDressing20261117.py')
        inspector=module('exposure_inspector','Scripts/Editor/InspectCaveSites20261118.py')
        reader=module('exposure_graph_reader','Scripts/Editor/NaturalRoadMaterial20261117.py')
        actor_reader=module('exposure_actor_reader','Scripts/Editor/AuthorNaturalBossThreshold20261118.py')
        visibility=module('exposure_visibility_state','Scripts/Editor/RefineCaveVisibility20261118.py')
        walls=module('exposure_effect_state','Scripts/Editor/RefineCaveRockWalls20261118.py')
        api._guard(u);require(not api._dirty(u),'No dirty packages allowed before exposure tuning')
        require(u.SystemLibrary.get_console_variable_int_value('r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange')==1,'EV100 range required')
        native=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(native.get('success') and native.get('landscape_reports_up_to_date') and native['composite_blake3']==COMPOSITE and
                native['original_layer_blake3']==ORIGINAL and native['component_count']==native['collision_count']==45,'Settled exact collar terrain required')
        subsystem=u.get_editor_subsystem(u.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors())
        require(len(actors)==1408,'Exact1408 editor actors required')
        before=actor_reader._snapshot_actors(u,actors,api);before_components=inspector.persistent_components(u,actors)
        baseline=read('baseline-live/baseline-actors.json')
        require({p:before[p] for p in baseline['actors']}==baseline['actors'] and
                {p:before_components[p] for p in baseline['actors']}==baseline['persistent_components'],'Original1404 state changed')
        material_before={p:visibility.material_state(u,u.load_asset(p),reader) for p in tri['expected_material_states_after']}
        require(material_before==tri['expected_material_states_after'],'Exact43-node/new-seven material states required')
        sources=read('baseline-live/baseline-materials.json')['materials']
        source_before={p:inspector.material_state(u,u.load_asset(p),reader) for p in sources}
        require(source_before==sources and len(source_before)==40,'Original40 source material states changed')
        effects_before=walls.effect_state(u,actors,visibility,inspector,api)
        exposure=[v for v in effects_before['global_pp'].values() if v['label']=='WL20261112_Exposure']
        require(len(exposure)==1 and exposure[0]['min']==exposure[0]['max']==11.5,'Original globalEV11.5 required')
        components={path(c):c for a in actors for c in a.get_components_by_class(u.ActorComponent)}
        targets=[];expected_packages=set()
        for site in spec['sites']:
            pp=components.get(site['post_process_path']);box=components.get(site['box_path'])
            require(isinstance(pp,u.PostProcessComponent) and isinstance(box,u.BoxComponent) and
                    path(pp.get_owner())==site['actor_path'] and pp.get_attach_parent()==box and
                    not pp.get_editor_property('unbound') and pp.get_editor_property('enabled') and
                    pp.get_editor_property('priority')==100. and pp.get_editor_property('blend_radius')==200. and pp.get_editor_property('blend_weight')==1.,
                    'Exact two existing bounded local volumes required')
            settings=pp.get_editor_property('settings')
            require(settings.export_text()==site['settings_export_text'] and
                    settings.get_editor_property('auto_exposure_min_brightness')==settings.get_editor_property('auto_exposure_max_brightness')==3.,
                    'Previous exactEV3 settings required; no retry or wider tuning')
            origin,extent,_=u.SystemLibrary.get_component_bounds(box)
            require(max(abs(a-b) for a,b in zip(api._xyz(origin),site['box_center_cm']))<.01 and
                    max(abs(a-b) for a,b in zip(api._xyz(extent),site['box_extent_cm']))<.01 and
                    box.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION,'Exposure bounds/collision changed')
            changed=settings.copy();changed.set_editor_property('auto_exposure_min_brightness',0.);changed.set_editor_property('auto_exposure_max_brightness',0.)
            restored=changed.copy();restored.set_editor_property('auto_exposure_min_brightness',3.);restored.set_editor_property('auto_exposure_max_brightness',3.)
            require(restored.export_text()==settings.export_text(),'Prepared settings differ beyond min/max')
            targets.append((site,pp,changed,settings.export_text()));expected_packages.add(pp.get_owner().get_package().get_name())
        require(len(targets)==len(expected_packages)==2 and not api._dirty(u),'Exact two clean dressing packages required')
        report['material_states_unchanged']=material_before;report['effects_before']=effects_before
        with u.ScopedEditorTransaction('Bounded caveEV0 after actual entrance visual rejection 20261118'):
            report['mutation_started']=True
            for site,pp,changed,before_text in targets:
                pp.get_owner().modify();pp.modify();pp.set_editor_property('settings',changed)
                actual=pp.get_editor_property('settings')
                require(actual.get_editor_property('auto_exposure_min_brightness')==actual.get_editor_property('auto_exposure_max_brightness')==0.,'LocalEV0 readback differs')
                reset=actual.copy();reset.set_editor_property('auto_exposure_min_brightness',3.);reset.set_editor_property('auto_exposure_max_brightness',3.)
                require(reset.export_text()==before_text,'Local settings changed beyond min/max')
                report['sites'].append({'site_id':site['site_id'],'actor_path':site['actor_path'],'post_process_path':path(pp),
                    'box_path':path(pp.get_attach_parent()),'before_settings_export_text':before_text,
                    'after_settings_export_text':actual.export_text(),'before_ev100':3.,'after_ev100':0.,
                    'only_min_max_changed':True,'package':pp.get_owner().get_package().get_name()})
        after_actors=list(subsystem.get_all_level_actors())
        require(actor_reader._snapshot_actors(u,after_actors,api)==before and inspector.persistent_components(u,after_actors)==before_components,
                'Actor/component/geometry/collision state changed')
        effects_after=walls.effect_state(u,after_actors,visibility,inspector,api);normalized=copy.deepcopy(effects_after)
        for site,pp,_,_ in targets:normalized['local_pp'][path(pp)]['settings']=effects_before['local_pp'][path(pp)]['settings']
        require(normalized==effects_before,'Global exposure/camera/lights/bounds/other local properties changed')
        require({p:visibility.material_state(u,u.load_asset(p),reader) for p in material_before}==material_before and
                {p:inspector.material_state(u,u.load_asset(p),reader) for p in source_before}==source_before,'A material state changed')
        native_after=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(native_after.get('success') and native_after['composite_blake3']==COMPOSITE and native_after['original_layer_blake3']==ORIGINAL,'Terrain changed')
        require(set(api._dirty(u))==expected_packages,'Only the two exact dressing packages may be dirty')
        report.update(success=True,spec_sha256=SPEC_SHA256,actor_count=1408,effects_after=effects_after,
            original1404_preserved=True,all_geometry_collision_lights_camera_global_exposure_preserved=True,
            source40_and_new7_material_states_preserved=True,no_overrides_added=True,only_two_local_min_max_values_changed=True,
            dirty_before=[],dirty_after=api._dirty(u),finished_utc=datetime.now(timezone.utc).isoformat(),
            validation_pending=['actual fixed-pose game-viewport entrance brightness','full camera traversal','two-package save and cold validation'])
        return report
    except Exception:
        report['traceback']=traceback.format_exc();raise CaveExposureError(report) from None


def main():
    parser=argparse.ArgumentParser(description='Pure bounded exposure spec generation only.');parser.add_argument('--write-spec',action='store_true')
    args=parser.parse_args();require(args.write_spec,'Explicit --write-spec required');value=build_spec()
    with SPEC.open('xb') as stream:stream.write(serialized(value))
    print(json.dumps({'spec_sha256':hashlib.sha256(serialized(value)).hexdigest(),'volumes':2,'beforeEV100':3,'afterEV100':0,'nominalExposureMultiplier':8},indent=2))


if __name__=='__main__':main()
