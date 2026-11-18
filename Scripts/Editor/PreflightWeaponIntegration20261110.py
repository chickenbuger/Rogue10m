from pathlib import Path
import json,runpy,unreal
P=Path(unreal.Paths.project_dir()).resolve();O=P/'Feature/doc/evidence/sword-knuckle-20261110/ResumeSword1'
def main():
 rows=[]
 for weapon in ['GreatSword','Knuckle']:
  path='/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_'+weapon
  obj=unreal.load_asset(path)
  if not obj:raise RuntimeError('Missing profile '+path)
  values=list(obj.get_editor_property('active_technique_catalog'))
  rows.append({'profile':obj.get_path_name(),'active_catalog_count':len(values),'catalog_untouched':True})
 report={'read_only':True,'profiles':rows,'empty_catalog_legacy_gate_passed':all(r['active_catalog_count']==0 for r in rows)}
 (O/'catalog-preintegration-readback.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
 if not report['empty_catalog_legacy_gate_passed']:raise RuntimeError('Nonempty catalog; hold consumer integration until explicit review')
 print('RESULT=CATALOG_PROFILES_EMPTY_LEGACY_GATE_PASSED')
if __name__=='__main__':
 try:main()
 finally:unreal.SystemLibrary.quit_editor()