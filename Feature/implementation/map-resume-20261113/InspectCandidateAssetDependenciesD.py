"""Read the on-disk Asset Registry graph separately from live object references.

This does not load candidate worlds/assets, save packages, or authorize cropping.
"""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path("D:/Project/Rogue10m")
MAP = "/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A"
SOURCE_MAP = "/Game/Rogue10m/Maps/Production/WorldLayout20261112"
OUT = ROOT / "Feature/doc/evidence/map-resume-20261113/AssetDependencyD"
assert Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve()
OUT.mkdir(exist_ok=False)
prefix = "Rogue10m/Maps/Production/WorldLayoutCompact20261113A"
files = [ROOT / "Content" / (prefix + ".umap")]
for kind in ("__ExternalActors__", "__ExternalObjects__"):
    files.extend(sorted((ROOT / "Content" / kind / prefix).rglob("*.uasset")))
assert len(files) == 1419 and all(p.is_file() for p in files), "Candidate package set changed"
before = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()
options = unreal.AssetRegistryDependencyOptions()
for name in ("include_soft_package_references", "include_hard_package_references",
             "include_searchable_names", "include_soft_management_references",
             "include_hard_management_references"):
    options.set_editor_property(name, True)
rows = []
for path in files:
    package = "/Game/" + path.relative_to(ROOT / "Content").with_suffix("").as_posix()
    assets = registry.get_assets_by_package_name(package, include_only_on_disk_assets=True)
    dependencies = registry.get_dependencies(package, options)
    referencers = registry.get_referencers(package, options)
    rows.append({"package": package, "asset_records": len(assets),
                 "dependencies": None if dependencies is None else sorted(str(v) for v in dependencies),
                 "referencers": None if referencers is None else sorted(str(v) for v in referencers),
                 "coverage": "registered_on_disk_package" if assets else "no_asset_record_do_not_infer_no_refs"})
source_referencers = registry.get_referencers(SOURCE_MAP, options)
after = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
assert before == after, "Package bytes changed during read-only registry query"
dirty = {"maps": [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
         "content": [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]}
report = {"candidate_map": MAP, "source_map": SOURCE_MAP,
          "packages": rows, "candidate_package_count": len(rows),
          "packages_with_asset_records": sum(r["asset_records"] > 0 for r in rows),
          "source_map_referencers": None if source_referencers is None else sorted(str(v) for v in source_referencers),
          "same_package_bytes": before == after, "dirty": dirty,
          "crop_authorized_by_this_report": False, "assets_saved": False, "gui_input_operations": 0,
          "scope": "Asset Registry on-disk package/searchable-name/manage graph only. Not live object references, soft subobject property paths, native non-UPROPERTY fields, or registry completeness certification."}
(OUT / "registry.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
(OUT / "package-sha256.json").write_text(json.dumps(before, indent=2), encoding="utf-8")
assert not dirty["maps"] and not dirty["content"], "Dirty packages observed; none were saved"
unreal.log("R10_CANDIDATE_ASSET_REGISTRY_READ_COMPLETE")
