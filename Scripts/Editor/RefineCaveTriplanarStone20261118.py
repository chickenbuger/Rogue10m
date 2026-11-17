"""Explicit unsaved world-projected stone repair after the rejected UV candidate.

Import is inert. Only the owned M_CaveSurface graph and two existing Core wall
material bindings may change. Geometry, camera, lights, exposure and assets stay.
"""
from __future__ import annotations
import argparse
import copy
from datetime import datetime,timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import traceback

ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'Feature/doc/evidence/caves20261118'
SPEC=BASE/'triplanar-spec.json'
SPEC_SHA256='0d24c669a60f9c998401afcc7f7c75e74f6fe1f72c499496fda3139e5c47353f'
VISIBILITY_SHA256='ad6cd1efe5e64a29ba3d311df890d861819238379748f4be3d31379c886183f9'
WALL_RECEIPT_SHA256='f7f3c275c961b5a8fed6efcd6badbb9a9ce9f2486185044bd632d9bf3d5d19b4'
TARGET='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
OWN='/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117/Caves20261118'
SURFACE=OWN+'/M_CaveSurface'


class CaveTriplanarError(RuntimeError):
    def __init__(self,report):
        self.report=report;super().__init__(json.dumps(report,ensure_ascii=False))


def require(value,message):
    if not value:raise RuntimeError(message)


def module(name,relative):
    spec=importlib.util.spec_from_file_location(name,ROOT/relative)
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value


def path(value):return value.get_path_name() if value else None


def serialized(value):
    return (json.dumps(value,sort_keys=True,ensure_ascii=False,separators=(',',':'),allow_nan=False)+'\n').encode('utf-8')


def read(name,sha=None):
    raw=(BASE/name).read_bytes()
    if sha:require(hashlib.sha256(raw).hexdigest()==sha,'Immutable input changed: '+name)
    return json.loads(raw)


def node_program(texture):
    nodes=[];links=[]
    def n(key,kind,props=None):nodes.append({'key':key,'class':'MaterialExpression'+kind,'properties':props or {}})
    def link(a,b,pin='',output=''):links.append({'source':a,'output':output,'destination':b,'pin':pin})
    n('tile240','Constant',{'r':240.})
    planes={'xy':(True,True,False),'xz':(True,False,True),'yz':(False,True,True)}
    for plane,(r,g,b) in planes.items():
        n('uv_'+plane,'ComponentMask',{'r':r,'g':g,'b':b,'a':False})
        n('scaled_uv_'+plane,'Divide',{'const_a':0.,'const_b':1.})
        n('texture_'+plane,'TextureSample',{'texture':texture,'sampler_type':'SAMPLERTYPE_COLOR','sampler_source':'SSM_FROM_TEXTURE_ASSET'})
        link('existing_world_position','uv_'+plane);link('uv_'+plane,'scaled_uv_'+plane,'A')
        link('tile240','scaled_uv_'+plane,'B');link('scaled_uv_'+plane,'texture_'+plane,'UVs')
    n('abs_normal','Abs');link('existing_pixel_normal_ws','abs_normal')
    for axis,flags in {'x':(True,False,False),'y':(False,True,False),'z':(False,False,True)}.items():
        n('weight_'+axis,'ComponentMask',dict(zip(('r','g','b','a'),(*flags,False))))
        link('abs_normal','weight_'+axis)
    for key in ('weight_sum_xy','weight_sum_xyz'):
        n(key,'Add',{'const_a':0.,'const_b':0.})
    link('weight_x','weight_sum_xy','A');link('weight_y','weight_sum_xy','B')
    link('weight_sum_xy','weight_sum_xyz','A');link('weight_z','weight_sum_xyz','B')
    for axis,plane in (('x','yz'),('y','xz'),('z','xy')):
        n('normalized_'+axis,'Divide',{'const_a':0.,'const_b':1.})
        link('weight_'+axis,'normalized_'+axis,'A');link('weight_sum_xyz','normalized_'+axis,'B')
        n('weighted_'+plane,'Multiply',{'const_a':0.,'const_b':1.})
        link('texture_'+plane,'weighted_'+plane,'A','RGB');link('normalized_'+axis,'weighted_'+plane,'B')
    n('texture_sum_yz_xz','Add',{'const_a':0.,'const_b':0.})
    n('texture_sum_xyz','Add',{'const_a':0.,'const_b':0.})
    link('weighted_yz','texture_sum_yz_xz','A');link('weighted_xz','texture_sum_yz_xz','B')
    link('texture_sum_yz_xz','texture_sum_xyz','A');link('weighted_xy','texture_sum_xyz','B')
    n('tinted_stone','Multiply',{'const_a':0.,'const_b':1.})
    link('texture_sum_xyz','tinted_stone','A');link('existing_side_color','tinted_stone','B')
    require(len(nodes)==25 and len(links)==38,'Exact triplanar program size required')
    return nodes,links


