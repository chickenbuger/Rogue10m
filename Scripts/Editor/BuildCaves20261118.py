"""Explicit phased cave authoring in the already-owned idle editor. No saves.

The observer uses a reentrancy guard because shader compilation pumps Slate.
Each one-shot author leaves partial work/evidence on failure; no automatic
retry, rollback, level switch, PIE, deletion, or package save is performed.
"""
from pathlib import Path
import hashlib
import importlib.util
import json
import os
import struct
import sys
import time
import traceback
import unreal as u

ROOT=Path('D:/Project/Rogue10m')
BASE=ROOT/'Feature/doc/evidence/caves20261118'
OUT=BASE/'author-live'
TARGET='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
PLAN_SHA='39b0150690e02a6edb453f91d0362603d9063dc3771dce9977d870fe184e6a74'
SOURCE_GRID_SHA='a7f20b232a14f887ba96387c828ed717c0b02f3065bb6e39ba1a3b60a2d37154'
state={'phase':'materials','busy':False,'started':time.monotonic(),'ticks':0}


def require(ok,message):
    if not ok:raise RuntimeError(message)


def module(name,rel):
    spec=importlib.util.spec_from_file_location(name,ROOT/rel)
    obj=importlib.util.module_from_spec(spec);spec.loader.exec_module(obj)
    return obj


def read(rel):return json.loads((BASE/rel).read_text(encoding='utf-8-sig'))


def write(name,obj):
    with (OUT/(name+'.json')).open('x',encoding='utf-8') as f:
        json.dump(obj,f,ensure_ascii=False,indent=2)


def dirty():
    return sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+
                   list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})


def context():
    require(os.getpid()==read('baseline-live/result.json')['pid'],'Original inspected editor process required')
    w=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    require(Path(u.Paths.project_dir()).resolve()==ROOT and w.get_path_name().split('.')[0]==TARGET
            and not u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor(),'Exact idle candidate required')


def inspect(heights=False):
    obj=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(heights))
    require(obj.get('success') and obj['component_count']==obj['collision_count']==45 and obj['layer_count']==2,
            'Registered retained45/two-layer candidate required')
    return obj


def save_structure_checkpoint():
    require(read('author-live/structures.json')['success'] and read('author-live/materials.json')['success'],
            'Successful additive material and structural receipts required')
    n=inspect(False);baseline=read('baseline-live/baseline-native.json')
    require(n['composite_blake3']==baseline['composite_blake3'] and
            n['original_layer_blake3']==baseline['original_layer_blake3'],'Original terrain required before structural checkpoint')
    expected=set(read('author-live/structures.json')['dirty_after'])
    require(set(dirty())==expected and len(expected)==12,'Exact planned twelve structural/material/folder packages required')
    plan={'schema':'Rogue10m.NaturalBoundaryPackageSave.20261117.v1','map':TARGET,'confirm_save':True,
          'expected_composite_blake3':n['composite_blake3'],'expected_original_layer_blake3':n['original_layer_blake3'],
          'packages':dirty()}
    write('structure-checkpoint-save-plan',plan)
    result=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.save_candidate_packages(json.dumps(plan)))
    write('structure-checkpoint-save-result',result)
    require(result.get('success') and not dirty(),'Exact structural checkpoint save failed; do not retry')


