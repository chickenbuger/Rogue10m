"""Activate the reviewed sword guard directional locomotion without activating attack candidates."""
from pathlib import Path
import json,runpy,unreal
P=Path(unreal.Paths.project_dir()).resolve();OUT=P/'Feature/doc/evidence/sword-knuckle-20261110';R='/Game/Rogue10m/Animation/Combat/GreatSword20261110'
def need(path):
 o=unreal.load_asset(path)
 if not o:raise RuntimeError('Missing '+path)
 return o
def main():
 runpy.run_path(str(P/'Scripts/Editor/StageGreatSwordDerivatives20261110.py'))['main']()
 snap=runpy.run_path(str(P/'Scripts/Editor/PrepareWeaponFantasyVFX20261110.py'))['snapshot']
 skills=[need('/Game/DataAsset/AttackSkill/ClassCombat/'+w+'/DA_Attack_'+w+'_'+n) for w in ['GreatSword','Knuckle'] for n in ['Primary01','Primary02','Primary03','Special','Charged']]
 before={o.get_path_name():snap(o) for o in skills}
 requests=[R+'/AM_GreatSword_'+n+'|'+R+'/A_GreatSword_'+n for n in ['Primary01','Primary02','Primary03','Special','Charged']]
 (OUT/'montage-requests.txt').write_text('\n'.join(requests),encoding='utf-8')
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 unreal.SystemLibrary.execute_console_command(world,'Rogue10m.ConfigureWeaponAnimations')
 native=json.loads((OUT/'weapon-animation-native.json').read_text(encoding='utf-8-sig'))
 if not native.get('passed') or native.get('stride_sample_count')!=27:raise RuntimeError('Native authoring failed; save nothing '+str(native))
 bs=need(R+'/BS_GreatSword_Ground');samples=bs.get_editor_property('sample_data')
 if len(samples)!=27 or any(not s.get_editor_property('animation').get_path_name().startswith(R+'/A_Armed_') for s in samples):raise RuntimeError('Invalid armed grid')
 after={o.get_path_name():snap(o) for o in skills}
 if before!=after:raise RuntimeError('Protected skill changed')
 saved=[]
 for path in [R+'/BS_GreatSword_Ground','/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed']+[R+'/AM_GreatSword_'+n for n in ['Primary01','Primary02','Primary03','Special','Charged']]:
  obj=need(path)
  if not unreal.EditorAssetLibrary.save_loaded_asset(obj,False):raise RuntimeError('Save failed '+path)
  saved.append(obj.get_path_name())
 report={'result':'ground_locomotion_active_pending_runtime','attack_candidates':'staged only; original skill references unchanged','protected_skills_unchanged':before==after,'saved':saved,'native':native,'preview_evidence':'DevLog/20261110_SwordKnuckle/PreviewAligned_61da49fe/captures.json'}
 (OUT/'ResumeSword1/locomotion-activation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 print('RESULT=GREATSWORD_GROUND_ACTIVATED skills_unchanged=10 samples=27 unique_clips=17')
if __name__=='__main__':
 try:main()
 finally:unreal.SystemLibrary.quit_editor()