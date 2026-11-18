import unreal, json
from pathlib import Path
ROOT = Path(r'D:/Project/Rogue10m')
out = ROOT/'Feature/doc/evidence/humanoid-boss-npc-20261110/asset-audit.json'
reg=unreal.AssetRegistryHelpers.get_asset_registry()
reg.search_all_assets(True)
roots=['/Game/Polyart/ModularStylizedChars/Meshes/SkeletalMeshes/Characters','/Game/Polyart/ModularStylizedChars/Animations','/Game/CloseCombatAnimSet','/Game/CombatMagicAnims','/Game/Characters/Mannequins','/Game/Rogue10m/Characters/Arin/RefinedV3','/Game/DataAsset/Monster/FinalBoss','/Game/Monster','/Game/Rogue10m/World']
rows=[]
for root in roots:
 for ad in reg.get_assets_by_path(root, recursive=True):
  cls=str(ad.asset_class_path.asset_name)
  if cls not in ['SkeletalMesh','Skeleton','AnimSequence','Rogue10mMonsterDataAsset','Rogue10mMonsterAttackPatternProfile','Rogue10mTutorialDataAsset']: continue
  obj=ad.get_asset()
  row={'path':obj.get_path_name(),'class':cls}
  for key in ['skeleton','skeletal_mesh','attack_pattern_profile','idle_animation','movement_animation','attack_animation','frontal_wave_animation','abyss_zone_animation','death_animation','mesh_relative_location','mesh_relative_rotation','mesh_relative_scale','animation_blueprint_class','monster_id','display_name']:
   try:
    v=obj.get_editor_property(key)
    row[key]=v.get_path_name() if isinstance(v,unreal.Object) else str(v)
   except Exception: pass
  if isinstance(obj,unreal.AnimSequence): row['length']=obj.get_play_length()
  if isinstance(obj,unreal.SkeletalMesh):
   try: row['bounds']=str(obj.get_bounds())
   except Exception: pass
  rows.append(row)
out.write_text(json.dumps(rows,indent=2),encoding='utf-8')
print('HUMANOID_AUDIT_DONE',len(rows))
