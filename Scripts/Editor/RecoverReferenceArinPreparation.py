"""Prepare exactly nine v3 dependencies from the inspected seven-asset stop.

Never reimport, overwrite, delete, or change the guide. Only the frozen source
hashes and the inspected 3-core/4-texture partial scope are accepted. Root owns
invocation after inspecting this script and the current live editor state.
"""
from datetime import datetime, timezone
import importlib
import json
from pathlib import Path
import sys
import uuid

import unreal


FROZEN_HASHES = {
    "Arin.RefinedFBXSHA256": "45296f9fcbd0d182406c85d29f8627409e090dc0c97ec97815a8035221448f00",
    "Arin.SurfaceManifestSHA256": "eb37013d9fd18113e1dffbe7668d1f66fb38da73f1269438b0f103c18d2d386f",
    "Arin.TextureBundleSHA256": "49520bb03ebc3068fe6b9ed57c56eb86885b3ed17a65ea225746b2589fe1df7c",
}

project = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(project / "Scripts/Editor"))
import ApplyReferenceArin as application
application = importlib.reload(application)

if application.ROOT != "/Game/Rogue10m/Characters/Arin/RefinedV3":
    raise RuntimeError("Recovery accepts only the owned RefinedV3 root")
if application.GROUP_RESOLUTIONS != {"Adventurer": 4096}:
    raise RuntimeError("Recovery accepts only one Adventurer 4K atlas")

data, blueprint, cdo, actor = application._guide_context()
before = application._guide_state(data, blueprint, cdo, actor)
baseline = json.loads(application.CHECKPOINT.read_text(encoding="utf-8-sig"))
if before != baseline:
    raise RuntimeError("Guide must still exactly match the untouched v2 checkpoint")

manifest, sources, hashes = application._surface_sources()
if hashes != FROZEN_HASHES or len(sources) != 4:
    raise RuntimeError("Recovery requires the exact inspected final FBX, manifest, and four PNG bytes")
application._source_bone_contract()
interruption_bytes = application.STATE_FILE.read_bytes()
interruption = json.loads(interruption_bytes.decode("utf-8-sig"))
if interruption.get("phase") != "aborted_requires_inspection" or interruption.get("source_hashes") != FROZEN_HASHES:
    raise RuntimeError("Recovery requires the recorded interrupted stage and exact frozen source hashes")
if ("mode" in interruption or interruption.get("validate_only") is not False
    or interruption.get("root") != application.ROOT
    or Path(interruption.get("source_fbx", "")).resolve() != application.SOURCE_FBX.resolve()):
    raise RuntimeError("Recovery accepts only the original interrupted application state, never a rewritten recovery mode")

existing = application._scope_assets()
partial = {application.MESH_PATH, application.SKELETON_PATH, application.ANIMATION_PATH} | {
    source["asset"] for source in sources.values()}
if set(existing) != partial or len(existing) != 7:
    raise RuntimeError("Recovery accepts only the inspected three core and four texture assets")
if any(application._disk_path(path).exists() for path in partial):
    raise RuntimeError("Recovery accepts only the inspected unsaved seven-asset interruption")
events = interruption.get("events", [])
if ([event.get("phase") for event in events] != ["preflight", "import_fbx", "import_4_textures", "aborted_requires_inspection"]
    or set(events[-1].get("observed_assets", [])) != partial
    or not str(events[-1].get("error", "")).startswith("Imported atlas size differs:")):
    raise RuntimeError("Recorded interruption does not describe this exact seven-asset scope")

mesh = application._require(application.MESH_PATH, unreal.SkeletalMesh)
animation = application._require(application.ANIMATION_PATH, unreal.AnimSequence)
skeleton = application._require(application.SKELETON_PATH, unreal.Skeleton)
for asset in (mesh, animation):
    imported = Path(asset.get_editor_property("asset_import_data").get_first_filename()).resolve()
    if imported != application.SOURCE_FBX.resolve():
        raise RuntimeError("Core source import filename differs: " + str(imported))
# Core metadata is intentionally absent before the original final-save stage.
# Validate the actual import filename, shared skeleton, exact source bone list,
# 170cm dimensions, two-second Idle, root motion, and imported normal settings.
core = application._validate_core(mesh, animation, skeleton)

