"""Prepare, inspect, stage, activate or verify the Staff readability trial.

prepare is entirely offline. Live phases require the coordinator's exclusive
editor slot and reuse the existing official MCP schema-validating client. stage
creates one fresh unbound DataAsset; activate changes only the four reviewed
presentation fields on Staff Primary01. No editor/build launch or retry occurs.
"""
from __future__ import annotations

import argparse
import copy
import ctypes
import json
from pathlib import Path
import subprocess
import sys
import urllib.parse

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Scripts"))
import StaffVfxReadability20261111 as contract
import BindMagicSkillVfxMCP20261111 as native


def readability_fingerprints(plan):
    """Pin the new trial implementation in addition to the old pilot scope."""
    contract.validate_plan(plan)
    values = native.fingerprints(native.build_binding_plan(ROOT))
    for relative in (
        "Source/Rogue10m/Components/Rogue10mCombatVfxPlacement.h",
        "Source/Rogue10m/Tests/Rogue10mCombatVfxPlacementTests.cpp",
        "Source/Rogue10m/Tests/Rogue10mStaffVfxRuntimeTest.cpp",
        "Scripts/StaffVfxReadability20261111.py",
        "Scripts/Editor/StageStaffVfxReadability20261111.py",
        "Scripts/VerifyStaffVfxRuntime.ps1",
    ):
        values[relative] = native.file_sha(ROOT / relative)
    return values


def validate_listener_identity(processes, expected_project, split_commandline):
    """Direct UE listener only; clones with the same project GUID are distinct."""
    rows = processes if isinstance(processes, list) else [processes]
    if len(rows) != 1 or not isinstance(rows[0], dict):
        raise RuntimeError("Exactly one directly owned Unreal MCP listener required")
    owner = rows[0]
    if str(owner.get("Name", "")).lower() != "unrealeditor.exe":
        raise RuntimeError("MCP port owner is not direct UnrealEditor.exe; proxy identity is unproven")
    argv = split_commandline(owner.get("CommandLine", ""))
    projects = []
    for arg in argv:
        arg = arg.removeprefix("-project=")
        if arg.lower().endswith(".uproject"):
            path = Path(arg)
            if not path.is_absolute():
                raise RuntimeError("Absolute live project command-line path required")
            projects.append(path.resolve())
    if projects != [Path(expected_project).resolve()]:
        raise RuntimeError("MCP editor is running a different/unproven project checkout")
    return owner


def windows_argv(commandline):
    if not commandline:
        raise RuntimeError("Listener command line unavailable")
    argc = ctypes.c_int()
    split = ctypes.windll.shell32.CommandLineToArgvW
    split.argtypes = (ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int))
    split.restype = ctypes.POINTER(ctypes.c_wchar_p)
    result = split(commandline, ctypes.byref(argc))
    if not result:
        raise RuntimeError("Cannot parse live editor command line")
    try:
        return [result[i] for i in range(argc.value)]
    finally:
        free = ctypes.windll.kernel32.LocalFree
        free.argtypes = (ctypes.c_void_p,)
        free.restype = ctypes.c_void_p
        free(result)


def listener_identity(url):
    parsed = urllib.parse.urlparse(url)
    if sys.platform != "win32" or parsed.hostname not in ("localhost", "127.0.0.1", "::1"):
        raise RuntimeError("Direct local Windows UE listener identity required")
    port = parsed.port or (443 if parsed.scheme == "https" else 80)
    script = ("$ErrorActionPreference='Stop'; "
              f"$owners = @(Get-NetTCPConnection -LocalPort {port} -State Listen | "
              "Select-Object -ExpandProperty OwningProcess -Unique); "
              "@($owners | ForEach-Object { Get-CimInstance Win32_Process -Filter "
              "('ProcessId=' + $_) | Select-Object ProcessId,Name,CommandLine }) | ConvertTo-Json -Compress")
    result = subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
                            capture_output=True, text=True, check=True, timeout=20)
    owner = validate_listener_identity(json.loads(result.stdout), ROOT / "Rogue10m.uproject", windows_argv)
    return {"pid": owner["ProcessId"], "process_name": owner["Name"],
            "project_file": str((ROOT / "Rogue10m.uproject").resolve()), "direct_listener_port": port}


