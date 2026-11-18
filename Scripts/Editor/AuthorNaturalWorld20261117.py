"""Exact-candidate, owned rendered-editor startup for the reviewed natural-world plan.

Runs one height application and one content pass, then waits for visual review.
The sole external signal is a one-shot, exact-hash save request; it cannot run
code, change parameters, load levels, or save foreign packages. PIE is owner
controlled through the official MCP tools. Importing outside the explicit
startup switch is rejected before callbacks or writes.
"""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import runpy
import sys
import time
import traceback
import unreal as u

ROOT = Path('D:/Project/Rogue10m')
BASE = ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117'
OUT = BASE/'author-live'
TARGET = '/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
OWN = '/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117'
QUEST = OWN+'/DA_IntroSupply_Flat'
state = {'phase':'registry','ticks':0,'started':time.monotonic(),'round':0,'last_world':None,'first_seen':None}


def require(value, message):
    if not value: raise RuntimeError(message)


def write(name, value):
    with (OUT/(name+'.json')).open('x',encoding='utf-8') as f:
        json.dump(value,f,ensure_ascii=False,indent=2,allow_nan=False)


def read(name):
    return json.loads((BASE/name).read_text(encoding='utf-8-sig'))


def module(name, relative):
    spec=importlib.util.spec_from_file_location(name,ROOT/relative)
    result=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def xyz(v): return [v.x,v.y,v.z]
def path(o): return o.get_path_name() if o else None
def dirty():
    return sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+
                   list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})


def context():
    es=u.get_editor_subsystem(u.UnrealEditorSubsystem)
    require(Path(u.Paths.project_dir()).resolve()==ROOT and
            es.get_editor_world().get_path_name().split('.')[0]==TARGET and not es.get_game_world(),
            'Exact idle candidate editor required')


def inspect(heights=False):
    d=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(heights))
    require(d.get('success') and d.get('dirty_package_names_unchanged'),'Native inspection failed: '+str(d.get('error')))
    return d


def native_apply():
    context(); require(not dirty(),'Clean saved baseline required')
    actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
    baseline=read('baseline-live/actors.json')
    require({a.get_path_name() for a in actors}=={r['path'] for r in baseline},'Baseline actor set differs')
    bypath={a.get_path_name():a for a in actors}
    for row in baseline:
        a=bypath[row['path']]
        require(max(abs(v-w) for v,w in zip(xyz(a.get_actor_location()),row['location']))<.02,
                'Baseline actor moved: '+row['label'])
    before=inspect(True);write('native-before',before)
    require(before['layer_count']==1 and before['landscape_reports_up_to_date'],'Original sole settled layer required')
    height=read('target-height-plan.json')
    height['expected_composite_blake3']=before['composite_blake3']
    height['expected_original_layer_blake3']=before['original_layer_blake3']
    write('applied-height-plan',height)
    result=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.apply_height_plan(json.dumps(height),True))
    write('apply-height',result)
    require(result.get('success') and result.get('original_layer_preserved'),'Height application failed: '+str(result.get('error')))
    state.update(before=before,apply=result,phase='terrain_settle',ticks=0)


