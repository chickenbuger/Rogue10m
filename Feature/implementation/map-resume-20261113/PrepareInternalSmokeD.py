"""Fill a fresh internal-only smoke seal from real capture/build/native evidence.

No UE/UAT/game execution; owner release is created separately in its live PowerShell session.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
PACKET = ROOT / "Feature/implementation/map-resume-20261113/internal-smoke-culture-d"
EXPECTED_NATIVE_VALIDATOR = "985500591447274B367B97A89257D1CA992BA7891890D220608D7B13B383CF98"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--native-label", choices=("NativeTestsC",), required=True)
    args = parser.parse_args()
    assert hashlib.sha256((PACKET / "native43_validation.py").read_bytes()).hexdigest().upper() == EXPECTED_NATIVE_VALIDATOR
    sys.path.insert(0, str(PACKET))
    import internal_smoke
    c = internal_smoke.helpers()
    captured = args.capture / "smoke.UNSEALED.json"
    seal = json.loads(captured.read_text(encoding="utf-8-sig"))
    assert seal["root"] == str(ROOT) and seal["status"] == "UNSEALED"
    assert seal["runtime"]["python"]["path"] == str(Path(sys.executable))
    args.output_root.mkdir(exist_ok=False)
    base = ROOT / "Feature/doc/evidence/map-resume-20261113"
    native = base / args.native_label
    refs = {key: c.file_pin(native / name) for key, name in {
        "process": "process.json", "result": "result.json", "reviewed": "reviewed-result.json",
        "report": "report/index.json", "log": "editor.log", "startup_review": "startup-review-clean-en.json"}.items()}
    refs["selection"] = c.file_pin(ROOT / "Feature/implementation/map-resume-20261113/native-no-gui-tests-20261113.json")
    seal.update(status="sealed", engine_build_id=json.loads((ROOT / "Binaries/Win64/UnrealEditor.modules").read_text())["BuildId"],
                run_tag="SmokeD20261113", smoke_map_selection_final=True,
                editor_build={"result": c.file_pin(base / "TerrainRawTileBuildD/result.json"),
                              "log": c.file_pin(base / "TerrainRawTileBuildD/editor-build.log")},
                native43=refs, output_root=str(args.output_root),
                outputs={name: str(args.output_root / name) for name in c.OUTPUTS},
                note="Internal Development build only: 1800m candidate, no crop/NPC placement/default-map switch, UI/visual/physical play UNVERIFIED. New exact-English Native43; old startup15 not approved or masked.")
    assert seal["release_eligible"] is False and seal["acceptance_flags"] == internal_smoke.FLAGS
    out = args.capture.parent / "smoke-d.sealed.json"
    with out.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(seal, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    result = {"seal": c.file_pin(out), "owner_area": seal["owner_area"], "output_root": seal["output_root"],
              "maps": seal["cook_maps"], "release_eligible": False, "status": "sealed_internal_inputs_owner_release_required"}
    with (args.capture.parent / "prepared.json").open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(result, stream, indent=2)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
