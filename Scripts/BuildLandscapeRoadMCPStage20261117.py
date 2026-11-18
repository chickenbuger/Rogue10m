"""Build reviewable arguments for official registered road-editor tool batches.

This host-side builder performs no Unreal calls. Each output is exclusive.
The actual editor results are retained separately by InvokeUnrealMCP.py.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Feature/doc/evidence/landscape-resume-20261117"
PLAN = ROOT / "Feature/doc/evidence/landscape-road-surface20261117/preflight.json"
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"

HEADER = '''import json

def tool(group,name,args):
 return execute_tool("editor_toolset.toolsets."+group+"."+name,json.dumps(args))
def scene(name,args):
 return tool("scene.SceneTools",name,args)
def actor(name,args):
 return tool("actor.ActorTools",name,args)
def props(obj,keys):
 return json.loads(tool("object.ObjectTools","get_properties",{"instance":obj,"properties":keys})["returnValue"])
def setprops(obj,values):
 assert tool("object.ObjectTools","set_properties",{"instance":obj,"values":json.dumps(values)})["returnValue"] is True
def find(name,tag=""):
 return scene("find_actors",{"name":name,"tag":tag,"collision_channels":[]})["returnValue"]
def ref(path):
 return {"refPath":path}
def guard():
 assert scene("get_current_level",{})["returnValue"]==TARGET
 assert execute_tool("EditorToolset.EditorAppToolset.IsPIERunning","{}")["returnValue"] is False
 assert len(find("WL26_Road_","Rogue10m.WorldLayout20261112"))==663
def verify_source(row,hidden):
 a,c=ref(row["actor_path"]),ref(row["component_path"])
 ap=props(a,["bHidden","tags"])
 cp=props(c,["bVisible","staticMesh","overrideMaterials"])
 assert ap["bHidden"] is hidden and cp["bVisible"] is (not hidden)
 assert sorted(ap["tags"])==row["tags"]
 assert cp["staticMesh"]["refPath"]=="/Engine/BasicShapes/Cube.Cube"
 xf=actor("get_actor_transform",{"actor":a})["returnValue"]
 for field,expected,keys in (("location",row["location_cm"],("x","y","z")),("scale",row["scale"],("x","y","z")),("rotation",row["rotation_pitch_yaw_roll"],("pitch","yaw","roll"))):
  assert max(abs(xf[field][k]-v) for k,v in zip(keys,expected))<0.01
 return {"label":row["label"],"actor":a,"component":c,"properties":cp}
'''

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("stage",choices=("probe","prototype","extend","save"))
    args=parser.parse_args()
    plan=json.loads(PLAN.read_text(encoding="utf-8-sig"))
    assert plan["status"]=="read_only_preflight_passed_prototype_not_authored" and plan["target"]==TARGET
    rows=plan["source_roads"]
    selected=[r for r in plan["decals"] if (r["edge_id"] in ("E04","E11")) == (args.stage!="extend")]
    sources=[r for r in rows if (r["label"].split("_")[2] in ("E04","E11")) == (args.stage!="extend")]
    # Ground samples are evidence, not tool inputs. Never embed the 5k samples
    # and native inventory payload in a scripting request.
    slim={"material_path":plan["material_path"],"source_roads":rows if args.stage=="save" else []}
    constants="\nTARGET="+repr(TARGET)+"\nPLAN="+repr(slim)+"\nSELECTED="+repr(selected)+"\nSOURCES="+repr(sources[:1] if args.stage=="probe" else sources)+"\n"
    if args.stage=="probe":
        body='''
def run():
 guard()
 assert len(find("WL26_RoadSurface20261117_"))==0
 return {"status":"read_only_source_probe","sample":verify_source(SOURCES[0],False),"current_level":TARGET,"source_count":663}
'''
    elif args.stage in ("prototype","extend"):
        count=0 if args.stage=="prototype" else 2
        body='''
def run():
 guard()
 assert len(find("WL26_RoadSurface20261117_"))==EXPECTED
 for row in SOURCES:
  verify_source(row,False)
 material=ref(PLAN["material_path"]+".M_RoadSurface")
 mp=props(material,["materialDomain","blendMode"])
 assert mp["materialDomain"]=="MD_DeferredDecal" and mp["blendMode"]=="BLEND_Translucent"
 created=[]
 for row in SELECTED:
  xform={"location":dict(zip(("x","y","z"),row["center_cm"])),"rotation":row["rotation"],"scale":{"x":1,"y":1,"z":1}}
  a=scene("add_to_scene_from_class",{"actor_type":ref("/Script/Engine.DecalActor"),"name":row["label"],"xform":xform,"snap_to_ground":False})["returnValue"]
  assert actor("set_label",{"actor":a,"label":row["label"]})["returnValue"] is True
  setprops(a,{"tags":["Rogue10m.WorldLayout20261112","Rogue10m.LandscapeRoadSurface20261117"]})
  scene("set_actor_folder",{"actor":a,"folder_path":"RoadSurface20261117"})
  c=actor("get_root_component",{"actor":a})["returnValue"]
  size=dict(zip(("x","y","z"),row["local_half_extents_cm"]))
  setprops(c,{"decalMaterial":material,"decalSize":size,"fadeScreenSize":0.0})
  read=props(c,["decalMaterial","decalSize","fadeScreenSize","bVisible"])
  assert read["decalMaterial"]["refPath"]==material["refPath"] and read["fadeScreenSize"]==0 and read["bVisible"] is True
  assert max(abs(read["decalSize"][k]-size[k]) for k in ("x","y","z"))<0.01
  created.append({"label":row["label"],"actor":a,"component":c,"readback":read})
 hidden=[]
 for row in SOURCES:
  setprops(ref(row["component_path"]),{"bVisible":False})
  setprops(ref(row["actor_path"]),{"bHidden":True})
  verify_source(row,True)
  hidden.append(row["label"])
 return {"status":"authored_unsaved_requires_review","created":created,"hidden_old_roads":hidden,"terrain_mutated":False,"collision_changed":False,"packages_saved":[]}
'''.replace("EXPECTED",str(count))
    else:
        body='''
def run():
 guard()
 projectors=find("WL26_RoadSurface20261117_","Rogue10m.LandscapeRoadSurface20261117")
 assert len(projectors)==29
 for row in PLAN["source_roads"]:
  verify_source(row,True)
 saved=[]
 for a in projectors+[ref(r["actor_path"]) for r in PLAN["source_roads"]]:
  scene("save_actor",{"actor":a})
  saved.append(a)
 assert tool("asset.AssetTools","save_assets",{"asset_paths":[PLAN["material_path"],TARGET]})["returnValue"] is True
 return {"status":"saved_candidate_only","saved_actors":saved,"saved_assets":[PLAN["material_path"],TARGET],"requires_independent_cold_validation":True}
'''
    script=HEADER+constants+body
    dest=OUT/("road-"+args.stage+"-compact.args.json")
    with dest.open("x",encoding="utf-8") as stream:
        json.dump({"toolset_name":"editor_toolset.toolsets.programmatic.ProgrammaticToolset","tool_name":"execute_tool_script","arguments":{"script":script}},stream,ensure_ascii=False,indent=2)
    print(dest)

if __name__=="__main__":
    main()
