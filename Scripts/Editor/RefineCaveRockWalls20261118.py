"""Explicit unsaved assignment of existing rock material to two cave wall bins.

Import is inert. No assets, components, actors, instances, collision, lighting,
post-process, camera, terrain or source materials are created or edited.
The two bindings include each site's stone ceiling in the same frozen ISM bin.
"""
from __future__ import annotations
import argparse
import copy
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import traceback

ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'Feature/doc/evidence/caves20261118'
SPEC=BASE/'wall-texture-spec.json'
SPEC_SHA256='a4127f8ca450c50c114001d60c92e05da98c380c9597c22715f382c3827b3cd8'
CORE_SHA256='39b0150690e02a6edb453f91d0362603d9063dc3771dce9977d870fe184e6a74'
VISIBILITY_SHA256='ad6cd1efe5e64a29ba3d311df890d861819238379748f4be3d31379c886183f9'
TARGET='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
OWN='/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117'
OLD_MATERIAL=OWN+'/M_Natural_WarmStone'
NEW_MATERIAL=OWN+'/Caves20261118/MI_CaveRock'


class CaveWallTextureError(RuntimeError):
    def __init__(self,report):
        self.report=report
        super().__init__(json.dumps(report,ensure_ascii=False))


def require(value,message):
    if not value:raise RuntimeError(message)


def module(name,relative):
    spec=importlib.util.spec_from_file_location(name,ROOT/relative)
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value


def path(value):return value.get_path_name() if value else None


def serialized(value):
    return (json.dumps(value,sort_keys=True,ensure_ascii=False,separators=(',',':'),allow_nan=False)+'\n').encode('utf-8')


def read(relative,sha=None):
    raw=(BASE/relative).read_bytes()
    if sha:require(hashlib.sha256(raw).hexdigest()==sha,'Immutable input changed: '+relative)
    return json.loads(raw)


def build_spec():
    core=read('plan.json',CORE_SHA256);visibility=read('author-live/visibility.json',VISIBILITY_SHA256)
    structures=read('author-live/structures.json')
    require(visibility.get('success') and structures.get('success') and structures.get('plan_sha256')==CORE_SHA256,'Successful previous stages required')
    bindings=[]
    for site in structures['sites']:
        components=[c for c in site['components'] if c['role']=='stone_wall']
        require(len(components)==1,'One stone-wall bin per site required')
        c=components[0];expected=84 if site['site']=='Q' else 86
        require(len(c['instances'])==expected,'Walls plus one stone ceiling expected')
        bindings.append({'site_id':site['site'],'actor_path':site['actor_path'],'component_path':c['path'],
                         'package':site['package'],'instance_count':expected,
                         'wall_instances':expected-1,'stone_ceiling_instances':1,
                         'old_material':OLD_MATERIAL,'new_material':NEW_MATERIAL,'slot':0})
    return {'schema':'Rogue10m.CaveWallTextureSpec.20261118.v1','map':TARGET,
        'status':'planned_not_applied','structural_plan_sha256':CORE_SHA256,
        'visibility_receipt_sha256':VISIBILITY_SHA256,'binding_changes':bindings,
        'exact_bindings_changed':2,'total_structural_instances_preserved':286,
        'new_actors':0,'new_components':0,'new_assets':0,'new_instances':0,
        'reference_intent':'Root-selected Valheim-inspired continuous rock walls and ceiling behind retained boulder skin; no asset copying.',
        'preservation':'All transforms, instance data, collision, existing1404, camera, exposure, lights, source40 and seven cave-material states unchanged.',
        'visual_acceptance':'Pending actual render; reject and retain candidate if cube UVs look unsuitable. No automatic second attempt.'}


def effect_state(u,actors,visibility,inspector,api):
    return {'global_pp':visibility.full_pp_state(u,actors),'camera':visibility.full_camera_state(u,inspector),
        'lights':{path(c):visibility.light_state(c,api) for a in actors for c in a.get_components_by_class(u.PointLightComponent)},
        'local_pp':{path(c):{'settings':c.get_editor_property('settings').export_text(),
            'unbound':bool(c.get_editor_property('unbound')),'enabled':bool(c.get_editor_property('enabled')),
            'priority':float(c.get_editor_property('priority')),'blend_radius':float(c.get_editor_property('blend_radius')),
            'blend_weight':float(c.get_editor_property('blend_weight')),'parent':path(c.get_attach_parent())}
            for a in actors for c in a.get_components_by_class(u.PostProcessComponent)}}


