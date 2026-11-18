"""Read retained NativeD/E5/Native51B evidence; no engine/process/QA operations."""
import json
from pathlib import Path
import re
import types
import internal_smoke as m


def audit():
    c=m.helpers();root=Path(c.ORIGINAL);old=root/'Feature/doc/evidence/map-resume-20261113';base=root/'Feature/doc/evidence/npc-combat-credit-20261113A';native=base/'Native51B'
    refs={k:c.file_pin(native/v) for k,v in {'process':'process.json','result':'result.json','reviewed':'reviewed-result.json','report':'report/index.json','log':'editor.log','selection':'selection.json'}.items()}
    refs.update(build_result=c.file_pin(base/'editor-build-fixture-result.json'),launcher=c.file_pin(root/'Feature/implementation/npc-combat-credit-20261113A/run_native51_b.py'),preservation=c.file_pin(base/'preservation-result.json'))
    module=types.ModuleType('retained_native51_audit');module.__file__=str(m.HERE/'native51_validation.py')
    exec(compile(c.stable_bytes(module.__file__),module.__file__,'exec'),module.__dict__)
    sources={scope+'/'+p['path']:p['sha256'] for scope in ('Source','Config') for p in c.scan_tree(root/scope)['files']}
    dll=c.file_pin(root/'Binaries/Win64/UnrealEditor-Rogue10m.dll');build=c.read_json_pin(refs['build_result'])
    summary=module.validate_native51(refs,{'root':root,'project':str(root/'Rogue10m.uproject'),'engine':r'D:\Program Files\UE_5.8','dll_sha256':dll['sha256'],'source_config':sources,'editor_finished_utc':build['ended_utc'],'read_pin':c.read_pin,'policy':c.load(c.stable_bytes(m.HERE/'native51-policy.json'))})
    d_refs={k:c.file_pin(old/'NativeTestsD'/name) for k,name in {'result':'result.json','reviewed':'reviewed-result.json','report':'report/index.json','log':'editor.log','preflight':'internal-smoke-native-preflight.json'}.items()}
    d_log=c.read_pin(d_refs['log']).decode('utf-8-sig');d_pre=c.read_json_pin(d_refs['preflight'])
    d_report=c.read_json_pin(d_refs['report']);d_result=c.read_json_pin(d_refs['result'])
    d_names={t['fullTestPath'] for t in d_report['tests']}
    old43={name for name in c.load(c.stable_bytes(m.HERE/'native51-policy.json'))['tests'] if not name.startswith('Rogue10m.VillageQuest.CombatCredit.')}
    c.require(not c.RED.search(d_log) and len(d_report['tests'])==len(d_names)==43 and d_names==old43
              and d_report['succeeded']==37 and d_report['succeededWithWarnings']==6
              and all(d_report[k]==0 for k in ('failed','notRun','inProcess'))
              and all(t['state']=='Success' and t['errors']==0 for t in d_report['tests'])
              and d_result['exit_code']==0 and all(flag in d_result['command'] for flag in ('-culture=en','-language=en','-locale=en-US')),
              'Retained NativeD exact43/English culture/wholeRED0 provenance differs')
    delivery_pin=c.file_pin(old/'InternalSmokeD/persistent-delivery05.json');delivery=c.read_json_pin(delivery_pin)
    e5_manifest=c.read_json_pin(delivery['delivery_manifest']);post=c.read_json_pin(delivery['source_postflight']);identity=c.read_json_pin(delivery['source_iostore_verification'])
    c.require(delivery['files']==50 and delivery['payload_bytes']==1948283459 and delivery['release_eligible'] is False and post['status']=='internal_smoke_artifact_built_UNVERIFIED','Actual retained E5 artifact receipt differs')
    # Stored full manifests are referenced, not a new 1.95GB payload scan.
    lineage={'helper':c.file_pin(root/'Feature/implementation/map-resume-20261113/internal-smoke-cook-fix-e/strict_primitives.py'),
             'core':c.file_pin(root/'Feature/implementation/map-resume-20261113/internal-smoke-cook-fix-e/internal_smoke.py'),
             'wrapper':c.file_pin(root/'Feature/implementation/map-resume-20261113/internal-smoke-cook-fix-e/Invoke-InternalSmoke.ps1')}
    c.require(lineage['helper']['sha256']==m.HELPER_SHA,'Successful Cook-Fix-E helper provenance differs')
    return m.stamp({'schema':'R10-native51-retained-readonly-audit-v1','status':'validated_retained_numeric_and_artifact_provenance',
        'native51':summary,'native51_refs':refs,'current_source_config_files_observed':len(sources),'current_dll':dll,
        'nativeD_refs':d_refs,'nativeD_whole_red_count':0,'nativeD_preflight':d_pre,'old_nativeA_startup15_applies_to_native51':False,
        'preservedE5_receipt':delivery_pin,'preservedE5_delivery_manifest':delivery['delivery_manifest'],
        'E5_files':50,'E5_bytes':1948283459,'E5_payload_sha256':delivery['payload_sha256'],'E5_contains_NPC_credit_fix':False,
        'E5_postflight':delivery['source_postflight'],'E5_iostore':delivery['source_iostore_verification'],
        'E5_postflight_status':post['status'],'E5_iostore_status':identity.get('status'),'cook_fix_E_runtime':lineage,
        'new_ue_ubt_uat_cook_game_executions':0,'new_gui_operations':0,'approval_or_metadata_clone_QA_operations':0,
        'final_content_captured_or_frozen':False,'packaging_status':'NOT_EXECUTED'})


if __name__=='__main__':
    result=audit();output=m.HERE/'results/retained-readonly-audit-final.json';output.parent.mkdir(exist_ok=True)
    with output.open('x',encoding='utf-8',newline='\n') as f:json.dump(result,f,ensure_ascii=False,indent=2);f.write('\n')
    print(json.dumps({k:result[k] for k in ('status','current_source_config_files_observed','current_dll','nativeD_whole_red_count','E5_files','E5_bytes','E5_contains_NPC_credit_fix','final_content_captured_or_frozen')},indent=2))
