"""Build offline derivative proof/diff, then optionally seal files read-only.

Writes only below this new Feature packet. Never starts any external process.
"""
import argparse
import ast
from datetime import datetime, timezone
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import stat

HERE = Path(__file__).resolve().parent
UPSTREAM = HERE.parent / "internal-smoke-culture-utc-d"
UPSTREAM_MANIFEST = "4F3B090D23CD670F5490967DA124485A7EF8F73BE8F21D62283089E3F4460735"
WRAPPER = "32A95E82FB7C68485FD90086D0F185E583BAC86A9B28287B2E69F1367C4CFBD9"
NATIVE = "985500591447274B367B97A89257D1CA992BA7891890D220608D7B13B383CF98"
CHANGED = {"strict_primitives.py", "internal_smoke.py"}


def pin(path, relative=False):
    path = Path(path); data = path.read_bytes()
    return {"path": path.relative_to(HERE).as_posix() if relative else str(path),
            "size": len(data), "sha256": hashlib.sha256(data).hexdigest().upper()}


def write_new(name, value):
    path = HERE / name
    if path.exists(): raise ValueError("Existing artifact preserved: " + name)
    data = value if isinstance(value, bytes) else (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode()
    with path.open("xb") as stream: stream.write(data)


def outside_functions(raw):
    source = raw.decode("utf-8"); lines = source.splitlines(keepends=True)
    nodes = [n for n in ast.parse(source).body if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))
             and n.name in {"argument_plan", "exact_cooker_command", "postflight"}]
    if len(nodes) != 3: raise ValueError("Wrong scoped function set")
    for node in sorted(nodes, key=lambda n: n.lineno, reverse=True):
        lines[node.lineno-1:node.end_lineno] = ["# MASKED_ALLOWED_FUNCTION:" + node.name + "\n"]
    return "".join(lines)


def replay_one(original, patch_lines):
    """Exact in-memory unified-hunk replay; never applies files to a checkout."""
    old = original.decode("utf-8").splitlines(keepends=True)
    out = []; cursor = 0; i = 0
    while i < len(patch_lines):
        line = patch_lines[i]
        if not line.startswith("@@ "): i += 1; continue
        match = re.match(r"@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@", line)
        if not match: raise ValueError("Invalid unified hunk")
        start = int(match[1]) - 1
        if start < cursor: raise ValueError("Overlapping unified hunk")
        out.extend(old[cursor:start]); cursor = start; i += 1
        old_count = new_count = 0
        while i < len(patch_lines) and not patch_lines[i].startswith("@@ "):
            line = patch_lines[i]
            if not line or line[0] not in " +-": raise ValueError("Unsupported patch row")
            text = line[1:]
            if line[0] in " -":
                if cursor >= len(old) or old[cursor] != text: raise ValueError("Patch context mismatch")
                cursor += 1; old_count += 1
            if line[0] in " +": out.append(text); new_count += 1
            i += 1
        if old_count != int(match[2] or 1) or new_count != int(match[4] or 1): raise ValueError("Hunk count mismatch")
    out.extend(old[cursor:])
    return "".join(out).encode("utf-8")


