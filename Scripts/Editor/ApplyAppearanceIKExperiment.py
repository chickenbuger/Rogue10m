"""Isolate HumanMale live IK effects after the root-only experiment.
ROGUE_RETARGET_IK_MODE=no_ik (default) or restore. Only Run IK Rig enabled state changes.
"""
from pathlib import Path
import hashlib
import json
import os
import unreal

PROJECT=Path(unreal.Paths.project_dir())
ASSET='/Game/Character/Customization/Retargeting/IKR_Manny_To_Hu_M'
FILE=PROJECT/'Content/Character/Customization/Retargeting/IKR_Manny_To_Hu_M.uasset'
EVIDENCE=PROJECT/'tmp/appearance-retarget/live-no-ik'
def sha():return hashlib.sha256(FILE.read_bytes()).hexdigest()
def main():
    mode=os.environ.get('ROGUE_RETARGET_IK_MODE','no_ik')
    if mode not in ('no_ik','restore'):raise RuntimeError('Invalid mode '+mode)
    EVIDENCE.mkdir(parents=True,exist_ok=True);manifest=EVIDENCE/'before.json'
    asset=unreal.load_asset(ASSET)
    if not asset:raise RuntimeError('HumanMale retargeter missing')
    ctl=unreal.IKRetargeterController.get_controller(asset)
    indices=[i for i in range(ctl.get_num_retarget_ops()) if str(ctl.get_op_name(i))=='Run IK Rig']
    roots=[i for i in range(ctl.get_num_retarget_ops()) if str(ctl.get_op_name(i))=='Root Motion']
    if len(indices)!=1 or len(roots)!=1:raise RuntimeError('Expected one IK and one Root Motion op')
    index=indices[0]; root=ctl.get_op_controller(roots[0]);before_enabled=ctl.get_retarget_op_enabled(index)
    if str(root.get_source_root_bone()).lower()!='root' or str(root.get_target_root_bone()).lower()!='root':
        raise RuntimeError('Root-only fix must remain enabled for this isolated experiment')
    if mode=='no_ik':
        previous=json.loads((PROJECT/'tmp/appearance-retarget/live-root-only/apply-result.json').read_text(encoding='utf-8'))
        if sha()!=previous['after_sha256']:raise RuntimeError('Asset differs from measured root-only baseline')
        if not before_enabled:raise RuntimeError('Run IK Rig already disabled')
        if manifest.exists():raise RuntimeError('Restore manifest already exists; do not overwrite history')
        (EVIDENCE/'IKR_Manny_To_Hu_M.uasset.backup').write_bytes(FILE.read_bytes())
        manifest.write_text(json.dumps({'asset':ASSET,'sha256':sha(),'op_enabled':before_enabled,
            'settings':ctl.get_op_controller(index).get_settings().export_text()},indent=2),encoding='utf-8')
        enabled=False
    else:
        previous=json.loads(manifest.read_text(encoding='utf-8'))
        applied=json.loads((EVIDENCE/'no_ik-result.json').read_text(encoding='utf-8'))
        if sha()!=applied['after_sha256'] or before_enabled:raise RuntimeError('Current state is not this IK experiment')
        enabled=previous['op_enabled']
    before_hash=sha()
    if not ctl.set_retarget_op_enabled(index,enabled):raise RuntimeError('Could not change IK enabled state')
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False):raise RuntimeError('Save failed')
    result={'mode':mode,'asset':ASSET,'before_sha256':before_hash,'after_sha256':sha(),'op_enabled':ctl.get_retarget_op_enabled(index),
            'source_root':str(root.get_source_root_bone()),'target_root':str(root.get_target_root_bone())}
    (EVIDENCE/(mode+'-result.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
    unreal.log('RESULT=APPEARANCE_IK_'+mode.upper()+'_COMPLETE '+json.dumps(result))
if __name__=='__main__':main()
