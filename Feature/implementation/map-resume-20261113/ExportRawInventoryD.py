"""Write new evidence BINs from the read-only UE API; never edit Unreal packages."""
import base64
import hashlib
import json
from pathlib import Path
import shutil
import unreal

ROOT = Path("D:/Project/Rogue10m")
MAP = "/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A"
OUT = ROOT / "Feature/doc/evidence/map-resume-20261113/RawInventoryD"
QA = Path("C:/Users/PC/Documents/Codex/2026-11-13/task/map-qa-crop-v2/runs/final-03/inventory-template.json")
assert Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve()
OUT.mkdir(exist_ok=False)
(OUT / "reference").mkdir()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write_json(name, value):
    (OUT / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


prefix = "Rogue10m/Maps/Production/WorldLayoutCompact20261113A"
packages = [ROOT / "Content" / (prefix + ".umap")]
for kind in ("__ExternalActors__", "__ExternalObjects__"):
    packages.extend(sorted((ROOT / "Content" / kind / prefix).rglob("*.uasset")))
assert len(packages) == 1419
package_before = {str(p.relative_to(ROOT)): sha(p.read_bytes()) for p in packages}
write_json("reference/source-package-manifest.json", package_before)
for source, name in ((ROOT / "Source/Rogue10m/Editor/Rogue10mCompactWorldAuthoring.cpp", "exporter.cpp"),
                     (ROOT / "Source/Rogue10m/Editor/Rogue10mCompactWorldAuthoring.h", "exporter.h"),
                     (ROOT / "Binaries/Win64/UnrealEditor-Rogue10m.dll", "exporter.dll")):
    shutil.copyfile(source, OUT / "reference" / name)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
assert world.get_path_name() == MAP + ".WorldLayoutCompact20261113A"
helper = unreal.Rogue10mCompactWorldAuthoring
loaded = json.loads(helper.load_candidate_region())
assert loaded.get("success")
before = json.loads(helper.snapshot_retained_terrain_data())
assert before.get("success")
raw = json.loads(helper.export_candidate_retained_tiles())
write_json("raw-transport.json", raw)
assert raw.get("success") is True and raw.get("allRequestedTilesComplete") is True
assert raw.get("rawLookupFallbackUsed") is False and raw.get("channelCount") == 8
assert raw["capture"]["allDescriptorsLoaded"] is True
after = json.loads(helper.snapshot_retained_terrain_data())
write_json("semantic-before.json", before)
write_json("semantic-after.json", after)
assert after.get("success") and before["semantic_snapshot_digest"] == after["semantic_snapshot_digest"]
inventory = json.loads(QA.read_text(encoding="utf-8-sig"))
inventory.update(sourceMap=MAP, sourceLandscapeGuid=raw["sourceLandscapeGuid"],
                 sourceLayers=raw["sourceLayers"], paintTargets=raw["paintTargets"],
                 componentLayerPresence=raw["componentLayerPresence"], rawLookupFallbackUsed=False)
expected_sections = {(x, y) for y in range(630, 883, 63) for x in range(315, 820, 63)}
channels = []
tile_bytes = 0
for channel_index, channel in enumerate(raw["channels"]):
    assert channel.get("complete") and channel["tileCount"] == 45
    assert channel["seamDuplicateOccurrences"] == 4832 and channel["seamMismatchCount"] == 0
    assert channel["sampleOrder"] == "x_fast_y_increasing"
    size = 8192 if channel["encoding"] == "u16le" else 4096
    assert channel["encoding"] in ("u16le", "u8")
    target = OUT / "channels" / ("ch%02d" % channel_index)
    target.mkdir(parents=True)
    rows, sections = [], set()
    for tile in channel["tiles"]:
        section = tuple(tile["sectionBase"])
        assert section in expected_sections and section not in sections
        sections.add(section)
        data = base64.b64decode(tile["raw_base64"], validate=True)
        assert len(data) == size == tile["byteCount"] and tile["sampleCount"] == 4096
        name = target / ("%d-%d.bin" % section)
        with name.open("xb") as stream:
            stream.write(data)
        tile_bytes += len(data)
        row = {k: v for k, v in tile.items() if k != "raw_base64"}
        row.update(file=name.relative_to(OUT).as_posix(), sha256=sha(data))
        rows.append(row)
    assert sections == expected_sections
    channels.append({**{k: v for k, v in channel.items() if k != "tiles"}, "tiles": rows})
assert tile_bytes == 1843200 and len(channels) == 8
inventory["channels"] = channels
capture = dict(raw["capture"])
capture["files"] = {}
for field, filename in (("sourcePackageManifest", "source-package-manifest.json"),
                        ("exporterSource", "exporter.cpp"), ("exporterDll", "exporter.dll")):
    path = OUT / "reference" / filename
    digest = sha(path.read_bytes())
    capture["files"][field] = {"file": path.relative_to(OUT).as_posix(), "sha256": digest}
    capture[field + "Sha256"] = digest
inventory["capture"] = capture
# Preserve partial/unknown coverage and restoration fields from the independent template.
# Valid BINs do not certify component overrides, native references, normals, or crop.
inventory["observationBoundary"] = "Actual read-only tile bytes; coverage/identity/destination/restoration remain unapproved"
write_json("inventory.json", inventory)
package_after = {str(p.relative_to(ROOT)): sha(p.read_bytes()) for p in packages}
assert package_before == package_after
write_json("result.json", {"status": "eight_channels_360_tiles_captured_not_crop_authorization",
                           "tile_bytes": tile_bytes, "canonical_bytes": 1794880,
                           "candidate_packages_unchanged": True, "semantic_unchanged": True,
                           "source_manifest_sha256": capture["sourcePackageManifestSha256"],
                           "source_cpp_sha256": capture["exporterSourceSha256"],
                           "dll_sha256": capture["exporterDllSha256"],
                           "actual_crop_performed": False, "assets_saved": False, "gui_input": 0})
unreal.log("R10_RAW_INVENTORY_8_CHANNELS_360_TILES_RECORDED_NO_CROP_AUTHORIZATION")