def refine_cave_rock_walls(*,allow_editor_mutation=False,expected_spec_sha256,
                         expected_composite_blake3,expected_original_layer_blake3,expected_dirty_packages):
    report={'schema':'Rogue10m.CaveWallTextureRefinement.20261118.v1','success':False,
        'started_utc':datetime.now(timezone.utc).isoformat(),'mutation_started':False,
        'assets_saved':False,'save_called':False,'created_assets':[],'created_actors':[],
        'created_components':[],'instance_changes':[],'binding_changes':[],
        'automatic_retry':False,'automatic_rollback':False,'partial_work_deleted':False}
    try:
        require(allow_editor_mutation is True and expected_spec_sha256==SPEC_SHA256,'Explicit invocation and reviewed spec required')
        spec=read('wall-texture-spec.json',SPEC_SHA256);read('plan.json',CORE_SHA256)
        vis_receipt=read('author-live/visibility.json',VISIBILITY_SHA256)
        require(spec['schema']=='Rogue10m.CaveWallTextureSpec.20261118.v1' and spec['map']==TARGET,'Exact wall texture spec required')
        import unreal as u
        api=module('wall_texture_component_api','Scripts/Editor/NaturalWorldDressing20261117.py')
        inspector=module('wall_texture_inspector','Scripts/Editor/InspectCaveSites20261118.py')
        reader=module('wall_texture_graph_reader','Scripts/Editor/NaturalRoadMaterial20261117.py')
        actor_reader=module('wall_texture_actor_reader','Scripts/Editor/AuthorNaturalBossThreshold20261118.py')
        visibility=module('wall_texture_visibility_state','Scripts/Editor/RefineCaveVisibility20261118.py')
        api._guard(u)
        require(len(expected_dirty_packages)==len(set(expected_dirty_packages)) and set(api._dirty(u))==set(expected_dirty_packages),'Exact dirty set required')
        native=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(native.get('success') and native.get('landscape_reports_up_to_date') and
                native['composite_blake3']==expected_composite_blake3 and native['original_layer_blake3']==expected_original_layer_blake3,'Settled terrain hashes differ')
        subsystem=u.get_editor_subsystem(u.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors())
        require(len(actors)==1408,'Exact1408 scene required')
        before=actor_reader._snapshot_actors(u,actors,api);before_components=inspector.persistent_components(u,actors)
        baseline=read('baseline-live/baseline-actors.json')
        require({p:before[p] for p in baseline['actors']}==baseline['actors'] and
                {p:before_components[p] for p in baseline['actors']}==baseline['persistent_components'],'Original1404 actors changed')
        old_mat=u.load_asset(OLD_MATERIAL);new_mat=u.load_asset(NEW_MATERIAL)
        require(isinstance(new_mat,u.MaterialInterface) and path(new_mat).split('.')[0]==NEW_MATERIAL,'Existing owned rock material required')
        new_states={p:visibility.material_state(u,u.load_asset(p),reader) for p in vis_receipt['expected_material_states_after']}
        require(new_states==vis_receipt['expected_material_states_after'] and len(new_states)==7,'Current seven material states differ')
        source_baseline=read('baseline-live/baseline-materials.json')['materials']
        source_before={p:inspector.material_state(u,u.load_asset(p),reader) for p in source_baseline}
        require(source_before==source_baseline and len(source_before)==40,'Original40 source materials changed')
        components={path(c):c for a in actors for c in a.get_components_by_class(u.InstancedStaticMeshComponent)}
        targets=[]
        structure_receipt=read('author-live/structures.json');checked=0
        for site in structure_receipt['sites']:
            for rec in site['components']:
                c=components.get(rec['path']);require(c is not None and c.get_instance_count()==len(rec['instances']),'Structure bin/count changed')
                for row in rec['instances']:
                    api._same_transform(row['observed_world_transform'],api._instance_transform(u,c,row['index']));checked+=1
        require(checked==286,'All286 structural transforms required')
        actors_by_path={path(a):a for a in actors}
        for binding in spec['binding_changes']:
            c=components.get(binding['component_path'])
            require(c is not None and c.get_owner()==actors_by_path[binding['actor_path']] and
                    c.get_instance_count()==binding['instance_count'] and c.get_num_materials()==1 and c.get_material(0)==old_mat,
                    'Exact old two stone-wall material bindings required; no overwrite/retry')
            targets.append((binding,c))
        effects_before=effect_state(u,actors,visibility,inspector,api)
        require(set(api._dirty(u))==set(expected_dirty_packages),'Read-only wall preflight dirtied a package')
        with u.ScopedEditorTransaction('Continuous cave rock wall and ceiling material 20261118'):
            report['mutation_started']=True
            for binding,c in targets:
                c.get_owner().modify();c.modify();c.set_material(0,new_mat)
                require(c.get_material(0)==new_mat,'Rock material binding readback differs')
                report['binding_changes'].append({**binding,'observed_material':path(c.get_material(0))})
        after_actors=list(subsystem.get_all_level_actors())
        require({path(a) for a in after_actors}==set(before),'Actor set changed')
        after=actor_reader._snapshot_actors(u,after_actors,api);after_components=inspector.persistent_components(u,after_actors)
        normalized=copy.deepcopy(after);target_paths={b['component_path'] for b,_ in targets}
        for actor_path,value in normalized.items():
            for primitive in value['primitives']:
                if primitive['path'] in target_paths:
                    previous=next(p for p in before[actor_path]['primitives'] if p['path']==primitive['path'])
                    primitive['materials']=previous['materials']
        require(normalized==before and after_components==before_components,'Unapproved geometry/collision/component/material reference changed')
        require(effect_state(u,after_actors,visibility,inspector,api)==effects_before,'Lighting/camera/exposure changed')
        require({p:visibility.material_state(u,u.load_asset(p),reader) for p in new_states}==new_states and
                {p:inspector.material_state(u,u.load_asset(p),reader) for p in source_before}==source_before,'A material graph/parameter/file changed')
        native_after=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(native_after.get('success') and native_after['composite_blake3']==expected_composite_blake3 and
                native_after['original_layer_blake3']==expected_original_layer_blake3,'Terrain changed')
        allowed=set(expected_dirty_packages)|{b['package'] for b,_ in targets}
        require(set(api._dirty(u))<=allowed,'Unexpected dirty package after wall material assignment')
        report.update(success=True,spec_sha256=SPEC_SHA256,structural_plan_sha256=CORE_SHA256,
            visibility_receipt_sha256=VISIBILITY_SHA256,actor_count=1408,all286_structural_transforms_preserved=True,
            original1404_actors_preserved=True,all_other_material_references_preserved=True,
            all_geometry_collision_lighting_camera_exposure_preserved=True,source40_and_new7_material_states_preserved=True,
            material_states_unchanged=new_states,dirty_before=list(expected_dirty_packages),dirty_after=api._dirty(u),
            allowed_new_dirty_packages=sorted({b['package'] for b,_ in targets}),
            finished_utc=datetime.now(timezone.utc).isoformat(),
            validation_pending=['actual continuous-rock appearance and cube UV review','updated structural QA','bounded save and cold reload'])
        return report
    except Exception:
        report['traceback']=traceback.format_exc();raise CaveWallTextureError(report) from None


def main():
    parser=argparse.ArgumentParser(description='Pure wall-texture spec generation only.')
    parser.add_argument('--write-spec',action='store_true');args=parser.parse_args();require(args.write_spec,'Explicit --write-spec required')
    value=build_spec()
    with SPEC.open('xb') as stream:stream.write(serialized(value))
    print(json.dumps({'spec_sha256':hashlib.sha256(serialized(value)).hexdigest(),'bindings':value['binding_changes']},indent=2))


if __name__=='__main__':main()
