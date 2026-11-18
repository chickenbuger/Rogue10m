"""Explicit bounded overlay: two continuous shells and preserved ramp visuals."""
from pathlib import Path
import hashlib
import importlib.util
import json
import os
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/cave-organic20261118';OUT=BASE/'author-live'
assert os.getpid()==49456 and not (OUT/'scene.json').exists()
def load(name,file):
    s=importlib.util.spec_from_file_location(name,ROOT/file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
api=load('organic_scene_api','Scripts/Editor/NaturalWorldDressing20261117.py')
qa=load('organic_scene_qa','Scripts/Editor/OrganicCaves20261118QA.py')
config=json.loads((BASE/'validation-config.json').read_text(encoding='utf-8'))
spec=json.loads((BASE/'geometryspec-v2.json').read_text(encoding='utf-8'))
assert hashlib.sha256((BASE/'geometryspec-v2.json').read_bytes()).hexdigest()==config['geometry_spec_sha256']
mesh_receipt=json.loads((OUT/'meshes.json').read_text(encoding='utf-8'));assert mesh_receipt['passed']
assert set(api._dirty(u))==set(mesh_receipt['dirty_packages_after'])
assert u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_path_name().split('.')[0]==qa.TARGET
assert not u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
actors=list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors());assert len(actors)==1408
labels={a.get_actor_label():a for a in actors}
plan=qa.frozen_contract()['plan'];sds=u.get_engine_subsystem(u.SubobjectDataSubsystem);lib=u.SubobjectDataBlueprintFunctionLibrary
receipt={'passed':False,'new_component_paths':{},'retired_components':[],'hidden_floor_components':[],
         'actors_before':1408,'assets_saved':False,'landscape_modified':False}
with u.ScopedEditorTransaction('Organic cave continuous walls and ceilings'):
    for site in spec['sites']:
        sid=site['site_id'];structure=labels['CV18_'+sid+'_Structure'];dressing=labels['CV18_'+sid+'_Dressing']
        structure.modify();dressing.modify()
        existing={c.get_name():c for a in [structure,dressing] for c in a.get_components_by_class(u.PrimitiveComponent)}
        for name in ['CV18_'+sid+'_ISM_01',sid+'_CaveDressingISM_00']:
            c=existing[name];c.modify();c.set_visibility(False,False);c.set_hidden_in_game(True,False);c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
            receipt['retired_components'].append(c.get_path_name())
        floor=existing['CV18_'+sid+'_ISM_00'];collision_before=floor.get_collision_enabled()
        floor.modify();floor.set_visibility(False,False);floor.set_hidden_in_game(True,False)
        assert floor.get_collision_enabled()==collision_before
        receipt['hidden_floor_components'].append(floor.get_path_name())
        root=dressing.get_editor_property('root_component')
        handles=list(sds.k2_gather_subobject_data_for_instance(dressing))
        matches=[h for h in handles if lib.get_associated_object(lib.get_data(h))==root]
        assert len(matches)==1
        _,shell=api._new_component(u,sds,matches[0],u.StaticMeshComponent,'Organic_'+sid+'_Shell')
        shell.set_mobility(u.ComponentMobility.STATIC);shell.set_component_tick_enabled(False)
        shell.set_static_mesh(u.load_asset(config['expected_mesh_paths'][sid]))
        shell.set_world_transform(u.Transform(location=u.Vector(*site['world_pivot_cm'])),False,True)
        shell.set_material(0,u.load_asset(config['expected_material_path']));shell.set_material(1,u.load_asset(config['expected_floor_material_path']))
        shell.set_editor_property('generate_overlap_events',False);shell.set_editor_property('cast_shadow',True)
        shell.set_collision_profile_name('BlockAll',False);shell.set_collision_enabled(u.CollisionEnabled.QUERY_AND_PHYSICS)
        shell.set_visibility(True,False);shell.set_hidden_in_game(False,False)
        original=[s for s in plan['sites'] if s['site_id']==sid][0]
        ramp_plan=[r for r in original['components'] if r['label']==sid+'_Ramp'][0]
        _,ramp=api._new_component(u,sds,matches[0],u.StaticMeshComponent,'Organic_'+sid+'_RampVisual')
        ramp.set_mobility(u.ComponentMobility.STATIC);ramp.set_component_tick_enabled(False)
        ramp.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
        ramp.set_material(0,u.load_asset(config['expected_floor_material_path']))
        p,y,r=ramp_plan['rotation_pitch_yaw_roll_deg']
        ramp.set_world_transform(u.Transform(location=u.Vector(*ramp_plan['location_cm']),
            rotation=u.Rotator(pitch=p,yaw=y,roll=r),scale=u.Vector(*[n/100 for n in ramp_plan['dimensions_cm']])),False,True)
        ramp.set_collision_enabled(u.CollisionEnabled.NO_COLLISION);ramp.set_editor_property('can_ever_affect_navigation',False)
        ramp.set_editor_property('generate_overlap_events',False);ramp.set_visibility(True,False);ramp.set_hidden_in_game(False,False)
        receipt['new_component_paths'][sid]={'shell':shell.get_path_name(),'ramp_visual':ramp.get_path_name()}
receipt['dirty_packages_after']=api._dirty(u)
receipt['passed']=len(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())==1408
with (OUT/'scene.json').open('x',encoding='utf-8',newline='\n')as f:json.dump(receipt,f,indent=2)
assert receipt['passed'] and len(receipt['dirty_packages_after'])==8
print('ORGANIC_SCENE_FOUR_COMPONENT_OVERLAY_UNSAVED')