def process_exists(process_id):
    if type(process_id) is not int or process_id <= 0:
        raise RuntimeError("Valid prior editor PID required")
    script = ("$ErrorActionPreference='Stop'; "
              f"(@(Get-CimInstance Win32_Process -Filter 'ProcessId={process_id}').Count -gt 0) | ConvertTo-Json -Compress")
    result = subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
                            capture_output=True, text=True, check=True, timeout=20)
    value = json.loads(result.stdout)
    if type(value) is not bool:
        raise RuntimeError("Prior editor process liveness is unproven")
    return value


def cold_readback(applied, report, pid_exists=process_exists):
    old = applied.get("listener_identity", {}).get("pid")
    new = report.get("listener_identity", {}).get("pid")
    if type(old) is not int or type(new) is not int:
        raise RuntimeError("Pinned prior/current direct listener PIDs required")
    prior_gone = old != new and not pid_exists(old)
    report["cold_reload_validated"] = prior_gone
    report["cold_editor_restart"] = {"old_pid": old, "new_pid": new,
                                    "old_shutdown_proven": prior_gone,
                                    "proof_method": "fresh direct UE listener identity plus prior PID absent from Win32_Process"}


def exists(c, path):
    result = c.call(native.A, "exists", {"path": path})
    if isinstance(result, dict) and set(result) == {"exists"}:
        result = result["exists"]
    if type(result) is not bool:
        raise RuntimeError("Unexpected asset existence result")
    return result


def inspect(c, plan):
    # Reuse the prior pilot's read-only project/profile/character/compile gates.
    live = native.inspect_state(c, native.build_binding_plan(ROOT))
    contract.validate_source(live["properties"][plan["source_asset"]])
    return live


def audit_protected(c, before, plan, changed_source=False):
    after = {path: native.all_properties(c, path) for path in before}
    for path, values in before.items():
        if path == plan["source_asset"] and changed_source:
            contract.audit_transition(values, after[path])
        elif after[path] != values:
            raise RuntimeError("Protected asset properties changed: " + path)
    return after


def checked_set(c, path, before, report):
    proposed = copy.deepcopy(before)
    proposed.update(copy.deepcopy(contract.UPDATES))
    contract.audit_transition(before, proposed)
    pending = {k: v for k, v in contract.UPDATES.items()
               if not contract._equivalent(before[k], v)}
    if pending:
        report["mutation_attempted"] = True
        if c.call(native.O, "set_properties", {
                "instance": {"refPath": native.object_path(path)},
                "values": json.dumps(pending, allow_nan=False)}) is not True:
            raise RuntimeError("Setter failed/uncertain; inspect before retry")
    after = native.all_properties(c, path)
    report["property_audit"] = contract.audit_transition(before, after)
    return after, bool(pending)


def scoped_save(c, path, report):
    report["save_attempted"] = True
    if c.call(native.A, "save_assets", {"asset_paths": [path]}) is not True:
        raise RuntimeError("Scoped save failed/uncertain; inspect before retry")
    if c.call(native.A, "is_dirty", {"asset_path": path}) is not False:
        raise RuntimeError("Asset remains dirty after save")
    report["asset_saved"] = True
    report["saved_asset_sha256"] = native.file_sha(native.package_file(path))


def stage(c, plan, baseline, report):
    source = plan["source_asset"]
    candidate = plan["staged_asset"]
    current = inspect(c, plan)
    if current != baseline:
        raise RuntimeError("Stale live snapshot/project skills; inspect again")
    if exists(c, candidate):
        raise RuntimeError("Fresh staged destination required")
    report["mutation_attempted"] = True
    result = c.call(native.A, "duplicate", {"path": source, "new_path": candidate})
    if result is False or result is None:
        raise RuntimeError("Duplicate failed/uncertain; inspect before retry")
    if not exists(c, candidate):
        raise RuntimeError("Duplicate missing/uncertain; do not retry")
    copied = native.all_properties(c, candidate)
    if copied != baseline["properties"][source]:
        raise RuntimeError("Duplicate did not preserve every reflected source property")
    after, _ = checked_set(c, candidate, copied, report)
    report["properties_after"] = audit_protected(c, baseline["properties"], plan)
    scoped_save(c, candidate, report)
    if native.all_properties(c, candidate) != after:
        raise RuntimeError("Candidate changed after save")
    report.update(status="staged_unbound_candidate_saved_runtime_review_pending",
                  candidate_properties=after, gameplay_binding_changed=False)


