"""Stage twenty independent melee DataAssets through Unreal Editor APIs.

Importing this file does not import Unreal or edit assets. The editor owner calls
stage_prototypes(apply=False, label="PlanA") first, then explicitly uses apply=True.
Only new, project-owned prototype packages are saved. Production profiles,
CharacterData, growth catalogs, unlocks and saves are never changed. Optional
prototype profiles are unassigned inspection fixtures, not production opt-in.

The script itself has only been exercised with pure recipes and mocked Unreal in
this work packet. Actual UE compilation, authoring, reload and 3P review remain
the integration owner's tasks.
"""

from dataclasses import asdict
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys


_SPEC = importlib.util.spec_from_file_location(
    "rogue10m_melee_prototype_recipes_20261111", Path(__file__).with_name("MeleeSkillPrototypeRecipes20261111.py"))
_RECIPES = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = _RECIPES
_SPEC.loader.exec_module(_RECIPES)

DESTINATION_ROOT = _RECIPES.DESTINATION_ROOT
AUTHOR_VERSION = _RECIPES.AUTHOR_VERSION
AUTHOR_TAG = "Rogue10mPrototypeAuthor"
SOURCE_TAG = "Rogue10mPrototypeSource"
IDENTITY_TAG = "Rogue10mPrototypeTechniqueId"
PENDING_TAG = "Rogue10mPrototypePending"
READY_TAG = "Rogue10mGameplayReady"
PROFILE_MUTABLE = {"profile_id", "display_name", "description", "skill_tree_skills", "active_technique_catalog"}
COMMON_MUTABLE = {"skill_name", "skill_description", "allowed_slot_policy", "enable_combo", "next_combo_skill"}
ADVANCE_FIELDS = {"advance_before_hit", "advance_distance", "advance_duration"}
CRITICAL_FIELDS = {"damage", "resource_costs", "attack_cooldown", "charge_seconds", "hit_start_delay_seconds",
                   "attack_montage", "animation_play_rate", "hit_mode", "hit_count", "hit_interval",
                   "cast_effect", "charge_effect", "impact_effect", "input_slot", "allowed_slot_policy",
                   "enable_combo", "next_combo_skill"} | ADVANCE_FIELDS


def _snake(name):
    if len(name) > 1 and name[0] == "b" and name[1].isupper():
        name = name[1:]
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


def reflected_fields(project_root, header, marker):
    """Read the local native declaration so newly added fields join the preservation guard."""
    text = (Path(project_root) / "Source/Rogue10m/Data" / header).read_text(encoding="utf-8-sig")
    if marker not in text:
        raise RuntimeError(f"Native class declaration unavailable: {marker}")
    text = text.split(marker, 1)[1]
    names = re.findall(
        r"^\s*(?:TObjectPtr<[^>]+>|TSoftObjectPtr<[^>]+>|TSubclassOf<[^>]+>|TArray<[^;]+>|TMap<[^;]+>|[A-Za-z_]\w*)\s+(\w+)\s*(?:=[^;]*)?;",
        text, re.M)
    return tuple(dict.fromkeys(_snake(name) for name in names))


def _plain(value, unreal):
    if value is None or isinstance(value, (str, bool, int, float)):
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, "items"):
        return {str(key): _plain(item, unreal) for key, item in value.items()}
    if hasattr(value, "export_text"):
        return value.export_text()
    try:
        return [_plain(item, unreal) for item in value]
    except TypeError:
        return str(value)


def _fields(asset, names, unreal):
    result = {}
    for name in names:
        try:
            result[name] = _plain(asset.get_editor_property(name), unreal)
        except Exception as error:
            raise RuntimeError(f"Stale reflection: {asset.get_path_name()}::{name}. Build and restart the editor first.") from error
    return result


def _hash(value):
    return hashlib.sha256(json.dumps(value, ensure_ascii=False, sort_keys=True).encode("utf-8")).hexdigest()


def _package(asset):
    return asset.get_path_name().split(".", 1)[0]


def _must_be_prototype(asset):
    if not _package(asset).startswith(DESTINATION_ROOT + "/"):
        raise RuntimeError("Refusing out-of-scope asset mutation/save: " + asset.get_path_name())


