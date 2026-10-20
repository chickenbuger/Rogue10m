"""Actual SHA256 Content checkpoint/audit for the scoped Arin v2 application."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
EVIDENCE=ROOT/"Feature/doc/evidence/arin-quality-20261007"


def fingerprint(path):
    digest=hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda:source.read(1024*1024),b""):
            digest.update(chunk)
    return {"size":path.stat().st_size,"sha256":digest.hexdigest()}


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--baseline",action="store_true")
    args=parser.parse_args()
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    files=sorted(path for path in (ROOT/"Content").rglob("*") if path.is_file())
    with ThreadPoolExecutor(max_workers=4) as workers:
        current=dict(zip((path.relative_to(ROOT).as_posix() for path in files),workers.map(fingerprint,files)))
    path=EVIDENCE/"content-before-sha256.json"
    if args.baseline:
        if path.exists():
            raise RuntimeError("Preserve existing checkpoint")
        path.write_text(json.dumps(current,indent=2),encoding="utf-8")
        print("Content SHA256 baseline: "+str(len(current)))
        return
    before=json.loads(path.read_text(encoding="utf-8"))
    application=json.loads((EVIDENCE/"unreal-refined-application.json").read_text(encoding="utf-8"))
    package=application["saved_actor_package"]
    allowed={"Content/Rogue10m/World/StartVillage/Tutorial/DA_StartVillageGuide.uasset",
             "Content/Rogue10m/World/StartVillage/Tutorial/BP_StartVillageGuide.uasset",
             package.replace("/Game/","Content/",1)+".uasset"}
    changed=sorted(path for path in before if path in current and before[path]!=current[path])
    missing=sorted(set(before)-set(current))
    added=sorted(set(current)-set(before))
    unexpected=sorted(set(changed)-allowed)
    outside=[path for path in added if not path.startswith("Content/Rogue10m/Characters/Arin/RefinedV2/")]
    passed=not(missing or unexpected or outside)
    report={"status":"passed" if passed else "failed","baseline_files":len(before),
            "unchanged_sha256":len(before)-len(changed)-len(missing),"changed":changed,"added":added,
            "deleted":missing,"unexpected_changes":unexpected,"unexpected_added":outside}
    (EVIDENCE/"content-preservation.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    print(json.dumps(report,indent=2))
    if not passed:
        raise SystemExit(1)


if __name__=="__main__":
    main()
