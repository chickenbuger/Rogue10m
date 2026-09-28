"""Apply the measured pose-preserving retarget policy to all six appearance retargeters.
Edits through Unreal Editor controllers; saves only these six retargeters.
"""
from pathlib import Path
import hashlib
import json
import runpy
import unreal

PROJECT=Path(unreal.Paths.project_dir())
ROOT='/Game/Character/Customization/Retargeting'
CODES=('Hu_M','Hu_F','Dw_M','Dw_F','Or_M','Or_F')
EVIDENCE=PROJECT/'tmp/appearance-retarget/final-policy'
def file_for(asset):return PROJECT/'Content'/Path(asset.removeprefix('/Game/')+'.uasset')
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    manifest=EVIDENCE/'before.json'
    if manifest.exists():raise RuntimeError('Final-policy backup already exists; refusing to overwrite history')
    original=json.loads((PROJECT/'tmp/appearance-retarget/retarget-audit.json').read_text(encoding='utf-8'))
    original_by_path={row['asset']:row for row in original['retargeters']}
    human_experiment=json.loads((PROJECT/'tmp/appearance-retarget/live-no-ik/no_ik-result.json').read_text(encoding='utf-8'))
    scope=[];before={}
    # Validate every input and retain all backups before the first editor mutation.
    for code in CODES:
        path=f'{ROOT}/IKR_Manny_To_{code}';asset=unreal.load_asset(path)
        if not asset:raise RuntimeError('Missing '+path)
        ctl=unreal.IKRetargeterController.get_controller(asset)
        ops=[{'name':str(ctl.get_op_name(i)), 'enabled':ctl.get_retarget_op_enabled(i),
              'settings':ctl.get_op_controller(i).get_settings().export_text()} for i in range(ctl.get_num_retarget_ops())]
        if code=='Hu_M':
            if sha(file_for(path))!=human_experiment['after_sha256']:
                raise RuntimeError('Hu_M differs from the measured root-no-IK experiment')
        else:
            expected=[{key:entry[key] for key in ('name','enabled','settings')} for entry in original_by_path[path]['ops']]
            if ops!=expected:raise RuntimeError('Retargeter changed since audit: '+path)
        before[path]={'sha256':sha(file_for(path)),'ops':ops}
        scope.append((path,asset,ctl))
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    for path,asset,ctl in scope:
        (EVIDENCE/(asset.get_name()+'.uasset.backup')).write_bytes(file_for(path).read_bytes())
    manifest.write_text(json.dumps(before,indent=2),encoding='utf-8')
    helper=runpy.run_path(str(PROJECT/'Scripts/Editor/CreateInheritedCharacterAssets.py'),run_name='appearance_generator_helpers')
    result={}
    for path,asset,ctl in scope:
        policy=helper['configure_pose_preserving_retargeter'](ctl)
        if policy['run_ik_enabled'] or policy['source_root'].lower()!='root' or policy['target_root'].lower()!='root':
            raise RuntimeError('Policy validation failed: '+path)
        for chain in policy['unmapped_accessories']:
            if str(ctl.get_source_chain(chain))!='None':raise RuntimeError('Accessory still mapped: '+chain)
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False):raise RuntimeError('Save failed: '+path)
        result[path]={'policy':policy,'before_sha256':before[path]['sha256'],'after_sha256':sha(file_for(path))}
        (EVIDENCE/'result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    unreal.log('RESULT=APPEARANCE_RETARGET_POLICY_COMPLETE '+json.dumps(result))
if __name__=='__main__':main()