def activate(c, plan, baseline, staged, report):
    source = plan["source_asset"]
    candidate = plan["staged_asset"]
    current = inspect(c, plan)
    if current != baseline:
        raise RuntimeError("Stale live snapshot/project skills; inspect again")
    if (staged.get("plan") != plan or staged.get("status") != "staged_unbound_candidate_saved_runtime_review_pending"
            or not staged.get("asset_saved") or staged.get("live") != baseline):
        raise RuntimeError("Matching saved stage report lineage required")
    if native.file_sha(native.package_file(candidate)) != staged["saved_asset_sha256"]:
        raise RuntimeError("Staged asset changed after report; inspect/review again")
    report["staged_asset_sha256"] = staged["saved_asset_sha256"]
    if c.call(native.A, "is_dirty", {"asset_path": candidate}) is not False:
        raise RuntimeError("Staged asset has unsaved changes")
    if native.all_properties(c, candidate) != staged["candidate_properties"]:
        raise RuntimeError("Staged reflected values differ from saved report")
    contract.audit_transition(baseline["properties"][source], staged["candidate_properties"])
    after, changed = checked_set(c, source, baseline["properties"][source], report)
    report["properties_after"] = audit_protected(c, baseline["properties"], plan, changed_source=True)
    if changed:
        scoped_save(c, source, report)
    if native.all_properties(c, source) != after:
        raise RuntimeError("Pilot changed after save")
    if c.call(native.A, "is_dirty", {"asset_path": source}) is not False:
        raise RuntimeError("Pilot remains dirty at activation readback")
    # No-op activation still pins the clean on-disk artifact.
    report["saved_asset_sha256"] = native.file_sha(native.package_file(source))
    report.update(status="four_property_trial_activated_gameplay_visual_review_pending",
                  gameplay_binding_changed=False, source_properties_after=after,
                  nonvisual_fields_preserved=True, saved_clean_readback_verified=True)