def content():
    context()
    plan=read('author-plan.json')
    actors=u.get_editor_subsystem(u.EditorActorSubsystem)
    bypath={a.get_path_name():a for a in actors.get_all_level_actors()}
    report={'map':TARGET,'moves':[],'roads':[],'quest':None,'dressing':None,'saved':False}
    state['partial_content']=report
    for row in plan['moves']:
        a=bypath[row['path']]
        require(max(abs(v-w) for v,w in zip(xyz(a.get_actor_location()),row['before_location']))<.02,'Stale move '+row['label'])
        a.modify();a.root_component.modify()
        a.set_actor_location(u.Vector(*row['after_location']),False,False)
        require(max(abs(v-w) for v,w in zip(xyz(a.get_actor_location()),row['after_location']))<.02,'Move readback '+row['label'])
        report['moves'].append({'path':row['path'],'location':xyz(a.get_actor_location())})
    material_module=module('natural_road_material','Scripts/Editor/NaturalRoadMaterial20261117.py')
    try:
        material,material_report=material_module.build_material(allow_editor_mutation=True)
    except material_module.MaterialBuildError as e:
        write('material-failure',e.report);raise
    write('material',material_report)
    for row in plan['road_decals']:
        a=bypath[row['path']];c=a.get_component_by_class(u.DecalComponent)
        require(c is not None,'Missing road decal')
        a.modify();a.root_component.modify();c.modify()
        a.set_actor_location(u.Vector(row['location'][0],row['location'][1],9600),False,False)
        size=c.get_editor_property('decal_size')
        require(abs(size.y-400)<.01,'Unexpected original road width')
        c.set_editor_property('decal_size',u.Vector(500,size.y,size.z))
        c.set_decal_material(material)
        report['roads'].append({'path':path(a),'location':xyz(a.get_actor_location()),
                                'size':xyz(c.get_editor_property('decal_size')),'material':path(c.get_decal_material())})
    require(len(plan['director_baseline'])==1,'Single original director required')
    d=plan['director_baseline'][0];a=bypath[d['actor']]
    source=a.get_editor_property('quest_definition')
    require(path(source)==d['definition'] and not u.EditorAssetLibrary.does_asset_exist(QUEST),'Quest clone preflight')
    require(hashlib.sha256((ROOT/d['source_file']).read_bytes()).hexdigest()==d['source_sha256'],'Quest source disk changed')
    clone=u.EditorAssetLibrary.duplicate_asset(d['definition'].split('.')[0],QUEST)
    require(clone is not None,'Quest duplicate failed')
    clone.set_editor_property('layout_origin_cm',u.Vector(d['origin'][0],d['origin'][1],9600))
    a.set_editor_property('quest_definition',clone)
    report['quest']={'actor':path(a),'source':path(source),'clone':path(clone),
                      'origin':xyz(clone.get_editor_property('layout_origin_cm')),
                      'source_sha256_after':hashlib.sha256((ROOT/d['source_file']).read_bytes()).hexdigest()}
    dressing=module('natural_dressing_author','Scripts/Editor/NaturalWorldDressing20261117.py')
    try:
        report['dressing']=dressing.build_dressing(plan['dressing'],allow_editor_mutation=True)
    except dressing.DressingBuildError as e:
        write('dressing-failure',e.report);raise RuntimeError('Dressing authoring failed; see preserved report') from None
    write('author-content',report)
    state.update(content=report,phase='content_settle',ticks=0)


def validate():
    validator=module('natural_validator','Scripts/Editor/ValidateNaturalWorld20261117.py')
    report=validator.validate_editor(native_before=state['before'],apply_report=state['apply'],author_report=state['content'])
    write(state.get('validation_name','validation'),report)
    require(report.get('passed'),'Structural QA failed; preserve unsaved state: '+str(report.get('failures')))
    state['validation']=report
    write('ready-for-visual-review',{'pid':os.getpid(),'passed_structural':True,'saved':False,
          'target_composite_blake3':state['apply']['target_composite_blake3'],
          'original_layer_blake3':state['before']['original_layer_blake3'],
          'dirty_packages':dirty(),'reviewed_save_signal':'review-save-request.json',
          'signal_action':'save_exact_natural_candidate','gameplay_acceptance':False})
    state['phase']='await_review'
    print('NATURAL_WORLD READY_FOR_VISUAL_REVIEW UNSAVED')


def save_after_review():
    request=OUT/'review-save-request.json'
    if not request.exists():return
    payload=json.loads(request.read_text(encoding='utf-8-sig'))
    require(set(payload)=={'action','target_composite_blake3','visual_review_passed'} and
            payload['action']=='save_exact_natural_candidate' and payload['visual_review_passed'] is True and
            payload['target_composite_blake3']==state['apply']['target_composite_blake3'],
            'Invalid one-shot review save signal; no parameters or code accepted')
    context();current=inspect()
    require(current['composite_blake3']==state['apply']['target_composite_blake3'] and
            current['original_layer_blake3']==state['before']['original_layer_blake3'],'State changed after review')
    capsule_validator=module('natural_capsule_validator','Scripts/Editor/ValidateNaturalCapsules20261117.py')
    capsule_report=capsule_validator.validate_capsules()
    write('capsule-validation',capsule_report)
    require(capsule_report.get('passed'),'Pawn capsule collision QA failed; no save')
    plan={'schema':'Rogue10m.NaturalBoundaryPackageSave.20261117.v1','map':TARGET,'confirm_save':True,
          'expected_composite_blake3':current['composite_blake3'],
          'expected_original_layer_blake3':current['original_layer_blake3'],'packages':dirty()}
    write('save-plan',plan)
    result=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.save_candidate_packages(json.dumps(plan)))
    write('save-result',result);require(result.get('success') and not dirty(),'Exact native save failed')
    state['walk']=module('natural_walk','Scripts/Editor/PlaytestLandscapeCrop20261116.py')
    state['walk'].EVIDENCE=BASE/'runtime-walk'
    state['phase']='waiting_PIE'
    write('saved-ready-for-PIE',{'saved':True,'owner_starts_PIE':True,'rounds':3,'dirty':dirty()})
    print('NATURAL_WORLD SAVED_READY_FOR_PIE')


