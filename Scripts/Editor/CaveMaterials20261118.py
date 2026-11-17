"""Create seven cave-owned derivatives, never save or edit a source material.

Import is inert. Explicit owner invocation in the idle, fully-loaded Crop world.
Source graphs/parameters/files and the old actor state are compared before and
after; a partial failure is retained and must be inspected, never retried.
"""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import traceback

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'Feature/doc/evidence/caves20261118'
TARGET = '/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
OLD = '/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117'
OWN = OLD + '/Caves20261118'
GROUND = '/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_GroundDetail'
GRASS = '/Game/Rogue10m/World/StartVillage/Materials/MI_StartVillage_Grass'
ROCK = '/Game/Stylized_Village/Assets/Stones/Boulder/_Materials/MI_Boulder_Med'
LAMP = OLD + '/M_Natural_Banner_Ochre'
NAMES = ('M_CaveSurface', 'MI_CaveGrass', 'MI_CaveStoneFloor', 'MI_CaveRock',
         'M_CaveLamp', 'MI_CaveLampWarm', 'MI_CaveLampCool')


def require(value, message):
    if not value:
        raise RuntimeError(message)


def module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def create_cave_materials(*, allow_editor_mutation=False, resume_diagnosed_partial=False):
    import unreal as u
    reader = module('cave_material_graph_reader', 'Scripts/Editor/NaturalRoadMaterial20261117.py')
    inspection = module('cave_material_inspector', 'Scripts/Editor/InspectCaveSites20261118.py')
    dressing = module('cave_material_actor_reader', 'Scripts/Editor/NaturalWorldDressing20261117.py')
    actors_reader = module('cave_material_snapshot', 'Scripts/Editor/AuthorNaturalBossThreshold20261118.py')
    report = {'schema': 'Rogue10m.CaveMaterials.20261118.v1', 'success': False,
              'created_assets': [], 'assets_saved': False, 'automatic_retry': False,
              'utc': datetime.now(timezone.utc).isoformat()}
    try:
        require(allow_editor_mutation is True, 'Explicit owner invocation required')
        checkpoint = json.loads((BASE/'baseline-files.json').read_text(encoding='utf-8-sig'))
        require(checkpoint.get('status') == 'EXACT1481_PHYSICAL_BACKUP_SHA256_VERIFIED_SCOPED_INPUTS_UNCHANGED'
                and checkpoint.get('backed_up_file_count') == 1481 and checkpoint.get('candidate_file_count') == 1473
                and checkpoint.get('owned_file_count') == 8 and checkpoint.get('no_reparse_paths_verified') is True
                and checkpoint.get('built_native_sources_match_successful_build') is True,
                'Verified exact1481 physical checkpoint required before material creation')
        reader._guard(u)
        initial_dirty=reader._dirty(u)
        partial_names={'M_CaveSurface','MI_CaveGrass'} if resume_diagnosed_partial else set()
        if resume_diagnosed_partial:
            failure=json.loads((BASE/'author-live/material-failure.json').read_text())
            observed=json.loads((BASE/'partial-material-inspection.json').read_text())
            require(failure['created_assets']==[OWN+'/M_CaveSurface',OWN+'/MI_CaveGrass']
                    and 'Texture setter BaseColorTexture' in failure['traceback']
                    and observed['source_materials_unchanged'] is True
                    and observed['actor_count']==1404
                    and set(initial_dirty)=={OWN+'/'+name for name in partial_names},
                    'Only the diagnosed two-asset partial state may resume')
            report['resumed_existing_assets']=list(failure['created_assets'])
            report['setter_binding_diagnosis']='UE5.8 MaterialEditingLibrary scalar/vector/texture setters always return false; verify getters instead.'
        else:
            require(not initial_dirty,'Clean candidate required before material creation')
        native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        baseline_native = json.loads((BASE/'baseline-live/baseline-native.json').read_text())
        require(native.get('success') and native['composite_blake3'] == baseline_native['composite_blake3']
                and native['original_layer_blake3'] == baseline_native['original_layer_blake3'], 'Baseline terrain changed')
        subsystem = u.get_editor_subsystem(u.EditorActorSubsystem)
        old_actors = list(subsystem.get_all_level_actors())
        require(len(old_actors) == 1404, 'Original1404 actors required')
        snapshots = actors_reader._snapshot_actors(u, old_actors, dressing)
        components = inspection.persistent_components(u, old_actors)
        baseline_mats = json.loads((BASE/'baseline-live/baseline-materials.json').read_text())
        sources = {p: u.load_asset(p) for p in (GROUND, GRASS, ROCK, LAMP)}
        source_states = {p: inspection.material_state(u, a, reader) for p, a in sources.items()}
        for p, value in source_states.items():
            baseline = baseline_mats['materials'].get(p) or baseline_mats['materials'].get(p+'.'+p.rsplit('/',1)[1])
            require(value == baseline, 'Source material differs from inspected baseline: '+p)
        require(source_states[ROCK]['used_with_instanced_static_meshes'] is True,
                'Reuse only the already-instancing-compatible rock parent')
        lib = u.MaterialEditingLibrary
        for name in ('set_material_instance_parent', 'set_material_instance_scalar_parameter_value',
                     'set_material_instance_vector_parameter_value', 'set_material_instance_texture_parameter_value',
                     'get_material_instance_scalar_parameter_value', 'get_material_instance_vector_parameter_value',
                     'get_material_instance_texture_parameter_value', 'update_material_instance',
                     'create_material_expression', 'connect_material_expressions', 'connect_material_property',
                     'recompile_material'):
            require(callable(getattr(lib, name, None)), 'Unavailable public material binding: '+name)
        for name in NAMES:
            dest = OWN+'/'+name
            if name in partial_names:
                require(u.load_asset(dest) is not None and not (ROOT/'Content'/Path(dest[6:]+'.uasset')).exists(),
                        'Diagnosed derivative must remain unsaved in memory')
                continue
            require(not u.EditorAssetLibrary.does_asset_exist(dest)
                    and not (ROOT/'Content'/Path(dest[6:]+'.uasset')).exists()
                    and u.find_object(None, dest+'.'+name) is None, 'Destination already exists: '+dest)

        def duplicate(source, name):
            obj = u.load_asset(OWN+'/'+name) if name in partial_names else u.EditorAssetLibrary.duplicate_asset(source, OWN+'/'+name)
            require(obj is not None, 'Duplicate failed: '+name)
            report['created_assets'].append(OWN+'/'+name)
            return obj

        def instance(source, name, parent):
            obj = duplicate(source, name)
            require(isinstance(obj, u.MaterialInstanceConstant), 'Expected MaterialInstanceConstant')
            if name not in partial_names:lib.set_material_instance_parent(obj, parent)
            require(obj.get_editor_property('parent') == parent, 'Parent assignment failed')
            return obj

        def scalar(obj, name, value):
            result=None
            if abs(lib.get_material_instance_scalar_parameter_value(obj,name)-value)>=1e-4:
                result=lib.set_material_instance_scalar_parameter_value(obj,name,value)
            require(abs(lib.get_material_instance_scalar_parameter_value(obj, name)-value)<1e-4, 'Scalar verification '+name)
            report.setdefault('setter_receipts',[]).append({'material':obj.get_path_name(),'parameter':name,'binding_return':result,'getter_verified':True})

        def vector(obj, name, value):
            actual = lib.get_material_instance_vector_parameter_value(obj, name)
            result=None
            if any(abs(float(getattr(actual,k))-v)>=1e-4 for k,v in zip('rgba',value)):
                result=lib.set_material_instance_vector_parameter_value(obj,name,u.LinearColor(*value))
                actual=lib.get_material_instance_vector_parameter_value(obj,name)
            require(all(abs(float(getattr(actual,k))-v)<1e-4 for k,v in zip('rgba',value)), 'Color verification '+name)
            report.setdefault('setter_receipts',[]).append({'material':obj.get_path_name(),'parameter':name,'binding_return':result,'getter_verified':True})

        def texture(obj, name, package):
            tex = u.load_asset(package)
            require(isinstance(tex, u.Texture), 'Missing existing texture '+package)
            result=None
            if lib.get_material_instance_texture_parameter_value(obj,name)!=tex:
                result=lib.set_material_instance_texture_parameter_value(obj,name,tex)
            require(lib.get_material_instance_texture_parameter_value(obj, name) == tex, 'Texture verification '+name)
            report.setdefault('setter_receipts',[]).append({'material':obj.get_path_name(),'parameter':name,'binding_return':result,'getter_verified':True})

        def unsaved_state(obj):
            base = obj
            parents = []
            while isinstance(base,u.MaterialInstance):
                base = base.get_editor_property('parent')
                require(base is not None and base.get_path_name() not in parents,'Material parent cycle')
                parents.append(base.get_path_name())
            require(isinstance(base,u.Material),'Concrete base required')
            value = {'path':obj.get_path_name(),'parent_chain':parents,
                     'base_material':base.get_path_name(),'unsaved':True,
                     'used_with_instanced_static_meshes':bool(base.get_editor_property('used_with_instanced_static_meshes')),
                     'graph':reader._fingerprint(u,lib,base)}
            if isinstance(obj,u.MaterialInstance):
                value['scalar_values']={str(k):lib.get_material_instance_scalar_parameter_value(obj,k)
                    for k in lib.get_scalar_parameter_names(base)}
                value['vector_values']={str(k):[float(getattr(lib.get_material_instance_vector_parameter_value(obj,k),a)) for a in 'rgba']
                    for k in lib.get_vector_parameter_names(base)}
                value['texture_values']={str(k):(lib.get_material_instance_texture_parameter_value(obj,k).get_path_name()
                    if lib.get_material_instance_texture_parameter_value(obj,k) else None)
                    for k in lib.get_texture_parameter_names(base)}
            return value

        surface = duplicate(GROUND, 'M_CaveSurface')
        if resume_diagnosed_partial:
            require(surface.get_editor_property('used_with_instanced_static_meshes') is True,'Partial surface flag differs')
        else:
            surface.set_editor_property('used_with_instanced_static_meshes', True)
            lib.recompile_material(surface)
        terrain = next(v for k,v in baseline_mats['materials'].items() if '/MI_Terrain' in k)
        grass = instance(GRASS, 'MI_CaveGrass', surface)
        texture(grass, 'BaseColorTexture', terrain['texture_values']['Grass Base Color'])
        texture(grass, 'NormalTexture', terrain['texture_values']['Grass Normal'])
        scalar(grass, 'TileSizeCm', 436.0)
        vector(grass, 'ColorTint', (1.15,1.15,0.60,1.0))
        scalar(grass, 'Roughness', .9)
        lib.update_material_instance(grass)
        floor = instance(GRASS, 'MI_CaveStoneFloor', surface)
        texture(floor, 'BaseColorTexture', terrain['texture_values']['Stone Base Color'])
        texture(floor, 'NormalTexture', terrain['texture_values']['Stone Normal'])
        scalar(floor, 'TileSizeCm', 240.0)
        vector(floor, 'ColorTint', (.75,.72,.65,1.0))
        scalar(floor, 'Roughness', .92)
        lib.update_material_instance(floor)
        rock = duplicate(ROCK, 'MI_CaveRock')
        lamp = duplicate(LAMP, 'M_CaveLamp')
        colors = [n for n in lib.get_material_expressions(lamp) if isinstance(n, u.MaterialExpressionVectorParameter)]
        require(len(colors)==1 and str(colors[0].get_editor_property('parameter_name'))=='Color', 'Exact2-node lamp source required')
        intensity = lib.create_material_expression(lamp, u.MaterialExpressionScalarParameter, -220,180)
        intensity.set_editor_property('parameter_name','Glow')
        intensity.set_editor_property('default_value',5.0)
        multiply = lib.create_material_expression(lamp,u.MaterialExpressionMultiply,0,180)
        require(lib.connect_material_expressions(colors[0],'RGB',multiply,'A')
                and lib.connect_material_expressions(intensity,'',multiply,'B')
                and lib.connect_material_property(multiply,'',u.MaterialProperty.MP_EMISSIVE_COLOR), 'Lamp graph connection failed')
        lamp.set_editor_property('used_with_instanced_static_meshes',True)
        lib.recompile_material(lamp)
        lamps = []
        for name, color in [('MI_CaveLampWarm',(1.0,.40,.09,1.0)),('MI_CaveLampCool',(.16,.58,1.0,1.0))]:
            obj = instance(GRASS, name, lamp)
            vector(obj,'Color',color)
            scalar(obj,'Glow',5.0)
            lib.update_material_instance(obj)
            lamps.append(obj)

        for p, source in sources.items():
            require(inspection.material_state(u,source,reader)==source_states[p], 'Source changed during derivative authoring: '+p)
        current = list(subsystem.get_all_level_actors())
        require(len(current)==1404 and actors_reader._snapshot_actors(u,current,dressing)==snapshots
                and inspection.persistent_components(u,current)==components, 'Existing actor/component changed')
        after = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
        require(after['composite_blake3']==native['composite_blake3'] and after['original_layer_blake3']==native['original_layer_blake3'], 'Materials changed terrain')
        require(set(reader._dirty(u))==set(report['created_assets']), 'Unexpected dirty packages during material authoring')
        report.update(success=True, dirty=reader._dirty(u), source_materials_preserved=True,
                      original_actor_state_preserved=True, terrain_unchanged=True,
                      material_paths_by_role={'stone_floor':OWN+'/MI_CaveStoneFloor','stone_wall':OLD+'/M_Natural_WarmStone',
                          'grass_roof':OWN+'/MI_CaveGrass','rock':OWN+'/MI_CaveRock','timber':OLD+'/M_Natural_DarkTimber',
                          'metal':OLD+'/M_Natural_WarmStone','lamp_emission':OWN+'/MI_CaveLampWarm',
                          'lamp_emission_Q':OWN+'/MI_CaveLampWarm','lamp_emission_Z':OWN+'/MI_CaveLampCool'},
                      created_material_states={obj.get_path_name():unsaved_state(obj)
                          for obj in [surface,grass,floor,rock,lamp,*lamps]})
        return report
    except Exception:
        report.update(traceback=traceback.format_exc(),dirty=reader._dirty(u))
        raise CaveMaterialError(report) from None


class CaveMaterialError(RuntimeError):
    def __init__(self, report):
        self.report=report
        super().__init__(json.dumps(report,ensure_ascii=False))