def build_spec():
    visibility=read('author-live/visibility.json',VISIBILITY_SHA256);wall=read('author-live/wall-texture.json',WALL_RECEIPT_SHA256)
    require(visibility.get('success') and wall.get('success'),'Successful semantic prior stages required')
    mats=read('baseline-live/baseline-materials.json')['materials']
    terrain=[v for k,v in mats.items() if '/MI_Terrain' in k]
    require(len(terrain)==1,'Exact retained terrain source required')
    texture=terrain[0]['texture_values']['Stone Base Color'];nodes,links=node_program(texture)
    bindings=[{**b,'old_material':OWN+'/MI_CaveRock','new_material':OWN+'/MI_CaveStoneFloor'} for b in wall['binding_changes']]
    return {'schema':'Rogue10m.CaveTriplanarStoneSpec.20261118.v1','map':TARGET,
        'visibility_receipt_sha256':VISIBILITY_SHA256,'wall_v1_receipt_sha256':WALL_RECEIPT_SHA256,
        'prior_wall_candidate_visual_status':'rejected_cube_uv_atlas_mismatch','surface_material':SURFACE,
        'fixed_stone_texture':texture,'world_tile_cm':240.,'side_color_before':[.25,.22,.17,1.],
        'side_color_after':[.75,.72,.65,1.],'surface_nodes_before':18,'nodes_added':25,'surface_nodes_after':43,
        'nodes':nodes,'links':links,'existing_aliases':{'existing_world_position':'original sole WorldPosition',
            'existing_pixel_normal_ws':visibility['surface_slope_nodes']['pixel_normal_ws'],
            'existing_side_color':visibility['surface_slope_nodes']['color']},
        'existing_lerp':visibility['surface_slope_nodes']['lerp'],'output_to_existing_lerp_A':'tinted_stone',
        'binding_changes':bindings,'old13_nodes_all_preserved':True,'top_B_and_Alpha_unchanged':True,
        'new_assets':0,'new_actors':0,'new_components':0,'new_instances':0,
        'parameter_lists_parentage_unchanged':True,'actual_render_required':True}


