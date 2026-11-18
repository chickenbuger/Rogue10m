"""Owned rendered startup: round existing ridge, then staged boss/village earth.

Only two fixed, exact-hash signals are accepted after the intermediate height
checkpoint. No supplied code, file path, parameters or broad save is accepted.
"""
import importlib.util
import json
import os
from pathlib import Path
import runpy
import sys
import time
import traceback
import unreal as u

ROOT=Path('D:/Project/Rogue10m')
BASE=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117'
OUT=BASE/'refinement-live'
TARGET='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
state={'phase':'registry','ticks':0,'started':time.monotonic()}


def require(value,message):
    if not value:raise RuntimeError(message)


def write(name,data):
    with (OUT/(name+'.json')).open('x',encoding='utf-8') as f:json.dump(data,f,ensure_ascii=False,indent=2)


def read(relative):return json.loads((BASE/relative).read_text(encoding='utf-8-sig'))
def dirty():return sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})


def module(name,relative):
    s=importlib.util.spec_from_file_location(name,ROOT/relative);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m


def context():
    editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
    require(Path(u.Paths.project_dir()).resolve()==ROOT and not editor.get_game_world() and
            editor.get_editor_world().get_path_name().split('.')[0]==TARGET,'Exact idle editor required')


def inspect():
    d=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate())
    require(d.get('success') and d.get('dirty_package_names_unchanged'),'Native read failed: '+str(d.get('error')))
    return d


def qa(stage):
    v=module('natural_refinement_validator_'+stage,'Scripts/Editor/ValidateNaturalWorld20261117.py')
    report=v.validate_editor(apply_report=state['applied'],refinement_stage=stage)
    write('validation-'+stage,report)
    require(report.get('passed'),'QA failed '+stage+': '+str(report.get('diagnostics'))+' '+str(report.get('traceback')))
    return report


def save(name):
    context();d=inspect()
    require(d['composite_blake3']==state['applied']['target_composite_blake3'] and
            d['original_layer_blake3']==state['original_hash'],'Height/source changed before bounded save')
    plan={'schema':'Rogue10m.NaturalBoundaryPackageSave.20261117.v1','map':TARGET,'confirm_save':True,
          'expected_composite_blake3':d['composite_blake3'],'expected_original_layer_blake3':d['original_layer_blake3'],'packages':dirty()}
    write(name+'-plan',plan)
    result=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.save_candidate_packages(json.dumps(plan)))
    write(name+'-result',result)
    require(result.get('success') and not dirty(),'Bounded save failed: '+str(result.get('error')))


def apply_revision():
    context();require(not dirty(),'Saved checkpoint required')
    before=inspect();write('native-before',before)
    old=read('author-live/apply-height.json');original=read('author-live/native-before.json')
    require(before['layer_count']==2 and before['landscape_reports_up_to_date'] and
            before['composite_blake3']==old['target_composite_blake3'] and
            before['original_layer_blake3']==original['original_layer_blake3'],'Unexpected starting terrain')
    require(len(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())==1400,'Expected saved1400 actors')
    plan=read('target-height-plan-rounded-ridge.json')
    plan.update(schema='Rogue10m.NaturalBoundaryHeightRevision.20261118.v1',
                expected_composite_blake3=before['composite_blake3'],
                expected_original_layer_blake3=before['original_layer_blake3'],expected_new_layer_guid=old['new_layer_guid'])
    write('applied-height-plan',plan)
    result=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.revise_height_plan(json.dumps(plan),True))
    write('apply-height',result)
    require(result.get('success') and result.get('original_layer_preserved'),'Revision failed: '+str(result.get('error')))
    state.update(applied=result,original_hash=before['original_layer_blake3'],phase='terrain_settle',ticks=0)


def stage_content():
    request=OUT/'content-request.json'
    if not request.exists():return
    data=json.loads(request.read_text())
    require(data=={'action':'author_boss_and_village_earth','target_composite_blake3':state['applied']['target_composite_blake3']},'Invalid fixed content signal')
    context();require(not dirty(),'Intermediate checkpoint must stay clean')
    state['phase']='authoring_content'
    boss=module('natural_boss_threshold','Scripts/Editor/AuthorNaturalBossThreshold20261118.py')
    try:report=boss.author_boss_threshold(allow_editor_mutation=True)
    except boss.BossThresholdError as e:
        write('boss-threshold-failure',e.report);raise RuntimeError('Boss threshold stopped; see report') from None
    write('boss-threshold',report)
    floor=module('natural_village_earth','Scripts/Editor/VillageEarthFloor20261118.py')
    try:report=floor.build_village_earth(allow_editor_mutation=True,
        expected_composite_blake3=state['applied']['target_composite_blake3'],expected_original_layer_blake3=state['original_hash'])
    except Exception as e:
        if hasattr(e,'report'):write('village-earth-failure',e.report)
        raise
    write('village-earth',report)
    state.update(phase='content_settle',ticks=0)


