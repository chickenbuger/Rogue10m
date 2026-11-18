"""Read-only snapshot differences after the explicit overlay."""
from pathlib import Path
import importlib.util
import json
import unreal as u
ROOT=Path('D:/Project/Rogue10m');OUT=ROOT/'tmp/OrganicCaves20261118/scene-after.json';assert not OUT.exists()
def load(name,file):
    s=importlib.util.spec_from_file_location(name,ROOT/file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
api=load('organic_diff_api','Scripts/Editor/NaturalWorldDressing20261117.py')
reader=load('organic_diff_reader','Scripts/Editor/AuthorNaturalBossThreshold20261118.py')
caves=[a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()if a.get_actor_label().startswith('CV18_')]
with OUT.open('x',encoding='utf-8',newline='\n')as f:json.dump(reader._snapshot_actors(u,caves,api),f,indent=2)
print('ORGANIC_SCENE_READ_ONLY_SNAPSHOT')