def _load(path, unreal, expected_type):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None or not isinstance(asset, expected_type):
        raise RuntimeError("Required existing asset missing or wrong type: " + path)
    return asset


def _overrides(row, unreal):
    result = {"skill_name": unreal.Text(row.name + " [제작중]"),
              "skill_description": unreal.Text(row.description),
              "allowed_slot_policy": getattr(unreal.Rogue10mSkillSlotPolicy, row.slot_policy),
              # A standalone technique must not chain into the original basic-attack assets.
              "enable_combo": False, "next_combo_skill": None}
    if row.advance:
        result.update(advance_before_hit=True, advance_distance=row.advance.distance_cm,
                      advance_duration=row.advance.duration_seconds)
    return result


def _verify_clone(asset, before, overrides, field_names, unreal):
    after = _fields(asset, field_names, unreal)
    allowed = set(overrides)
    unexpected = [name for name in field_names if name not in allowed and after[name] != before[name]]
    wrong = [name for name, value in overrides.items() if after[name] != _plain(value, unreal)]
    if unexpected or wrong:
        raise RuntimeError(f"Prototype contract changed at {asset.get_path_name()}: protected={unexpected}, overrides={wrong}")
    return after


def _tags_match(asset, source, identity, unreal):
    library = unreal.EditorAssetLibrary
    return (library.get_metadata_tag(asset, AUTHOR_TAG) == AUTHOR_VERSION
            and library.get_metadata_tag(asset, SOURCE_TAG) == source.get_path_name()
            and library.get_metadata_tag(asset, IDENTITY_TAG) == identity
            and library.get_metadata_tag(asset, READY_TAG) == "false")


def _tag(asset, source, identity, pending, unreal):
    _must_be_prototype(asset)
    for name, value in {AUTHOR_TAG: AUTHOR_VERSION, SOURCE_TAG: source.get_path_name(), IDENTITY_TAG: identity,
                        PENDING_TAG: json.dumps(pending, ensure_ascii=False), READY_TAG: "false"}.items():
        unreal.EditorAssetLibrary.set_metadata_tag(asset, name, value)


def _require_advance_source(row, source, unreal):
    if not row.advance:
        return
    if source.get_editor_property("hit_mode") != unreal.Rogue10mAttackHitMode.SINGLE or source.get_editor_property("hit_count") != 1:
        raise RuntimeError(f"Advance requires an existing single-hit source; no hit-count retuning allowed: {row.document_id}")


def _catalog_entries(rows, staged, unreal):
    entries = []
    for row in rows:
        entry = unreal.Rogue10mActiveTechniqueCatalogEntry()
        entry.set_editor_property("technique_id", unreal.Name(row.technique_id))
        entry.set_editor_property("skill", staged[row.document_id])
        entries.append(entry)
    return entries


