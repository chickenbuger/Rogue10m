"""Recover the inspected 19-asset texture-proxy interruption without reimport.

Only the exact previously inspected source and partial scope are accepted. The
guide remains unchanged until ApplyRefinedArin is explicitly run afterwards.
"""
import importlib
import json
import sys
from pathlib import Path
import unreal

project = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(project / 'Scripts/Editor'))
import ApplyRefinedArin as application
application = importlib.reload(application)

data, blueprint, cdo, actor = application._guide_context()
before = application._guide_state(data, blueprint, cdo, actor)
baseline = json.loads(application.CHECKPOINT.read_text(encoding='utf-8-sig'))
if before != baseline:
    raise RuntimeError('Guide must still exactly match the untouched v1 checkpoint')
manifest, sources, hashes = application._surface_sources()
interruption = json.loads(application.STATE_FILE.read_text(encoding='utf-8-sig'))
if interruption.get('phase') != 'aborted_requires_inspection' or interruption.get('source_hashes') != hashes:
    raise RuntimeError('Recovery requires the recorded interruption and identical source hashes')
existing = application._scope_assets()
partial = {application.MESH_PATH, application.SKELETON_PATH, application.ANIMATION_PATH} | {
    source['asset'] for source in sources.values()}
if set(existing) != partial or len(existing) != 19:
    raise RuntimeError('Recovery accepts only the inspected 3-core/16-texture partial scope')
application._json(application.EVIDENCE / 'unreal-refined-first-import-interruption.json', interruption)
mesh = application._require(application.MESH_PATH, unreal.SkeletalMesh)
animation = application._require(application.ANIMATION_PATH, unreal.AnimSequence)
skeleton = application._require(application.SKELETON_PATH, unreal.Skeleton)
for asset in (mesh, animation):
    imported = Path(asset.get_editor_property('asset_import_data').get_first_filename()).resolve()
    if imported != application.SOURCE_FBX.resolve():
        raise RuntimeError('Core source import filename differs: ' + str(imported))
application._validate_core(mesh, animation, skeleton)
textures = {}
before_sizes = {}
for key, source in sources.items():
    texture = application._require(source['asset'], unreal.Texture2D)
    application._verify_provenance(texture, hashes)
    if unreal.EditorAssetLibrary.get_metadata_tag(texture, 'Arin.TextureSHA256') != source['sha256']:
        raise RuntimeError('Inspected texture bytes differ: ' + str(key))
    if Path(texture.get_editor_property('asset_import_data').get_first_filename()).resolve() != source['file'].resolve():
        raise RuntimeError('Texture import filename differs: ' + str(key))
    before_sizes[source['name']] = [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()]
    textures[key] = texture

state = {'mode': 'scoped_recovery_prepare_only', 'source_hashes': hashes}
try:
    application._stage(state, 'recover_texture_resources', inspected_scope=sorted(existing))
    for key, texture in textures.items():
        application._texture_settings(texture, key[1])
    application._finish_texture_compilation()
    texture_report = application._validate_textures(textures, sources, hashes)
    application._stage(state, 'create_only_missing_5_materials')
    parent, materials = application._create_materials(textures, hashes)
    application._slots(mesh, materials, assign=True)
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    old_vertices = subsystem.get_num_verts(mesh, 0)
    old_skeleton = mesh.get_editor_property('skeleton')
    if not subsystem.regenerate_lod(mesh, 3, False, False):
        raise RuntimeError('Owned skeletal LOD generation failed')
    lods = application._lod_metrics(mesh)
    if subsystem.get_num_verts(mesh, 0) != old_vertices or mesh.get_editor_property('skeleton') != old_skeleton:
        raise RuntimeError('Generated LODs changed imported LOD0/skeleton')
    saved = []
    for asset in [*textures.values(), parent, *materials.values(), skeleton, animation, mesh]:
        application._set_provenance(asset, hashes)
        saved.append(application._save(asset))
    if set(application._scope_assets()) != application._expected_paths(sources) or len(saved) != 24:
        raise RuntimeError('Final owned dependencies differ')
    if before != application._guide_state(data, blueprint, cdo, actor):
        raise RuntimeError('Guide changed during dependency-only recovery')
    report = {'status': 'prepared_only_guide_unchanged', 'source_hashes': hashes,
        'recovery': {'reused_imported_assets': 19, 'new_material_assets': 5,
            'reimport_or_overwrite': False, 'before_resource_sizes': before_sizes,
            'solution': 'single set_editor_properties/PostEditChange per texture plus official Editor.AsyncTextureCompilationFinishAll'},
        'core': application._validate_core(mesh, animation, skeleton),
        'textures': texture_report, 'materials': application._validate_materials(materials, parent, textures),
        'slots': application._slots(mesh, materials), 'lods': lods, 'saved_assets': saved}
    application._json(application.EVIDENCE / 'unreal-refined-prepared.json', report)
    application._stage(state, 'completed', guide_unchanged=True)
    unreal.log('ARIN_REFINED_DEPENDENCIES_RECOVERED_AND_PREPARED')
except Exception as exc:
    application._stage(state, 'aborted_requires_inspection', error=str(exc), observed_assets=sorted(application._scope_assets()))
    raise
