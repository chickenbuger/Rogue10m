"""One existing Niagara candidate, no screenshots, no authoring, no full render retry.
Prepared for explicit leased-editor execution only. Tests running -> paused -> running
cache semantics synchronously without yielding/ticking between the three observations.
UE5.8 NiagaraSimCache.cpp WriteFrame requires Running; PauseInstance sets Paused.
Read Age and Particles.Age independently to identify the actual dataset name.
No maps/settings/assets saved. Source/candidate/DA SHA256 checked before and after.
"""
from pathlib import Path
import argparse,hashlib,json
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def package(p):return ROOT/'Content'/(p.split('.')[0][6:]+'.uasset')

def observe(world,comp,phase):
    row={'phase':phase,'valid':unreal.SystemLibrary.is_valid(comp),'active':comp.is_active(),
         'paused':comp.is_paused(),'force_solo':comp.get_force_solo(),
         'component_asset':comp.get_editor_property('asset').get_path_name()}
    cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(world)
    params=unreal.NiagaraSimCacheCreateParameters()
    params.set_editor_property('attribute_capture_mode',unreal.NiagaraSimCacheAttributeCaptureMode.ALL)
    result=unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache,params,comp,False,.001)
    row.update(return_type=type(result).__module__+'.'+type(result).__name__,return_repr=repr(result)[:1500],
               cache_frames=cache.get_num_frames(),cache_emitters=cache.get_num_emitters(),native_capture_success=result is not None)
    if result is not None:
        if not isinstance(result,unreal.NiagaraSimCache):raise RuntimeError('Unexpected Python binding type '+row['return_type'])
        row['system_age_s']=result.get_start_seconds()
        row['emitters']=[]
        for i in range(result.get_num_emitters()):
            name=result.get_emitter_name(i)
            ages={key:list(result.read_float_attribute(key,name,0)) for key in ('Age','Particles.Age')}
            positions=list(result.read_position_attribute('Position',name,True,0))
            row['emitters'].append({'name':str(name),'age_attributes':ages,
                'position_instance_count':len(positions),'positions':[[v.x,v.y,v.z] for v in positions]})
    return row

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest',required=True);parser.add_argument('--candidate',required=True)
    parser.add_argument('--output',required=True);parser.add_argument('--lease-granted',action='store_true')
    a=parser.parse_args(argv)
    if not a.lease_granted:raise RuntimeError('Exclusive editor lease required')
    out=Path(a.output).resolve()
    if out.exists():raise RuntimeError('Fresh report path required; keep prior failures')
    manifest=json.loads(Path(a.manifest).read_text(encoding='utf-8-sig'))
    matches=[c for c in manifest['candidates'] if c['asset']==a.candidate]
    if len(matches)!=1 or not matches[0].get('saved') or matches[0].get('activated') is not False:raise RuntimeError('Require exactly one saved unactivated manifest candidate')
    candidate=matches[0]; lifetime=float(candidate['lifetime_sim_seconds'])
    if not 0<lifetime<=.5:raise RuntimeError('Invalid lifetime')
    paths=set(manifest['source_hashes_before'])|{c['asset'] for c in manifest['candidates']}|{c['existing_skill_asset'] for c in manifest['candidates']}
    before={str(package(p)):sha(package(p)) for p in paths}
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():raise RuntimeError('Editor world without PIE required')
    report={'status':'started','candidate':a.candidate,'world':world.get_path_name(),'screenshots':0,
        'scope':'one existing transient component only; no maps/assets/settings save',
        'protected_sha256_before':before,'observations':[],
        'manual_simulation':{'ticks':8,'delta_s':lifetime*.25/8,'requested_relative_age':.25,
            'requested_age_s':lifetime*.25,'scale':1.,'dilation':1.,'force_solo':True},
        'actual_dataset_verified':False,'rendered_visibility_verified':False}
    comp=None;out.parent.mkdir(parents=True,exist_ok=True)
    try:
        system=unreal.load_asset(a.candidate)
        if not isinstance(system,unreal.NiagaraSystem):raise RuntimeError('Existing system missing')
        comp=unreal.NiagaraFunctionLibrary.spawn_system_at_location(world,system,unreal.Vector(0,0,100),
            unreal.Rotator(),unreal.Vector(1,1,1),False,False,unreal.NCPoolMethod.NONE,False)
        if not comp:raise RuntimeError('Transient component spawn failed')
        report['component']=comp.get_path_name()
        comp.set_force_solo(True);comp.set_age_update_mode(unreal.NiagaraAgeUpdateMode.TICK_DELTA_TIME)
        comp.set_custom_time_dilation(1.);comp.activate(True);comp.advance_simulation(8,lifetime*.25/8)
        # Deliberate state-comparison experiment, not retries of a failed mutation.
        report['observations'].append(observe(world,comp,'running_after_advance'))
        comp.set_paused(True)
        report['observations'].append(observe(world,comp,'paused_expected_capture_rejection'))
        comp.set_paused(False)
        report['observations'].append(observe(world,comp,'running_restored_without_tick'))
        comp.set_paused(True)
        first,middle,last=report['observations'];tol=lifetime*.25/8+.0001
        valid=[]
        for row in (first,last):
            values=[(e['age_attributes'][k],e['position_instance_count']) for e in row.get('emitters',[]) for k in ('Age','Particles.Age') if e['age_attributes'][k]]
            valid.append(row['native_capture_success'] and len(values)==1 and len(values[0][0])==1 and values[0][1]==1
                and abs(values[0][0][0]-lifetime*.25)<=tol and abs(row['system_age_s']-lifetime*.25)<=tol)
        report['actual_dataset_verified']=all(valid)
        report['paused_capture_rejected']=middle['native_capture_success'] is False
        report['status']='probe_passed' if all(valid) and not middle['native_capture_success'] else 'probe_inconclusive_inspect_observations'
    except Exception as exc:
        report.update(status='probe_failed_no_retry',error=str(exc))
        raise
    finally:
        if comp and unreal.SystemLibrary.is_valid(comp):
            try:comp.destroy_component(comp.get_owner());report['owned_component_destroyed']=True
            except Exception as exc:report['cleanup_error']=str(exc)
        after={p:sha(Path(p)) for p in before};report['protected_sha256_after']=after
        report['protected_assets_unchanged']=before==after
        out.write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('RESULT=ONE_PARTICLE_SIMCACHE_PROBE '+report['status'])
    return report

if __name__=='__main__':main()