def stage_prototypes(*, apply=False, create_staged_profiles=False, label="PrototypePlanA", unreal_module=None):
    """Preflight everything, then optionally duplicate/save only the prototype destination.

    Existing matching generated prototypes are reused unchanged. Unrelated or
    manually edited destinations are rejected before any new package is made.
    A failed batch keeps its partial candidates and reports them; it is not an
    atomic asset transaction and does not delete files or reset user edits.
    """
    if not re.fullmatch(r"[A-Za-z0-9_]{1,32}", label):
        raise ValueError("Use a short new alphanumeric evidence label")
    if unreal_module is None:
        import unreal as unreal_module
    unreal = unreal_module
    rows = _RECIPES.recipes()
    _RECIPES.validate_recipes(rows)
    project = Path(unreal.Paths.project_dir()).resolve()
    evidence = project / "Feature/doc/evidence/melee-skill-prototypes-20261111" / label
    if evidence.exists():
        raise RuntimeError("Fresh evidence label required; previous evidence is preserved: " + str(evidence))
    evidence.mkdir(parents=True)
    report = {"author_version": AUTHOR_VERSION, "apply_requested": bool(apply),
              "create_staged_profiles": bool(create_staged_profiles), "status": "preflight",
              "actual_engine_authoring_verified_by_this_script_test": False,
              "production_opt_in": False, "existing_profiles_changed": False,
              "candidate_split_is_proposal": True, "created_assets": [], "explicitly_saved_assets": [],
              "reused_existing_prototypes": [], "recipes": [asdict(row) for row in rows],
              "pending_integration": ["UE build/restart and real authoring/reload", "root-motion and montage playback check",
                                      "3P ready/contact/recovery, movement/wall and VFX review",
                                      "new parry/lift/three-cut/path-damage mechanics where marked",
                                      "new growth/UI nodes and save migration before production opt-in",
                                      "fixture assignment can auto-grant legacy-unmanaged clones; never assign production profiles"]}

    def write_report():
        (evidence / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    try:
        for name in ("Rogue10mAttackSkillData", "Rogue10mWeaponSkillProfileDataAsset", "Rogue10mSkillSlotPolicy",
                     "Rogue10mAttackHitMode", "Rogue10mActiveTechniqueCatalogEntry"):
            if not hasattr(unreal, name):
                raise RuntimeError("Required native reflection unavailable; build/restart first: " + name)
        skill_fields = reflected_fields(project, "Rogue10mAttackSkillData.h", "class ROGUE10M_API URogue10mAttackSkillData")
        if len(skill_fields) < 65 or not CRITICAL_FIELDS.issubset(skill_fields):
            raise RuntimeError("Native preservation schema is incomplete; do not author with stale source/build")
        profile_fields = reflected_fields(project, "Rogue10mSkillLoadoutDataAsset.h", "class ROGUE10M_API URogue10mWeaponSkillProfileDataAsset")
        if not {"default_skill_bindings", "initially_unlocked_skills", "skill_tree_skills", "active_technique_catalog"}.issubset(profile_fields):
            raise RuntimeError("Profile preservation schema is incomplete")
        sources = {path: _load(path, unreal, unreal.Rogue10mAttackSkillData) for path in sorted({row.source_package for row in rows})}
        profiles = {weapon: _load(path, unreal, unreal.Rogue10mWeaponSkillProfileDataAsset)
                    for weapon, path in _RECIPES.PROFILE_SOURCES.items()}
        source_before = {path: _fields(asset, skill_fields, unreal) for path, asset in sources.items()}
        profile_before = {weapon: _fields(asset, profile_fields, unreal) for weapon, asset in profiles.items()}
        report["source_snapshot_sha256"] = {path: _hash(value) for path, value in source_before.items()}
        report["profile_snapshot_sha256"] = {weapon: _hash(value) for weapon, value in profile_before.items()}
        report["preserved_native_skill_fields"] = len(skill_fields)
        existing = {}
        for row in rows:
            source = sources[row.source_package]
            if source.get_editor_property("attack_montage") is None:
                raise RuntimeError("Source montage missing; cannot silently create an empty attack: " + row.source_package)
            if source.get_editor_property("advance_before_hit"):
                raise RuntimeError("Legacy source already has advance behavior; inspect before repurposing: " + row.source_package)
            _require_advance_source(row, source, unreal)
            overrides = _overrides(row, unreal)
            if unreal.EditorAssetLibrary.does_asset_exist(row.destination_package):
                asset = _load(row.destination_package, unreal, unreal.Rogue10mAttackSkillData)
                if not _tags_match(asset, source, row.technique_id, unreal):
                    raise RuntimeError("Destination is not an unchanged owned prototype: " + row.destination_package)
                _verify_clone(asset, source_before[row.source_package], overrides, skill_fields, unreal)
                existing[row.document_id] = asset
        if create_staged_profiles:
            for weapon in profiles:
                path = f"{DESTINATION_ROOT}/Profiles/DA_PrototypeProfile_{weapon}"
                if unreal.EditorAssetLibrary.does_asset_exist(path):
                    # Profile generations are intentionally fresh-only: never overwrite a designer's fixture.
                    raise RuntimeError("Prototype profile already exists; preserve it and inspect manually: " + path)
        report["matching_existing_prototypes"] = sorted(existing)
        if not apply:
            report["status"] = "dry_run_only_no_assets_created"
            write_report()
            return report

        staged = {}
        save_queue = []
        for row in rows:
            source = sources[row.source_package]
            if row.document_id in existing:
                asset = existing[row.document_id]
                report["reused_existing_prototypes"].append(row.destination_package)
            else:
                unreal.EditorAssetLibrary.make_directory(row.destination_package.rsplit("/", 1)[0])
                asset = unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(), row.destination_package)
                if asset is None:
                    raise RuntimeError("Duplicate failed: " + row.destination_package)
                _must_be_prototype(asset)
                report["created_assets"].append(row.destination_package)
                for name, value in _overrides(row, unreal).items():
                    asset.set_editor_property(name, value)
                _tag(asset, source, row.technique_id, row.pending, unreal)
                _verify_clone(asset, source_before[row.source_package], _overrides(row, unreal), skill_fields, unreal)
                save_queue.append(asset)
            staged[row.document_id] = asset

        if create_staged_profiles:
            for weapon, source in profiles.items():
                path = f"{DESTINATION_ROOT}/Profiles/DA_PrototypeProfile_{weapon}"
                unreal.EditorAssetLibrary.make_directory(path.rsplit("/", 1)[0])
                profile = unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(), path)
                if profile is None:
                    raise RuntimeError("Prototype profile duplicate failed: " + path)
                report["created_assets"].append(path)
                _must_be_prototype(profile)
                selected = tuple(row for row in rows if row.weapon == weapon)
                tree = list(source.get_editor_property("skill_tree_skills"))
                if any(skill is None for skill in tree):
                    raise RuntimeError("Existing full tree contains null; do not silently rewrite it")
                for row in selected:
                    if staged[row.document_id] not in tree:
                        tree.append(staged[row.document_id])
                profile.set_editor_property("profile_id", unreal.Name(f"Prototype.{weapon}.20261111"))
                profile.set_editor_property("display_name", unreal.Text(f"{weapon} 기술 제작 검수용"))
                profile.set_editor_property("description", unreal.Text("미할당 제작 fixture. 기존 기본장착·초기해금 배열 보존. 연결 시 legacy 자동해금 가능; production 연결 금지."))
                profile.set_editor_property("skill_tree_skills", tree)
                profile.set_editor_property("active_technique_catalog", _catalog_entries(selected, staged, unreal))
                _tag(profile, source, f"Prototype.Profile.{weapon}", ("Unassigned fixture; not production opt-in",), unreal)
                after = _fields(profile, profile_fields, unreal)
                changed = [name for name in profile_fields if name not in PROFILE_MUTABLE and after[name] != profile_before[weapon][name]]
                if changed:
                    raise RuntimeError("Prototype profile changed protected defaults/unlocks: " + repr(changed))
                result = profile.validate_active_technique_catalog()
                valid = bool(result[0]) if isinstance(result, tuple) else bool(result)
                if not valid:
                    raise RuntimeError("Native prototype catalog validation failed: " + str(result))
                save_queue.append(profile)

        for path, source in sources.items():
            if _fields(source, skill_fields, unreal) != source_before[path]:
                raise RuntimeError("Original source mutated during authoring: " + path)
        for weapon, source in profiles.items():
            if _fields(source, profile_fields, unreal) != profile_before[weapon]:
                raise RuntimeError("Original profile mutated during authoring: " + weapon)
        for asset in save_queue:
            _must_be_prototype(asset)
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
                raise RuntimeError("Prototype save failed: " + asset.get_path_name())
            report["explicitly_saved_assets"].append(_package(asset))
        report["status"] = "prototype_data_staged_pending_real_runtime_review"
        report["generated_skill_count"] = len(staged)
        report["source_and_profile_fields_preserved"] = True
        write_report()
        return report
    except Exception as error:
        report["status"] = "failed_candidates_preserved_no_production_opt_in"
        report["error"] = str(error)
        write_report()
        raise


if __name__ == "__main__":
    stage_prototypes()  # Review-only unless the editor owner explicitly calls apply=True.
