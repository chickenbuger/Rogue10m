"""Persist only six missing project-owned sword derivatives. Never activates skills."""
import json,unreal
from pathlib import Path
R='/Game/Rogue10m/Animation/Combat/GreatSword20261110'
E=unreal.EditorAssetLibrary
def need(path):
 obj=unreal.load_asset(path)
 if not obj:raise RuntimeError('Missing '+path)
 return obj
def main():
 pairs=[(need('/Game/Rogue10m/Animation/Locomotion/PalmReference/BS_Unarmed_PalmReference'),R+'/BS_GreatSword_Ground')]
 for name in ['Primary01','Primary02','Primary03','Special','Charged']:
  need(R+'/A_GreatSword_'+name)
  skill=need('/Game/DataAsset/AttackSkill/ClassCombat/GreatSword/DA_Attack_GreatSword_'+name)
  source=skill.get_editor_property('attack_montage')
  if not source:raise RuntimeError('Missing montage '+name)
  pairs.append((source,R+'/AM_GreatSword_'+name))
 rows=[]
 for source,target in pairs:
  existing=E.does_asset_exist(target)
  obj=need(target) if existing else E.duplicate_asset(source.get_path_name(),target)
  if not obj or (not existing and not E.save_loaded_asset(obj,False)):raise RuntimeError('Stage failed '+target)
  rows.append({'source':source.get_path_name(),'target':obj.get_path_name(),'newly_saved':not existing})
 out=Path(unreal.Paths.project_dir()).resolve()/'Feature/doc/evidence/sword-knuckle-20261110/ResumeSword1/staging.json'
 out.write_text(json.dumps({'activated':False,'assets':rows},indent=2),encoding='utf-8')
 print('RESULT=GREATSWORD_DERIVATIVES_STAGED assets='+str(len(rows)))
if __name__=='__main__':main()