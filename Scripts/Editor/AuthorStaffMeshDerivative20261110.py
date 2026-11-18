from pathlib import Path
import sys,json,hashlib
root=Path(r'D:\Project\Rogue10m');sys.path.insert(0,str(root/'Scripts/Editor'))
from InspectWeaponFantasyVFX20261110 import Client
out=root/'Feature/doc/evidence/staff-equipment-20261110/StaffMeshDerivativeA'
if out.exists():raise RuntimeError('Fresh only')
out.mkdir(parents=True)
source='/Game/BattleWizardPolyart/Meshes/MagicStaffs/Staff01SM';dest='/Game/Weapons/Staff20261110/SM_BattleWizardStaff01'
manifest=json.loads((root/'Feature/doc/evidence/staff-equipment-20261110/BattleWizardStaffInstallA/install.json').read_text(encoding='utf-8-sig'))
def hashes():return {r['relative']:hashlib.sha256((root/'Content/BattleWizardPolyart'/r['relative']).read_bytes()).hexdigest() for r in manifest['files']}
before=hashes();report={'status':'started','source':source,'asset':dest,'source_before':before,'activated':False,'source_ownership_product':manifest['product'],'public_redistribution_verified':False}
def write():(out/'manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
c=Client('http://127.0.0.1:8001/mcp',out);ts='editor_toolset.toolsets.asset.AssetTools';desc=c.tool('describe_toolset',{'toolset_name':ts},'schema')
required={'exists','duplicate','update_metadata_tags','save_assets'}
if not required.issubset({t['name'].rsplit('.',1)[-1] for t in desc['tools']}):raise RuntimeError('Asset schema changed')
def call(name,args):return c.tool('call_tool',{'toolset_name':ts,'tool_name':name,'arguments':args},name)
write()
if call('exists',{'path':dest}) is not False:raise RuntimeError('Destination exists')
if call('duplicate',{'path':source,'new_path':dest}) is not True:raise RuntimeError('Duplicate failure/uncertain, do not retry')
report['status']='duplicated_unsaved';write()
if call('update_metadata_tags',{'asset_path':dest,'set_tags':{'Rogue10m.Source':source,'Rogue10m.SourceLicense':'Fab Standard; owned verified 2026-11-10','Rogue10m.Status':'Staff01 visual candidate; gameplay and grip pending'}}) is False:raise RuntimeError('Metadata failed')
if call('save_assets',{'asset_paths':[dest]}) is not True:raise RuntimeError('Save uncertain')
report.update(status='candidate_saved_runtime_unassigned',source_after=hashes(),saved=True)
report['source_preserved']=report['source_before']==report['source_after']
report['created_sha256']=hashlib.sha256((root/'Content'/f'{dest[6:]}.uasset').read_bytes()).hexdigest();write()
print('RESULT=STAFF_MESH_CANDIDATE_SAVED '+dest)
