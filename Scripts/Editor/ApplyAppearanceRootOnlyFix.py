"""Apply one isolated live retarget fix: HumanMale Root Motion root/pelvis separation.
Only IKR_Manny_To_Hu_M is saved. Set ROGUE_RETARGET_ROOT_ACTION=restore for an Editor API rollback.
"""
from pathlib import Path
import hashlib
import json
import os
import unreal

PROJECT=Path(unreal.Paths.project_dir())
ASSET='/Game/Character/Customization/Retargeting/IKR_Manny_To_Hu_M'
FILE=PROJECT/'Content/Character/Customization/Retargeting/IKR_Manny_To_Hu_M.uasset'
EVIDENCE=PROJECT/'tmp/appearance-retarget/live-root-only'

def sha(): return hashlib.sha256(FILE.read_bytes()).hexdigest()
def main():
    action=os.environ.get('ROGUE_RETARGET_ROOT_ACTION','apply')
    if action not in ('apply','restore'): raise RuntimeError('Invalid action '+action)
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    manifest=EVIDENCE/'before.json'
    asset=unreal.load_asset(ASSET)
    if not asset: raise RuntimeError('HumanMale retargeter missing')
    ctl=unreal.IKRetargeterController.get_controller(asset)
    indices=[i for i in range(ctl.get_num_retarget_ops()) if str(ctl.get_op_name(i))=='Root Motion']
    if len(indices)!=1: raise RuntimeError('Expected exactly one Root Motion op')
    index=indices[0]; root=ctl.get_op_controller(index)
    settings=root.get_settings()
    pelvis=str(settings.get_editor_property('target_pelvis').get_editor_property('bone_name'))
    before_source=str(root.get_source_root_bone());before_target=str(root.get_target_root_bone())
    old_settings=settings.export_text()
    if action=='apply':
        audit=json.loads((PROJECT/'tmp/appearance-retarget/retarget-ab.json').read_text(encoding='utf-8'))
        expected=audit['production_before_hashes'][ASSET]
        if sha()!=expected: raise RuntimeError('Production retargeter hash differs from A/B baseline; refusing to overwrite')
        if before_source.lower()!='pelvis' or before_target.lower()!='pelvis' or pelvis.lower()!='pelvis':
            raise RuntimeError('Expected the measured pelvis/pelvis Root Motion configuration')
        source_rig=unreal.IKRigController.get_controller(ctl.get_ik_rig(unreal.RetargetSourceOrTarget.SOURCE))
        target_rig=unreal.IKRigController.get_controller(ctl.get_ik_rig(unreal.RetargetSourceOrTarget.TARGET))
        new_source=str(source_rig.get_retarget_chain_start_bone('Root'))
        new_target=str(target_rig.get_retarget_chain_start_bone('root'))
        if new_source.lower()!='root' or new_target.lower()!='root': raise RuntimeError('Actual root chain not found')
        if manifest.exists(): raise RuntimeError('Existing restore manifest found; do not overwrite diagnostic history')
        (EVIDENCE/'IKR_Manny_To_Hu_M.uasset.backup').write_bytes(FILE.read_bytes())
        manifest.write_text(json.dumps({'asset':ASSET,'sha256':sha(),'source_root':before_source,'target_root':before_target,
            'target_pelvis':pelvis,'op_enabled':ctl.get_retarget_op_enabled(index),'settings':old_settings},indent=2),encoding='utf-8')
    else:
        if not manifest.exists(): raise RuntimeError('Restore manifest missing')
        previous=json.loads(manifest.read_text(encoding='utf-8'))
        applied=EVIDENCE/'apply-result.json'
        if not applied.exists() or sha()!=json.loads(applied.read_text(encoding='utf-8'))['after_sha256']:
            raise RuntimeError('Current asset does not match this experiment; refusing unrelated-state rollback')
        if before_source.lower()!='root' or before_target.lower()!='root' or pelvis.lower()!='pelvis':
            raise RuntimeError('Current settings no longer match this experiment')
        new_source=previous['source_root'];new_target=previous['target_root']
    before_hash=sha()
    root.set_source_root_bone(new_source);root.set_target_root_bone(new_target)
    # Trigger controller-managed reinitialization; the existing enabled value is preserved.
    ctl.set_retarget_op_enabled(index,ctl.get_retarget_op_enabled(index))
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False): raise RuntimeError('Retarget save failed')
    result={'action':action,'asset':ASSET,'before_sha256':before_hash,'after_sha256':sha(),
        'source_root':str(root.get_source_root_bone()),'target_root':str(root.get_target_root_bone()),
        'target_pelvis':str(root.get_settings().get_editor_property('target_pelvis').get_editor_property('bone_name')),
        'settings':root.get_settings().export_text()}
    (EVIDENCE/(action+'-result.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
    unreal.log('RESULT=APPEARANCE_ROOT_ONLY_'+action.upper()+'_COMPLETE '+json.dumps(result))

if __name__=='__main__': main()