def refine_cave_triplanar_stone(*,allow_editor_mutation=False,expected_spec_sha256,
                              expected_composite_blake3,expected_original_layer_blake3,expected_dirty_packages):
    report={'schema':'Rogue10m.CaveTriplanarStoneRefinement.20261118.v1','success':False,
        'started_utc':datetime.now(timezone.utc).isoformat(),'mutation_started':False,'assets_saved':False,'save_called':False,
        'created_assets':[],'created_actors':[],'created_components':[],'instance_changes':[],'binding_changes':[],
        'automatic_retry':False,'automatic_rollback':False,'partial_work_deleted':False}
    try:
        require(allow_editor_mutation is True and expected_spec_sha256==SPEC_SHA256,'Explicit invocation and reviewed spec required')
        spec=read('triplanar-spec.json',SPEC_SHA256);visibility_receipt=read('author-live/visibility.json',VISIBILITY_SHA256)
        read('author-live/wall-texture.json',WALL_RECEIPT_SHA256)
        require(spec['schema']=='Rogue10m.CaveTriplanarStoneSpec.20261118.v1' and spec['map']==TARGET,'Exact triplanar spec required')
        import unreal as u
        api=module('triplanar_component_api','Scripts/Editor/NaturalWorldDressing20261117.py')
        inspector=module('triplanar_inspector','Scripts/Editor/InspectCaveSites20261118.py')
        reader=module('triplanar_graph_reader','Scripts/Editor/NaturalRoadMaterial20261117.py')
        actor_reader=module('triplanar_actor_reader','Scripts/Editor/AuthorNaturalBossThreshold20261118.py')
        visibility=module('triplanar_visibility_state','Scripts/Editor/RefineCaveVisibility20261118.py')
        walls=module('triplanar_effect_state','Scripts/Editor/RefineCaveRockWalls20261118.py')
        api._guard(u)
        require(len(expected_dirty_packages)==len(set(expected_dirty_packages)) and set(api._dirty(u))==set(expected_dirty_packages),'Exact dirty set required')
        native=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(native.get('success') and native.get('landscape_reports_up_to_date') and
                native['composite_blake3']==expected_composite_blake3 and native['original_layer_blake3']==expected_original_layer_blake3,'Settled terrain hashes differ')
        subsystem=u.get_editor_subsystem(u.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors())
        require(len(actors)==1408,'Exact1408 scene required')
        before=actor_reader._snapshot_actors(u,actors,api);before_components=inspector.persistent_components(u,actors)
        baseline=read('baseline-live/baseline-actors.json')
        require({p:before[p] for p in baseline['actors']}==baseline['actors'] and
                {p:before_components[p] for p in baseline['actors']}==baseline['persistent_components'],'Original1404 state changed')
        effects_before=walls.effect_state(u,actors,visibility,inspector,api)
        material_before={p:visibility.material_state(u,u.load_asset(p),reader) for p in visibility_receipt['expected_material_states_after']}
        require(material_before==visibility_receipt['expected_material_states_after'],'Current18-node material states differ')
        source_baseline=read('baseline-live/baseline-materials.json')['materials']
        sources_before={p:inspector.material_state(u,u.load_asset(p),reader) for p in source_baseline}
        require(sources_before==source_baseline and len(sources_before)==40,'Source40 material states changed')
        lib=u.MaterialEditingLibrary;surface=u.load_asset(SURFACE);surface_key=path(surface)
        require(isinstance(surface,u.Material),'Existing owned surface Material required')
        graph_before=reader._fingerprint(u,lib,surface)
        require(graph_before==visibility_receipt['surface_graph_after'] and len(graph_before['nodes'])==18,'Exact successful visibility graph required')
        old_nodes={n.get_name():n for n in lib.get_material_expressions(surface)}
        worlds=[n for n in old_nodes.values() if isinstance(n,u.MaterialExpressionWorldPosition)]
        require(len(worlds)==1,'One existing WorldPosition required')
        aliases={'existing_world_position':worlds[0],
                 'existing_pixel_normal_ws':old_nodes[spec['existing_aliases']['existing_pixel_normal_ws']],
                 'existing_side_color':old_nodes[spec['existing_aliases']['existing_side_color']]}
        lerp=old_nodes[spec['existing_lerp']];color=aliases['existing_side_color']
        actual_color=[float(getattr(color.get_editor_property('constant'),a)) for a in 'rgba']
        require(max(abs(a-b) for a,b in zip(actual_color,spec['side_color_before']))<1e-6,'Previous side color differs')
        texture=u.load_asset(spec['fixed_stone_texture'])
        require(isinstance(texture,u.Texture2D) and texture.get_editor_property('srgb') is True,'Existing sRGB stone color texture required')
        require(texture.get_editor_property('address_x')==u.TextureAddress.TA_WRAP and
                texture.get_editor_property('address_y')==u.TextureAddress.TA_WRAP,'Source stone color must already wrap in both UV axes')
        require(all(getattr(u,n['class'],None) is not None for n in spec['nodes']) and
                hasattr(u.MaterialSamplerType,'SAMPLERTYPE_COLOR') and hasattr(u.SamplerSourceMode,'SSM_FROM_TEXTURE_ASSET'),
                'Public triplanar expression/sampler bindings unavailable')
        old_material=u.load_asset(OWN+'/MI_CaveRock');new_material=u.load_asset(OWN+'/MI_CaveStoneFloor')
        components={path(c):c for a in actors for c in a.get_components_by_class(u.InstancedStaticMeshComponent)}
        target_components=[]
        for binding in spec['binding_changes']:
            c=components.get(binding['component_path'])
            require(c is not None and path(c.get_owner())==binding['actor_path'] and c.get_instance_count()==binding['instance_count']
                    and c.get_num_materials()==1 and c.get_material(0)==old_material,'Expected v1 rock-material bindings differ')
            target_components.append((binding,c))
        structures=read('author-live/structures.json');checked=0
        for site in structures['sites']:
            for rec in site['components']:
                c=components[rec['path']];require(c.get_instance_count()==len(rec['instances']),'Structure count changed')
                for row in rec['instances']:
                    api._same_transform(row['observed_world_transform'],api._instance_transform(u,c,row['index']));checked+=1
        require(checked==286 and set(api._dirty(u))==set(expected_dirty_packages),'Transforms or preflight dirty set changed')
        report['material_states_before']=material_before;report['surface_graph_before']=graph_before;report['side_color_before']=actual_color
        with u.ScopedEditorTransaction('World-projected cave stone instead of boulder UV atlas 20261118'):
            report['mutation_started']=True;surface.modify();nodes={}
            for i,description in enumerate(spec['nodes']):
                cls=getattr(u,description['class']);n=lib.create_material_expression(surface,cls,1100+(i%6)*180,1000+(i//6)*180)
                require(n is not None,'Triplanar expression creation failed: '+description['key']);nodes[description['key']]=n
                for prop,value in description['properties'].items():
                    if prop=='texture':value=texture
                    elif prop=='sampler_type':value=u.MaterialSamplerType.SAMPLERTYPE_COLOR
                    elif prop=='sampler_source':value=u.SamplerSourceMode.SSM_FROM_TEXTURE_ASSET
                    n.set_editor_property(prop,value)
            all_nodes={**aliases,**nodes}
            for link in spec['links']:
                reader._connect(lib,surface,all_nodes[link['source']],all_nodes[link['destination']],link['pin'],link['output'])
            color.set_editor_property('constant',u.LinearColor(r=.75,g=.72,b=.65,a=1.))
            reader._connect(lib,surface,nodes['tinted_stone'],lerp,'A','')
            lib.recompile_material(surface)
            for binding,c in target_components:
                c.get_owner().modify();c.modify();c.set_material(0,new_material)
                require(c.get_material(0)==new_material,'Triplanar wall material assignment failed')
                report['binding_changes'].append({**binding,'observed_material':path(c.get_material(0))})
            report['triplanar_nodes']={k:v.get_name() for k,v in nodes.items()}
            report['triplanar_node_records']=[]
            for description in spec['nodes']:
                n=nodes[description['key']];observed={}
                for prop in description['properties']:
                    value=n.get_editor_property(prop)
                    observed[prop]=path(value) if prop=='texture' else str(value) if prop in ('sampler_type','sampler_source') else value
                report['triplanar_node_records'].append({'key':description['key'],'name':n.get_name(),'class':n.get_class().get_name(),
                    'properties':observed,'reader_properties':reader._properties(n),'inputs':reader._inputs(lib,surface,n)})
        graph_after=reader._fingerprint(u,lib,surface)
        require(len(graph_after['nodes'])==43 and len(set(graph_after['nodes'])-set(graph_before['nodes']))==25,'Exact43-node surface required')
        original13=visibility_receipt['surface_graph_before']['nodes']
        require(all(graph_after['nodes'][k]==v for k,v in original13.items()),'Original13 nodes changed')
        for name,value in graph_before['nodes'].items():
            if name==spec['existing_lerp']:
                normalized=copy.deepcopy(graph_after['nodes'][name])
                normalized['inputs']=[next(x for x in value['inputs'] if x['pin']=='A') if p['pin']=='A' else p for p in normalized['inputs']]
                require(normalized==value,'Existing visibility Lerp changed beyond A')
            else:require(graph_after['nodes'][name]==value,'Existing visibility node changed')
        require(graph_after['outputs']==graph_before['outputs'],'A material output changed')
        material_after={p:visibility.material_state(u,u.load_asset(p),reader) for p in material_before}
        for p,value in material_before.items():
            current=material_after[p]
            require({k:v for k,v in value.items() if k!='graph'}=={k:v for k,v in current.items() if k!='graph'},'Material parameters/parentage changed')
            require(current['graph']==(graph_after if value['base_material']==surface_key else value['graph']),'Unexpected material graph delta')
        after_actors=list(subsystem.get_all_level_actors());after=actor_reader._snapshot_actors(u,after_actors,api)
        normalized=copy.deepcopy(after);target_paths={b['component_path'] for b,_ in target_components}
        for actor_path,value in normalized.items():
            for primitive in value['primitives']:
                if primitive['path'] in target_paths:primitive['materials']=next(x['materials'] for x in before[actor_path]['primitives'] if x['path']==primitive['path'])
        require(normalized==before and inspector.persistent_components(u,after_actors)==before_components,'Geometry/collision/other material refs changed')
        require(walls.effect_state(u,after_actors,visibility,inspector,api)==effects_before,'Lighting/camera/exposure changed')
        require({p:inspector.material_state(u,u.load_asset(p),reader) for p in sources_before}==sources_before,'A source40 material changed')
        final_native=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(final_native.get('success') and final_native['composite_blake3']==expected_composite_blake3 and
                final_native['original_layer_blake3']==expected_original_layer_blake3,'Terrain changed')
        require(set(api._dirty(u))<=set(expected_dirty_packages)|{SURFACE}|{b['package'] for b,_ in target_components},'Unexpected dirty package')
        report.update(success=True,spec_sha256=SPEC_SHA256,visibility_receipt_sha256=VISIBILITY_SHA256,
            wall_v1_receipt_sha256=WALL_RECEIPT_SHA256,surface_graph_after=graph_after,expected_material_states_after=material_after,
            side_color_after=[float(getattr(color.get_editor_property('constant'),a)) for a in 'rgba'],
            fixed_stone_texture=path(texture),fixed_stone_texture_srgb=bool(texture.get_editor_property('srgb')),
            original13_nodes_preserved=True,existing_visibility_B_and_Alpha_preserved=True,source40_materials_preserved=True,
            original1404_preserved=True,all286_transforms_and_all_geometry_collision_preserved=True,
            all_lighting_camera_exposure_preserved=True,all_seven_material_non_graph_state_preserved=True,
            dirty_before=list(expected_dirty_packages),dirty_after=api._dirty(u),finished_utc=datetime.now(timezone.utc).isoformat(),
            validation_pending=['actual world-projected stone appearance','strict graph and material-binding QA','runtime camera','bounded save and cold reload'])
        return report
    except Exception:
        report['traceback']=traceback.format_exc();raise CaveTriplanarError(report) from None


def main():
    parser=argparse.ArgumentParser(description='Pure triplanar spec generation only.');parser.add_argument('--write-spec',action='store_true')
    args=parser.parse_args();require(args.write_spec,'Explicit --write-spec required');value=build_spec()
    with SPEC.open('xb') as stream:stream.write(serialized(value))
    print(json.dumps({'spec_sha256':hashlib.sha256(serialized(value)).hexdigest(),'nodes_added':25,'nodes_after':43,'links':38,
                      'texture':value['fixed_stone_texture'],'material_bindings_changed':2},indent=2))


if __name__=='__main__':main()
