"""Owner-launched null-RHI baseline. Read-only asset/scene observation."""
import hashlib
import json
import os
from pathlib import Path
import time
import traceback
import unreal as u

ROOT=Path("D:/Project/Rogue10m")
TARGET="/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
OUT=ROOT/"Feature/doc/evidence/landscape-natural-boundary20261117/baseline-live"
state={"phase":"registry","ticks":0,"start":time.monotonic()}


def xyz(v):return [v.x,v.y,v.z]
def asset(o):return o.get_path_name() if o else None
def write(name,data):
    with (OUT/(name+".json")).open("x",encoding="utf-8") as f:json.dump(data,f,ensure_ascii=False,indent=2)
def require(v,msg):
    if not v:raise RuntimeError(msg)
def dirty():
    return sorted(p.get_path_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages()))


def inspect():
    es=u.get_editor_subsystem(u.UnrealEditorSubsystem)
    world=es.get_editor_world()
    require(world.get_path_name().split('.')[0]==TARGET and not es.get_game_world() and not dirty(),"Exact idle/clean candidate required")
    h=u.Rogue10mCrop20261114WorldAuthoring
    for name,fn in (("terrain",h.inspect_candidate_terrain),("snapshot",h.snapshot_retained_terrain_data),("dependencies",h.inspect_candidate_dependencies)):
        d=json.loads(fn());write(name,d);require(d.get('success') is True,name+" failed")
    rows=[];directors=[]
    for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
        r=a.get_actor_rotation()
        row={"path":asset(a),"label":a.get_actor_label(),"class":asset(a.get_class()),"package":a.get_package().get_name(),
             "location":xyz(a.get_actor_location()),"rotation":[r.pitch,r.yaw,r.roll],"scale":xyz(a.get_actor_scale3d()),
             "tags":list(map(str,a.tags)),"folder":str(a.get_folder_path())}
        c=a.get_component_by_class(u.StaticMeshComponent)
        if c:
            row.update(mesh=asset(c.get_editor_property('static_mesh')),materials=[asset(c.get_material(i)) for i in range(c.get_num_materials())],
                       visible=c.get_editor_property('visible'),hidden=a.get_editor_property('hidden'),collision=str(c.get_collision_profile_name()))
        if isinstance(a,u.Rogue10mVillageQuestDirector):
            d=a.get_editor_property('quest_definition');origin=d.get_editor_property('layout_origin_cm');rot=d.get_editor_property('layout_rotation')
            q=[]
            for v in d.get_editor_property('quests'):
                q.append({"quest_id":str(v.get_editor_property('quest_id')),"npc_location":xyz(v.get_editor_property('npc_location')),
                          "objectives":[xyz(p) for p in v.get_editor_property('objective_locations')]})
            p=ROOT/"Content"/(d.get_path_name().split('.')[0][6:]+'.uasset')
            directors.append({"actor":row['path'],"definition":asset(d),"origin":xyz(origin),"rotation":[rot.pitch,rot.yaw,rot.roll],
                              "quests":q,"source_file":p.relative_to(ROOT).as_posix(),"source_sha256":hashlib.sha256(p.read_bytes()).hexdigest()})
        rows.append(row)
    write('actors',rows);write('directors',directors)
    meshes={}
    for name in ('Cube','Cylinder','Cone'):
        m=u.load_asset('/Engine/BasicShapes/'+name);b=m.get_bounding_box();meshes[name]={"min":xyz(b.min),"max":xyz(b.max)}
    write('primitive-meshes',meshes)
    require(not dirty(),"Read-only baseline dirtied packages")
    write('result',{"passed":True,"pid":os.getpid(),"map":TARGET,"actor_count":len(rows),"director_count":len(directors),"dirty":dirty(),"assets_saved":False})


def tick(_):
    try:
        require(time.monotonic()-state['start']<300,"Baseline watchdog")
        if u.AssetRegistryHelpers.get_asset_registry().is_loading_assets():return
        if state['phase']=='registry':
            d=json.loads(u.Rogue10mCrop20261114WorldAuthoring.load_candidate_region());write('load-region',d);require(d['success'],"Region load failed");state['phase']='settle';return
        state['ticks']+=1
        if state['ticks']<15:return
        u.unregister_slate_post_tick_callback(state['handle']);inspect();u.SystemLibrary.quit_editor()
    except Exception:
        write('failure',{"traceback":traceback.format_exc(),"phase":state['phase'],"dirty":dirty()})
        u.log_error(traceback.format_exc())
        try:u.unregister_slate_post_tick_callback(state['handle'])
        except Exception:pass
        u.SystemLibrary.quit_editor()


require('-RogueNaturalStage=inspect' in u.SystemLibrary.get_command_line() and '-unattended' in u.SystemLibrary.get_command_line(),"Explicit owned inspection launch required")
OUT.mkdir(parents=True,exist_ok=False)
state['handle']=u.register_slate_post_tick_callback(tick)