def final_save():
    p=OUT/'save-request.json'
    if not p.exists():return
    data=json.loads(p.read_text())
    require(data=={'action':'save_final_refinement','target_composite_blake3':state['applied']['target_composite_blake3'],'visual_review_passed':True},'Invalid fixed final-save signal')
    save('final-save')
    write('saved-final',{'saved':True,'visual_review_passed':True,'dirty':dirty(),'cold_verification_pending':True})
    state['phase']='complete';u.unregister_slate_post_tick_callback(state['handle'])
    print('NATURAL_REFINEMENT SAVED_FINAL')


def tick(_):
    if state.get('busy'):return
    state['busy']=True
    try:
        if state['phase']=='await_content':stage_content();return
        if state['phase']=='await_final_visual':final_save();return
        require(time.monotonic()-state['started']<1200,'Refinement watchdog')
        if u.AssetRegistryHelpers.get_asset_registry().is_loading_assets():return
        state['ticks']+=1
        if state['phase']=='registry':
            context();require(not dirty(),'Clean fresh editor required')
            d=json.loads(u.Rogue10mCrop20261114WorldAuthoring.load_candidate_region());write('region-load',d)
            require(d.get('success'),'Full45 region load failed')
            u.EditorLevelLibrary.editor_set_game_view(True)
            write('game-view',{'enabled':True,'purpose':'Hide editor-only markers and already-game-hidden boss debug support; no actor flags changed'})
            state.update(phase='initial_settle',ticks=0);return
        if state['phase']=='initial_settle' and state['ticks']>=30:apply_revision();return
        if state['phase']=='terrain_settle' and state['ticks']%30==0:
            d=inspect();require(d['original_layer_blake3']==state['original_hash'],'Original layer changed')
            if d['landscape_reports_up_to_date'] and d['composite_blake3']==state['applied']['target_composite_blake3']:
                write('terrain-settled',d);qa('height_only')
                c=module('natural_rounded_capsules','Scripts/Editor/ValidateNaturalCapsules20261117.py')
                report=c.validate_capsules();write('height-capsules',report);require(report.get('passed'),'Revised terrain capsule QA failed')
                save('height-save');state['phase']='await_content'
                write('height-ready',{'saved':True,'target_composite_blake3':d['composite_blake3'],'ready_for_visual_and_content':True})
                print('NATURAL_REFINEMENT HEIGHT_SAVED_READY')
            return
        if state['phase']=='content_settle' and state['ticks']>=120:
            qa('complete')
            write('ready-for-final-visual',{'target_composite_blake3':state['applied']['target_composite_blake3'],'saved':False,'actors':1404})
            state['phase']='await_final_visual';print('NATURAL_REFINEMENT CONTENT_READY_FOR_VISUAL')
    except Exception:
        error=traceback.format_exc();phase=state['phase'];state['phase']='failed'
        h=state.pop('handle',None)
        if h is not None:u.unregister_slate_post_tick_callback(h)
        write(state.get('failure_name','failure'),{'phase':phase,'traceback':error,'dirty':dirty()})
        u.log_error('NATURAL_REFINEMENT '+error)
    finally:state['busy']=False


def bootstrap():
    require('-RogueNaturalStage=refine' in u.SystemLibrary.get_command_line(),'Explicit owned refinement launch required')
    sys.dont_write_bytecode=True;OUT.mkdir(parents=True,exist_ok=False)
    runpy.run_path(str(ROOT/'Scripts/Editor/RestoreEditorCultureAfterSmoke.py'),run_name='__main__')
    write('startup',{'pid':os.getpid(),'map':TARGET,'stage':'refine','started':time.time()})
    state['handle']=u.register_slate_post_tick_callback(tick)


if __name__=='__main__':bootstrap()