def prepare():
    if pin(UPSTREAM / "DERIVATIVE.sha256.json")["sha256"] != UPSTREAM_MANIFEST:
        raise ValueError("Frozen UTC-D upstream manifest differs")
    utc_manifest = json.loads((UPSTREAM / "DERIVATIVE.sha256.json").read_bytes())
    utc_records = utc_manifest["files"]
    for member in utc_records:
        actual = pin(UPSTREAM / member["path"])
        if actual["sha256"] != member["sha256"] or actual["size"] != member["size"]:
            raise ValueError("Frozen upstream member changed: " + member["path"])
    original_manifest = json.loads((HERE / "ARTIFACTS.sha256.json").read_bytes())
    names = [entry["path"] for entry in original_manifest["files"]] + ["ARTIFACTS.sha256.json"]
    differences = [name for name in names if (UPSTREAM / name).read_bytes() != (HERE / name).read_bytes()]
    if set(differences) != CHANGED: raise ValueError("Unexpected copied member delta: " + repr(differences))
    old_helper = (UPSTREAM / "strict_primitives.py").read_bytes()
    new_helper = (HERE / "strict_primitives.py").read_bytes()
    if outside_functions(old_helper) != outside_functions(new_helper): raise ValueError("Change outside three allowed helper functions")
    helper_sha = pin(HERE / "strict_primitives.py")["sha256"]
    expected_internal = (UPSTREAM / "internal_smoke.py").read_text(encoding="utf-8").replace(
        "02DF5B26A566EBF528280B3DEAB1E97081E9CD7B3AAE6B47A8277C8D6A029980", helper_sha).replace(
        "Byte-identical strict primitives changed; old strict packet is not modified or relaxed",
        "Pinned isolated Cook-Fix-E helper changed; upstream strict packets remain immutable").replace(
        "Strict helper identity changed", "Cook-Fix-E helper identity changed")
    if expected_internal != (HERE / "internal_smoke.py").read_text(encoding="utf-8"): raise ValueError("Unexpected internal validator delta")
    if pin(HERE / "Invoke-InternalSmoke.ps1")["sha256"] != WRAPPER or pin(HERE / "native43_validation.py")["sha256"] != NATIVE:
        raise ValueError("Wrapper/native changed")
    patches = []; replay = []
    for name in sorted(CHANGED):
        before = (UPSTREAM / name).read_bytes(); after = (HERE / name).read_bytes()
        lines = list(difflib.unified_diff(before.decode().splitlines(keepends=True), after.decode().splitlines(keepends=True),
                                       fromfile="a/" + name, tofile="b/" + name, n=3))
        if replay_one(before, lines[2:]) != after: raise ValueError("Exact in-memory replay mismatch")
        patches.extend(lines)
        replay.append({"path": name, "before": pin(UPSTREAM / name), "after": pin(HERE / name, True), "exact_replay": "PASS"})
    write_new("cook-fix-e.patch", "".join(patches).encode())
    write_new("UPSTREAM-PROVENANCE.json", {"schema": "R10-cook-fix-e-lineage-v1", "scope": "internal_development_smoke",
        "upstream_root": str(UPSTREAM), "upstream_manifest": pin(UPSTREAM / "DERIVATIVE.sha256.json"),
        "upstream_manifest_members_rechecked": len(utc_records), "copied_original_members": len(names),
        "changed_members": sorted(CHANGED), "unchanged_copied_members": len(names)-len(CHANGED),
        "source_upstream_modified": False, "shared_project_modified": False,
        "runtime": [pin(HERE / n, True) for n in ("strict_primitives.py", "internal_smoke.py", "Invoke-InternalSmoke.ps1", "native43_validation.py", "native43-policy.json")],
        "helper_outside_three_functions": "BYTE_IDENTICAL", "internal_changes": "HELPER_SHA and two explanations only",
        "single_diff": pin(HERE / "cook-fix-e.patch", True), "exact_in_memory_replay": replay})
    print(json.dumps({"prepared": True, "copied": len(names), "changed": sorted(CHANGED), "upstream_members": len(utc_records)}))


def freeze():
    if (HERE / "DERIVATIVE.sha256.json").exists(): raise ValueError("Already frozen; never overwrite")
    result = json.loads((HERE / "evidence/offline-run03/validation.json").read_bytes())
    if result["status"] != "PASS" or result["failures"] or result["errors"]: raise ValueError("Offline suite not passing")
    review = json.loads((HERE / "INDEPENDENT-REVIEW.json").read_bytes())
    if review["status"] != "PASS" or review["critical_findings"]: raise ValueError("Independent review not passing")
    write_new("FROZEN.json", {"schema": "R10-cook-fix-e-frozen-v1", "status": "FROZEN_OFFLINE_REVIEWED_NO_UE_EXECUTION",
        "frozen_utc": datetime.now(timezone.utc).isoformat(), "runtime": [pin(HERE / n, True) for n in sorted(CHANGED)],
        "upstream_packet_immutable": True, "ue_ubt_uat_cook_game_mcp_gui_executions": 0,
        "release_eligible": False, "UI7": "UNVERIFIED", "visual_review": "UNVERIFIED",
        "instruction": "Do not change or add files here after handoff; actual owner uses external fresh capture/output/seal/release."})
    files = sorted(p for p in HERE.rglob("*") if p.is_file())
    if any("scratch" in p.relative_to(HERE).parts or "__pycache__" in p.relative_to(HERE).parts for p in files):
        raise ValueError("Unexpected disposable/cache files in final packet")
    for path in files: os.chmod(path, stat.S_IREAD)
    manifest = {"schema": "R10-cook-fix-e-derivative-manifest-v1", "status": "FROZEN_OFFLINE_REVIEWED_NO_UE_EXECUTION",
        "upstream_manifest_sha256": UPSTREAM_MANIFEST, "self_excluded": "DERIVATIVE.sha256.json",
        "files": [pin(p, True) for p in files]}
    write_new("DERIVATIVE.sha256.json", manifest)
    os.chmod(HERE / "DERIVATIVE.sha256.json", stat.S_IREAD)
    print(json.dumps({"packet": str(HERE), "manifest": pin(HERE / "DERIVATIVE.sha256.json", True),
        "files": len(files), "runtime": [pin(HERE / n, True) for n in ("strict_primitives.py", "internal_smoke.py", "Invoke-InternalSmoke.ps1", "native43_validation.py")],
        "patch": pin(HERE / "cook-fix-e.patch", True)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(); parser.add_argument("action", choices=("prepare", "freeze"))
    args = parser.parse_args()
    prepare() if args.action == "prepare" else freeze()
