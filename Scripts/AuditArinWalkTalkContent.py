"""Immutable SHA256 checkpoint and scoped audit for Arin walk/talk application.

This file only reads Content and the two existing Arin source-art directories.
It writes reports to this feature's evidence directory; no Unreal assets are
edited. Run --baseline once before importing the two new animation sequences.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "Feature/doc/evidence/arin-walk-talk-20261009"
BASELINE = EVIDENCE / "content-before-sha256.json"
PRIOR_INTEGRITY = ROOT / "Feature/doc/evidence/arin-reference-refinement-20261009/final-deliverable-integrity.json"
ART_ROOTS = (ROOT / "Art/NPC/Arin/ReferenceRefined", ROOT / "Art/NPC/Arin/Refined")
V3_PREFIX = "Content/Rogue10m/Characters/Arin/RefinedV3/"
FROZEN_V3_FILES = {
    V3_PREFIX + "SK_Arin.uasset",
    V3_PREFIX + "SK_Arin_Skeleton.uasset",
    V3_PREFIX + "ArinIdle.uasset",
    V3_PREFIX + "Materials/M_Arin_PBR.uasset",
    V3_PREFIX + "Materials/MI_Arin_Adventurer.uasset",
    *(V3_PREFIX + "Textures/T_Arin_Adventurer_" + channel + ".uasset"
      for channel in ("BaseColor", "Roughness", "Metallic", "Normal")),
}
EXPECTED_NEW_PACKAGES = {
    "/Game/Rogue10m/Characters/Arin/RefinedV3/Animations/A_Arin_Walk",
    "/Game/Rogue10m/Characters/Arin/RefinedV3/Animations/A_Arin_Talk",
}
EXPECTED_NEW_FILES = {
    package.replace("/Game/", "Content/", 1) + ".uasset"
    for package in EXPECTED_NEW_PACKAGES
}
ALLOWED_EXISTING_CHANGES = {
    "Content/Rogue10m/World/StartVillage/Tutorial/DA_StartVillageGuide.uasset",
    "Content/Rogue10m/World/StartVillage/Tutorial/BP_StartVillageGuide.uasset",
    "Content/__ExternalActors__/Rogue10m/Maps/StartHub/5/TO/W83RIU01UF417URBGM9BBZ.uasset",
}


def fingerprint(path):
    before = path.stat()
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise RuntimeError("File changed while hashing: " + str(path))
    return {"size": after.st_size, "sha256": digest.hexdigest()}


def snapshot(roots):
    files = sorted({path for root in roots for path in root.rglob("*") if path.is_file()})
    with ThreadPoolExecutor(max_workers=4) as workers:
        values = list(workers.map(fingerprint, files))
    return dict(zip((path.relative_to(ROOT).as_posix() for path in files), values))


def prior_art_hashes():
    """Use previous verified hashes only for existing project-owned Arin art."""
    if not PRIOR_INTEGRITY.is_file():
        return {}, {"available": False, "reason": "Prior integrity report absent"}
    report = json.loads(PRIOR_INTEGRITY.read_text(encoding="utf-8-sig"))
    if report.get("status") != "passed":
        raise RuntimeError("Prior integrity report was not passed")
    expected = {}
    for label, check in report.get("checks", {}).items():
        path_value = check.get("path")
        if path_value:
            path = Path(path_value).resolve()
        elif label.startswith("texture/"):
            path = ART_ROOTS[0] / "Textures" / label.removeprefix("texture/")
        else:
            continue
        if not any(path.is_relative_to(art_root.resolve()) for art_root in ART_ROOTS):
            continue
        if check.get("passed") is not True or not check.get("sha256"):
            raise RuntimeError("Prior art hash was not verified: " + label)
        expected[path.relative_to(ROOT).as_posix()] = check["sha256"]
    if not expected:
        raise RuntimeError("Prior report contains no verified Arin art hashes")
    return expected, {"available": True, "path": PRIOR_INTEGRITY.relative_to(ROOT).as_posix(),
                      "report_sha256": fingerprint(PRIOR_INTEGRITY)["sha256"],
                      "verified_art_hashes": len(expected)}


def mismatched_hashes(expected, actual):
    return sorted(path for path, digest in expected.items()
                  if path not in actual or actual[path]["sha256"] != digest)


def delta(before, current):
    return {
        "changed": sorted(path for path in before if path in current and before[path] != current[path]),
        "deleted": sorted(set(before) - set(current)),
        "added": sorted(set(current) - set(before)),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true", help="Create the immutable checkpoint before imports")
    args = parser.parse_args()
    if args.baseline and BASELINE.exists():
        raise RuntimeError("Preserve existing walk/talk checkpoint; it will not be overwritten")
    if not args.baseline and not BASELINE.is_file():
        raise RuntimeError("Run --baseline before importing the walk/talk animation assets")
    current = snapshot((ROOT / "Content",))
    art_current = snapshot(ART_ROOTS)
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    if args.baseline:
        missing_frozen = sorted(FROZEN_V3_FILES - set(current))
        collisions = sorted(EXPECTED_NEW_FILES & set(current))
        if missing_frozen or collisions:
            raise RuntimeError("Invalid pre-import state: " + json.dumps(
                {"missing_existing_v3": missing_frozen, "new_animation_name_collisions": collisions}))
        frozen_art_hashes, prior = prior_art_hashes()
        bad_art = mismatched_hashes(frozen_art_hashes, art_current)
        if bad_art:
            raise RuntimeError("Existing Arin source art differs from prior integrity: " + json.dumps(bad_art))
        report = {"schema_version": 1, "created_at_utc": datetime.now(timezone.utc).isoformat(),
                  "content": current, "existing_arin_art": art_current,
                  "prior_art_sha256": frozen_art_hashes, "prior_integrity": prior,
                  "frozen_v3_content_files": sorted(FROZEN_V3_FILES),
                  "expected_new_packages": sorted(EXPECTED_NEW_PACKAGES),
                  "allowed_existing_changes": sorted(ALLOWED_EXISTING_CHANGES)}
        with BASELINE.open("x", encoding="utf-8") as target:
            json.dump(report, target, indent=2)
            target.write("\n")
        print(json.dumps({"status": "checkpoint_created", "baseline_files": len(current),
                          "existing_arin_art_files": len(art_current), "prior_integrity": prior}, indent=2))
        return
    checkpoint = json.loads(BASELINE.read_text(encoding="utf-8-sig"))
    if checkpoint.get("schema_version") != 1:
        raise RuntimeError("Unsupported walk/talk checkpoint format")
    if (set(checkpoint.get("frozen_v3_content_files", [])) != FROZEN_V3_FILES
            or set(checkpoint.get("expected_new_packages", [])) != EXPECTED_NEW_PACKAGES
            or set(checkpoint.get("allowed_existing_changes", [])) != ALLOWED_EXISTING_CHANGES):
        raise RuntimeError("Checkpoint scope differs from this feature's fixed audit scope")
    before = checkpoint["content"]
    changes = delta(before, current)
    art_changes = delta(checkpoint["existing_arin_art"], art_current)
    unexpected_changes = sorted(set(changes["changed"]) - ALLOWED_EXISTING_CHANGES)
    unexpected_added = sorted(set(changes["added"]) - EXPECTED_NEW_FILES)
    missing_owned = sorted(EXPECTED_NEW_FILES - set(current))
    frozen_violations = sorted(path for path in FROZEN_V3_FILES
                               if path not in before or path not in current or before[path] != current[path])
    prior_art_violations = mismatched_hashes(checkpoint.get("prior_art_sha256", {}), art_current)
    passed = not (changes["deleted"] or unexpected_changes or unexpected_added or missing_owned
                  or frozen_violations or prior_art_violations or any(art_changes.values()))
    passed = passed and set(changes["added"]) == EXPECTED_NEW_FILES
    report = {"status": "passed" if passed else "failed", "baseline_files": len(before),
              "baseline_sha256": fingerprint(BASELINE)["sha256"],
              "unchanged_sha256": len(before) - len(changes["changed"]) - len(changes["deleted"]),
              **changes, "unexpected_changes": unexpected_changes, "unexpected_added": unexpected_added,
              "expected_owned_packages": sorted(EXPECTED_NEW_PACKAGES),
              "expected_added": sorted(EXPECTED_NEW_FILES), "missing_owned_dependencies": missing_owned,
              "frozen_v3_content_files": sorted(FROZEN_V3_FILES),
              "frozen_v3_violations": frozen_violations,
              "existing_arin_art_files": len(checkpoint["existing_arin_art"]),
              "existing_arin_art_changes": art_changes, "prior_art_sha256_violations": prior_art_violations,
              "prior_integrity": checkpoint["prior_integrity"]}
    (EVIDENCE / "content-preservation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    if not passed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
