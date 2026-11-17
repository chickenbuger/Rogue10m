"""Read-only failed internal build input/editor/save integrity verification."""
import argparse,datetime,hashlib,json,types
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
PACKET=ROOT/'Feature/implementation/map-resume-20261113/internal-smoke-culture-utc-d'

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--seal',type=Path,required=True);p.add_argument('--seal-sha256',required=True);p.add_argument('--label',choices=('run04','run05'),required=True);a=p.parse_args()
 raw=(PACKET/'strict_primitives.py').read_bytes();assert hashlib.sha256(raw).hexdigest().upper()=='02DF5B26A566EBF528280B3DEAB1E97081E9CD7B3AAE6B47A8277C8D6A029980'
 c=types.ModuleType('failed_integrity_strict');c.__file__=str(PACKET/'strict_primitives.py');exec(compile(raw,c.__file__,'exec'),c.__dict__)
 s=c.read_json_pin({'path':str(a.seal),'sha256':a.seal_sha256});assert s['root']==str(ROOT) and s['release_eligible'] is False
 inputs=c.read_json_pin(s['input_manifest']);c.compare_inventory(inputs,c.input_inventory(ROOT),'failed internal build inputs')
 for pin in s['editor_products']:c.read_pin(pin)
 for save in s['protected_saves']:c.compare_inventory(c.read_json_pin(save['manifest']),c.scan_tree(save['root'],True),'failed internal build protected saves')
 journal=c.load(c.stable_bytes(Path(s['outputs']['evidence'])/'wrapper-journal.json'))
 v={'status':'post_failure_inputs_editor6_and_saves_unchanged','run':a.label,'time_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'seal':c.file_pin(a.seal),'wrapper_status':journal['status'],'wrapper_phase':journal['phase'],'uat_exit':journal['buildcookrun_exit'],'input_files':len(inputs['files']),'editor6_verified':len(s['editor_products']),'protected_save_roots':len(s['protected_saves']),'current_game_products':[c.file_pin(ROOT/'Binaries/Win64'/n) for n in ('Rogue10m.exe','Rogue10m.target') if (ROOT/'Binaries/Win64'/n).exists()],'game_products_may_change_only_through_normal_UBT':True,'gui_operations':0,'script':c.file_pin(Path(__file__))}
 out=ROOT/'Feature/doc/evidence/map-resume-20261113/InternalSmokeD'/f'{a.label}-post-failure-integrity.json'
 with out.open('x',encoding='utf-8',newline='\n') as stream:json.dump(v,stream,indent=2)
 print(json.dumps(v,indent=2))
if __name__=='__main__':main()