textures = {}
before_sizes = {}
for key, source in sources.items():
    texture = application._require(source["asset"], unreal.Texture2D)
    application._verify_provenance(texture, FROZEN_HASHES)
    if unreal.EditorAssetLibrary.get_metadata_tag(texture, "Arin.TextureSHA256") != source["sha256"]:
        raise RuntimeError("Inspected texture bytes differ: " + str(key))
    imported = Path(texture.get_editor_property("asset_import_data").get_first_filename()).resolve()
    if imported != source["file"].resolve():
        raise RuntimeError("Texture import filename differs: " + str(key))
    before_sizes[source["name"]] = [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()]
    textures[key] = texture

# Preserve the interrupted report before touching the shared progress file.
archive_name = ("unreal-reference-import-interruption-45296f9fcbd0-"
    + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8] + ".json")
archive = application.EVIDENCE / archive_name
if archive.exists():
    raise RuntimeError("Preserve the existing unique interruption archive")
archive.write_bytes(interruption_bytes)

state = {"mode": "scoped_recovery_prepare_only", "source_hashes": FROZEN_HASHES,
         "first_interruption_archive": str(archive)}
try:
    application._stage(state, "finish_existing_texture_work", inspected_scope=sorted(existing))
    application._finish_texture_compilation()
    application._stage(state, "reapply_only_owned_texture_properties")
    for key, texture in textures.items():
        application._texture_settings(texture, key[1])
    application._finish_texture_compilation()
    # Gate actual resources before creating any material or touching guide data.
    texture_report = application._validate_textures(textures, sources, FROZEN_HASHES)

    application._stage(state, "create_only_missing_2_materials")
    parent, materials = application._create_materials(textures, FROZEN_HASHES)
    application._slots(mesh, materials, assign=True)
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    before_lod0_vertices = subsystem.get_num_verts(mesh, 0)
    before_skeleton = mesh.get_editor_property("skeleton")
    application._stage(state, "generate_only_owned_skeletal_lods")
    if not subsystem.regenerate_lod(mesh, 3, False, False):
        raise RuntimeError("Initial owned skeletal LOD generation failed")
    application._configure_generated_lods(mesh)
    if not subsystem.regenerate_lod(mesh, 3, False, False):
        raise RuntimeError("Boundary-protected owned skeletal LOD generation failed")
    lods = application._lod_metrics(mesh)
    if subsystem.get_num_verts(mesh, 0) != before_lod0_vertices or mesh.get_editor_property("skeleton") != before_skeleton:
        raise RuntimeError("Generated LODs changed imported LOD0 or its shared Skeleton")

    application._stage(state, "save_exactly_9_owned_dependencies")
    saved = []
    for asset in [*textures.values(), parent, *materials.values(), skeleton, animation, mesh]:
        application._set_provenance(asset, FROZEN_HASHES)
        saved.append(application._save(asset))
    expected = application._expected_paths(sources)
    if len(expected) != 9 or len(saved) != 9 or set(application._scope_assets()) != expected:
        raise RuntimeError("Final scope differs from the exact nine owned dependency packages")
    if any(not application._disk_path(path).is_file() for path in expected):
        raise RuntimeError("A prepared dependency was not explicitly saved to disk")
    after = application._guide_state(data, blueprint, cdo, actor)
    if before != after or baseline != after:
        raise RuntimeError("Guide changed during dependency-only recovery")

    report = {"status": "prepared_only_guide_unchanged", "source_hashes": FROZEN_HASHES,
        "first_interruption_archive": str(archive),
        "recovery": {"reused_imported_assets": 7, "new_material_assets": 2,
            "reimport_or_overwrite": False, "assets_deleted": False,
            "before_resource_sizes": before_sizes,
            "solution": "finish pending work; one set_editor_properties per exact owned texture; official texture finish; actual 4K resource gate before material creation"},
        "core": application._validate_core(mesh, animation, skeleton),
        "textures": texture_report, "materials": application._validate_materials(materials, parent, textures),
        "slots": application._slots(mesh, materials), "lods": lods, "saved_assets": saved,
        "package_files_verified": sorted(expected), "guide_unchanged": True,
        "guide_application_performed": False, "runtime_visual_review_required": True}
    application._json(application.EVIDENCE / "unreal-reference-prepared.json", report)
    application._stage(state, "completed", guide_unchanged=True)
    unreal.log("ARIN_REFERENCE_DEPENDENCIES_RECOVERED_AND_PREPARED")
except Exception as exc:
    application._stage(state, "aborted_requires_inspection", error=str(exc),
                       observed_assets=sorted(application._scope_assets()))
    raise
