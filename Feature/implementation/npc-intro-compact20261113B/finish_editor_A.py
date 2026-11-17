"""Capture authored NPC values then request a clean exit of this Work's exact owned Editor."""
from pathlib import Path
import importlib.util,json,os,unreal
R=Path('D:/Project/Rogue10m');E=R/'Feature/doc/evidence/npc-intro-compact20261113B'
launch=json.loads((E/'editor-owner-launch.json').read_text(encoding='utf-8'))
assert os.getpid()==launch['owner_pid'] and Path(unreal.Paths.project_dir()).resolve()==R
assert callable(getattr(unreal.SystemLibrary,'quit_editor',None))
spec=importlib.util.spec_from_file_location('intro_contract',R/'Feature/implementation/npc-intro-compact20261113B/intro_contract.py')
contract=importlib.util.module_from_spec(spec);spec.loader.exec_module(contract)
out=contract.capture();out['owned_editor_pid']=os.getpid()
with (E/'serialized-before-cold.json').open('x',encoding='utf-8') as f:json.dump(out,f,ensure_ascii=False,indent=2)
assert (E/'authoring-preservation.json').is_file()
with (E/'editor-A-normal-exit-request.json').open('x',encoding='utf-8') as f:json.dump({'owner_pid':os.getpid(),'dirty':out['dirty'],'method':'public SystemLibrary.quit_editor','metadata_clone_QA':False},f,indent=2)
print('Intro content saved and captured; requesting clean owned Editor exit.')
unreal.SystemLibrary.quit_editor()