def verify(c, plan, baseline, applied, current_hashes, report):
    if (applied.get("phase") != "activate"
            or applied.get("status") != "four_property_trial_activated_gameplay_visual_review_pending"
            or applied.get("plan") != plan or applied.get("nonvisual_fields_preserved") is not True
            or applied.get("protected_files_preserved") is not True
            or applied.get("fingerprints_after") != current_hashes):
        raise RuntimeError("Successful pinned activation/current artifact required")
    live = inspect(c, plan)
    source = plan["source_asset"]
    if live["project_skills_sha256"] != baseline["project_skills_sha256"]:
        raise RuntimeError("Project skills changed; re-inspect before readback")
    for path, before in baseline["properties"].items():
        if path == source:
            report["property_audit"] = contract.audit_transition(before, live["properties"][path])
        elif before != live["properties"][path]:
            raise RuntimeError("Protected asset drift during readback: " + path)
    if applied.get("properties_after") != live["properties"]:
        raise RuntimeError("Complete reflected values differ from activation report")
    candidate = plan["staged_asset"]
    if (c.call(native.A, "is_dirty", {"asset_path": candidate}) is not False
            or native.file_sha(native.package_file(candidate)) != applied.get("staged_asset_sha256")):
        raise RuntimeError("Staged candidate changed after activation")
    contract.audit_transition(baseline["properties"][source], native.all_properties(c, candidate))
    if native.file_sha(native.package_file(source)) != applied.get("saved_asset_sha256"):
        raise RuntimeError("Primary01 saved file differs from activation")
    report.update(status="four_property_trial_readback_verified_gameplay_qa_pending",
                  live=live, properties_after=live["properties"], nonvisual_fields_preserved=True,
                  saved_asset_sha256=applied["saved_asset_sha256"],
                  staged_asset_sha256=applied["staged_asset_sha256"], saved_clean_readback_verified=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phase", choices=("prepare", "inspect", "stage", "activate", "verify"), default="prepare")
    parser.add_argument("--label", required=True, help="Candidate identity, kept the same across phases")
    parser.add_argument("--evidence-label", required=True, help="Fresh evidence directory for each phase")
    parser.add_argument("--url", default="http://127.0.0.1:8001/mcp")
    parser.add_argument("--lease-granted", action="store_true")
    parser.add_argument("--snapshot", type=Path)
    parser.add_argument("--snapshot-sha256")
    parser.add_argument("--stage-report", type=Path)
    parser.add_argument("--stage-report-sha256")
    parser.add_argument("--activation-report", type=Path)
    parser.add_argument("--activation-report-sha256")
    parser.add_argument("--require-cold-reload", action="store_true")
    args = parser.parse_args()
    plan = contract.build_plan(args.label)
    contract.validate_plan(plan)
    # Validate the evidence label using the same non-traversing label contract.
    contract.build_plan(args.evidence_label)
    if args.phase != "prepare" and not args.lease_granted:
        raise RuntimeError("Coordinator's exclusive editor slot required; no calls made")
    out = ROOT / "Feature/doc/evidence/staff-vfx-readability-20261111" / args.evidence_label
    if out.exists():
        raise RuntimeError("Use fresh evidence label; existing evidence is preserved")
    out.mkdir(parents=True)
    report = {"phase": args.phase, "plan": plan, "editor_or_build_launched": False,
              "mcp_dispatched": False, "mutation_attempted": False, "asset_saved": False,
              "runtime_gameplay_validated": False, "visual_quality_approved": False,
              "cold_reload_validated": False}
    client = None
    try:
        if args.phase == "prepare":
            report["status"] = "implementation_prepared_offline_no_editor_calls"
        else:
            # Existing helper rejects a missing local project ID or different live project.
            report["listener_identity"] = listener_identity(args.url)
            client = native.NativeClient(args.url, out / "mcp")
            report["mcp_dispatched"] = True
            report["identity"] = native.project_identity(client)
            live = inspect(client, plan)
            report["live"] = live
            report["fingerprints_before"] = readability_fingerprints(plan)
            if args.phase == "inspect":
                report["status"] = "read_only_snapshot_review_project_skills_before_stage"
            else:
                if not args.snapshot or native.file_sha(args.snapshot) != args.snapshot_sha256:
                    raise RuntimeError("Pinned inspect snapshot and SHA256 required")
                snapshot = json.loads(args.snapshot.read_text(encoding="utf-8"))
                if (snapshot.get("phase") != "inspect" or snapshot.get("plan") != plan
                        or snapshot.get("status") != "read_only_snapshot_review_project_skills_before_stage"
                        or args.phase != "verify" and snapshot.get("fingerprints_before") != report["fingerprints_before"]):
                    raise RuntimeError("Inspect report identity/files/plan changed")
                report["inspect_snapshot_sha256"] = args.snapshot_sha256
                if args.phase == "stage":
                    stage(client, plan, snapshot["live"], report)
                elif args.phase == "verify":
                    if not args.activation_report or native.file_sha(args.activation_report) != args.activation_report_sha256:
                        raise RuntimeError("Pinned activation report and SHA256 required")
                    applied = json.loads(args.activation_report.read_text(encoding="utf-8"))
                    if applied.get("inspect_snapshot_sha256") != args.snapshot_sha256:
                        raise RuntimeError("Activation/inspection lineage differs")
                    report["activation_report_sha256"] = args.activation_report_sha256
                    verify(client, plan, snapshot["live"], applied, report["fingerprints_before"], report)
                    cold_readback(applied, report)
                    if args.require_cold_reload and report["cold_reload_validated"] is not True:
                        raise RuntimeError("Cold readback requires a new direct UE PID and proven prior shutdown")
                else:
                    if not args.stage_report or native.file_sha(args.stage_report) != args.stage_report_sha256:
                        raise RuntimeError("Pinned successful stage report and SHA256 required")
                    staged = json.loads(args.stage_report.read_text(encoding="utf-8"))
                    if (staged.get("inspect_snapshot_sha256") != args.snapshot_sha256
                            or staged.get("fingerprints_after") != report["fingerprints_before"]):
                        raise RuntimeError("Stage lineage/files changed; inspect again")
                    report["stage_report_sha256"] = args.stage_report_sha256
                    activate(client, plan, snapshot["live"], staged, report)
            report["fingerprints_after"] = readability_fingerprints(plan)
            pilot_key = native.package_file(plan["source_asset"]).relative_to(ROOT).as_posix()
            saved_hash = report.get("saved_asset_sha256") if args.phase == "activate" else None
            native.validate_final_fingerprints(report["fingerprints_before"], report["fingerprints_after"], pilot_key, saved_hash)
            report["protected_files_preserved"] = True
    except Exception as error:
        report.update(status="stopped_no_automatic_retry", error=str(error),
                      last_action=client.last_action if client else None,
                      uncertain_mutation_outcome_requires_readonly_inspection=report["mutation_attempted"])
        raise
    finally:
        (out / "summary.json").write_text(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False), encoding="utf-8")
    print("RESULT=" + report["status"] + " evidence=" + str(out))


if __name__ == "__main__":
    main()