def observe_pie():
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
    walk=state['walk']
    if state['phase']=='waiting_PIE':
        if world is None:
            state['last_world']=None;state['first_seen']=None;return
        if state['last_world'] is not None:return
        if state['first_seen'] is None:state['first_seen']=time.monotonic();return
        if time.monotonic()-state['first_seen']<3:return
        index=state['round']+1
        walk.begin_round(index,fresh_pie_confirmed=True,allow_default_run_start=True)
        state.update(round=index,last_world=world,phase='walking')
    elif state['phase']=='walking' and walk._runner.handle is None:
        result=json.loads((walk.EVIDENCE/('round-%d.json'%state['round'])).read_text(encoding='utf-8'))
        require(result.get('passed'),'Actual walk failed; no automatic retry')
        if state['round']==3:
            write('three-walks',walk.summarize());state['phase']='complete'
            u.unregister_slate_post_tick_callback(state['handle'])
            print('NATURAL_WORLD THREE_WALKS_COMPLETE')
        else:state['phase']='waiting_PIE'


def tick(_):
    try:
        if state['phase'] in ('waiting_PIE','walking'):observe_pie();return
        if state['phase']=='await_review':save_after_review();return
        require(time.monotonic()-state['started']<1200,'Authoring watchdog; no retry')
        if u.AssetRegistryHelpers.get_asset_registry().is_loading_assets():return
        state['ticks']+=1
        if state['phase']=='registry':
            context();require(not dirty(),'Clean startup required')
            result=json.loads(u.Rogue10mCrop20261114WorldAuthoring.load_candidate_region())
            write('region-load',result);require(result.get('success'),'Full retained region not loaded')
            state.update(phase='original_settle',ticks=0);return
        if state['phase']=='original_settle' and state['ticks']>=30:native_apply();return
        if state['phase']=='terrain_settle' and state['ticks']%30==0:
            current=inspect()
            require(current['original_layer_blake3']==state['before']['original_layer_blake3'],'Original layer changed')
            if current['landscape_reports_up_to_date'] and current['composite_blake3']==state['apply']['target_composite_blake3']:
                write('terrain-settled',current);content()
            return
        if state['phase']=='content_settle' and state['ticks']>=120:validate()
    except Exception:
        error=traceback.format_exc()
        failed_phase=state['phase']
        state['phase']='failed'
        handle=state.pop('handle',None)
        if handle is not None:u.unregister_slate_post_tick_callback(handle)
        write(state.get('failure_name','failure'),{'phase':failed_phase,'traceback':error,'dirty':dirty(),'partial_content':state.get('partial_content')})
        u.log_error('NATURAL_WORLD '+error)


def bootstrap():
    require('-RogueNaturalStage=author' in u.SystemLibrary.get_command_line(),'Explicit owned author startup required')
    sys.dont_write_bytecode=True
    OUT.mkdir(parents=True,exist_ok=False)
    runpy.run_path(str(ROOT/'Scripts/Editor/RestoreEditorCultureAfterSmoke.py'),run_name='__main__')
    write('startup',{'pid':os.getpid(),'map':TARGET,'command':u.SystemLibrary.get_command_line(),'started':time.time()})
    state['handle']=u.register_slate_post_tick_callback(tick)


if __name__=='__main__':bootstrap()