def apply_height(suffix=''):
    require(not dirty(),'Native height revision requires a saved clean structural checkpoint')
    before=inspect(True);baseline=read('baseline-live/baseline-native.json')
    require(before['composite_heights_uint16']==baseline['composite_heights_uint16']
            and hashlib.sha256(b''.join(struct.pack('<H',v) for v in before['composite_heights_uint16'])).hexdigest()==SOURCE_GRID_SHA,
            'Current whole grid must equal the reviewed rounded source, not an arbitrary fresh state')
    require(before['original_layer_blake3']==baseline['original_layer_blake3'],'Original layer changed')
    pure=module('cave_native_payload','Scripts/CaveLayoutPlan20261118.py')
    old=json.loads((ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117/author-live/apply-height.json').read_text())
    plan=pure.guarded_revision(read('target-height-plan.json'),expected_composite_blake3=before['composite_blake3'],
        expected_original_layer_blake3=before['original_layer_blake3'],expected_new_layer_guid=old['new_layer_guid'])
    write('applied-height-plan'+suffix,plan)
    result=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.revise_height_plan(json.dumps(plan),True))
    write('apply-height'+suffix,result)
    require(result.get('success') and result.get('original_layer_preserved'),'Native cave height revision failed')
    state.update(height=result,original=before['original_layer_blake3'],phase='settle',ticks=0)


def tick(_):
    if state.get('busy'):return
    state['busy']=True
    try:
        context()
        require(time.monotonic()-state['started']<1200,'Cave author watchdog expired')
        if u.AssetRegistryHelpers.get_asset_registry().is_loading_assets():return
        state['ticks']+=1
        if state['phase']=='materials':
            require(not dirty(),'Clean editor required at first mutation')
            state['phase']='authoring_materials'
            m=module('cave_material_author','Scripts/Editor/CaveMaterials20261118.py')
            try:r=m.create_cave_materials(allow_editor_mutation=True)
            except m.CaveMaterialError as e:write('material-failure',e.report);raise RuntimeError('Material author stopped; see original receipt') from None
            write('materials',r);require(r.get('success'),'Material author did not complete')
            state.update(materials=r,phase='structures',ticks=0);return
        if state['phase']=='structures':
            state['phase']='authoring_structures'
            m=module('cave_structure_author','Scripts/Editor/AuthorCaveSpaces20261118.py')
            n=inspect(False);roles=state['materials']['material_paths_by_role']
            try:r=m.author_cave_spaces(allow_editor_mutation=True,expected_plan_sha256=PLAN_SHA,
                expected_composite_blake3=n['composite_blake3'],expected_original_layer_blake3=n['original_layer_blake3'],
                expected_dirty_packages=dirty(),material_paths_by_role={k:roles[k] for k in m.ROLES})
            except m.CaveStructureError as e:write('structure-failure',e.report);raise RuntimeError('Structure author stopped; see original receipt') from None
            write('structures',r);require(r.get('success'),'Structure author did not complete')
            state.update(structures=r,phase='structure_checkpoint',ticks=0);return
        if state['phase']=='structure_checkpoint':
            state['phase']='saving_structure_checkpoint';save_structure_checkpoint()
            state.update(phase='height',ticks=0);return
        if state['phase']=='height':
            state['phase']='applying_height';apply_height();return
        if state['phase']=='settle' and state['ticks']%10==0:
            n=inspect(False)
            require(n['original_layer_blake3']==state['original'],'Original layer changed while settling')
            if n['landscape_reports_up_to_date'] and n['composite_blake3']==state['height']['target_composite_blake3']:
                write('terrain-settled',n)
                state['phase']='await_dressing'
                write('ready-for-dressing',{'height_hash':n['composite_blake3'],'actors':1406,'dirty':dirty(),'saved':False})
                print('CAVES STRUCTURES_HEIGHT_READY');return
        if state['phase']=='await_dressing':
            request=OUT/'dressing-request.json'
            if not request.exists():return
            request=json.loads(request.read_text())
            require(request.get('action')=='author_reviewed_cave_dressing' and
                    request.get('dressing_plan_sha256')==hashlib.sha256((BASE/'dressing-plan.json').read_bytes()).hexdigest(),
                    'Exact reviewed dressing request required')
            state['phase']='authoring_dressing'
            m=module('cave_dressing_author','Scripts/Editor/AuthorCaveDressing20261118.py')
            paths=[a.get_path_name() for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()]
            roles=state['materials']['material_paths_by_role']
            try:r=m.author_cave_dressing(allow_editor_mutation=True,expected_plan_sha256=request['dressing_plan_sha256'],
                expected_composite_blake3=state['height']['target_composite_blake3'],expected_original_layer_blake3=state['original'],
                material_paths_by_role={k:roles[k] for k in m.ROLES},expected_dirty_packages=dirty(),expected_actor_paths=paths)
            except Exception as e:
                if hasattr(e,'report'):write('dressing-failure',e.report)
                raise
            write('dressing',r);require(r.get('success'),'Dressing author did not complete')
            u.EditorLevelLibrary.editor_set_game_view(True)
            write('authored-ready',{'success':True,'actors':1408,'dirty':dirty(),'saved':False,'validation_pending':True})
            state['phase']='complete';u.unregister_slate_post_tick_callback(state['handle'])
            print('CAVES AUTHORING_COMPLETE_UNSAVED');return
    except Exception:
        error=traceback.format_exc();phase=state['phase'];state['phase']='failed'
        h=state.pop('handle',None)
        if h is not None:u.unregister_slate_post_tick_callback(h)
        write('failure',{'phase':phase,'traceback':error,'dirty':dirty(),'automatic_retry':False})
        u.log_error('CAVES AUTHOR '+error)
    finally:state['busy']=False


def bootstrap():
    sys.dont_write_bytecode=True
    context()
    require(hashlib.sha256((BASE/'plan.json').read_bytes()).hexdigest()==PLAN_SHA,'Immutable plan changed')
    OUT.mkdir(exist_ok=False)
    write('startup',{'pid':os.getpid(),'target':TARGET,'started_monotonic':state['started'],'dirty':dirty()})
    state['handle']=u.register_slate_post_tick_callback(tick)
    print('CAVES AUTHOR_ARMED')


if __name__=='__main__':bootstrap()
