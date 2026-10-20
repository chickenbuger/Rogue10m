"""Bind newly authored finite species Niagara and save only owned effects/profiles."""
import json
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Feature/doc/evidence/monster-effects-chase-20261011'
NAMES=('VerdantSlime','Sporeling','CrimsonSlime','IronShell','ThornWarden','Azathor','AzathorWave','AzathorZone')
def main():
 if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():raise RuntimeError('Idle Editor required')
 fx={n:unreal.load_asset('/Game/Rogue10m/VFX/Monsters/NS_Monster_'+n) for n in NAMES}
 if not all(isinstance(v,unreal.NiagaraSystem) for v in fx.values()):raise RuntimeError('Missing authored species effect')
 report={}
 for name,system in fx.items():
  unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([system])
  unreal.EditorAssetLibrary.save_loaded_asset(system,only_if_is_dirty=False)
  report[name]={'path':system.get_path_name()}
 for species in NAMES[:6]:
  if species=='Azathor':p='/Game/Rogue10m/World/OpenWorld/Monsters/Azathor/DA_Azathor_BossPatterns'
  else:p='/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_BasicAttack_'+species
  profile=unreal.load_asset(p)
  if not isinstance(profile,unreal.Rogue10mMonsterAttackPatternProfile):raise RuntimeError('Missing profile '+p)
  profile.set_editor_property('attack_effect',fx[species]);profile.set_editor_property('impact_effect',fx[species])
  profile.set_editor_property('attack_effect_scale',1.0);profile.set_editor_property('impact_effect_scale',.65)
  if species=='Azathor':
   profile.set_editor_property('frontal_wave_effect',fx['AzathorWave']);profile.set_editor_property('abyss_zone_effect',fx['AzathorZone'])
  unreal.EditorAssetLibrary.save_loaded_asset(profile,only_if_is_dirty=False)
  report[species]['profile']=profile.get_path_name()
 OUT.mkdir(parents=True,exist_ok=True);(OUT/'application.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 print('MONSTER_FX_SAVED',len(fx),'systems, 6 profiles')
if __name__=='__main__':main()
