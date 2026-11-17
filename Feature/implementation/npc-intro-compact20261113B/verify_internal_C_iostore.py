"""Run the recorded official UnrealPak listing, then the unchanged sealed validator."""
from pathlib import Path
from datetime import datetime, timezone
import json, subprocess, sys, types, hashlib

R = Path(__file__).resolve().parents[3]
E = R / "Feature/doc/evidence/npc-intro-compact20261113B"
T = R / "Feature/implementation/native51-internal-smoke-cook-fix-f"
P = T / "strict_primitives.py"
raw = P.read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == "0CDDDF8F2C5F8C8894DD1D5B8992546E0993E392F479DAA8B551CC7D891D6E0C"
c = types.ModuleType("intro_iostore_owner"); c.__file__ = str(P)
exec(compile(raw, str(P), "exec"), c.__dict__)
owner = c.load(c.stable_bytes(E / "internal-windows-C-owner-result.json"))
assert owner["exit_code"] == 0 and owner["execute_requested"] is True
seal = c.read_json_pin(owner["seal"])
evidence = Path(seal["outputs"]["evidence"])
post = c.load(c.stable_bytes(evidence / "postflight.json"))
assert post["status"] == "internal_smoke_artifact_built_UNVERIFIED" and post["release_eligible"] is False
assert post["seal_sha256"] == owner["seal"]["sha256"]
listing = post["next_iostore"]
assert listing["required_packages"] == seal["required_iostore_packages"] and len(listing["required_packages"]) == 7
csv = evidence / "iostore.csv"
assert not csv.exists() and not (E / "internal-C-iostore-final.json").exists()
tool = next(pin for pin in seal["tool_pins"] if pin["path"] == listing["exe"])
c.read_pin(tool)
argv = [listing["exe"], *listing["args"]]
start = datetime.now(timezone.utc)
p = subprocess.Popen(argv, cwd=R, stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW)
pid = p.pid
out, err = p.communicate()
end = datetime.now(timezone.utc)
(E / "internal-C-iostore-listing.stdout.log").write_bytes(out)
(E / "internal-C-iostore-listing.stderr.log").write_bytes(err)
receipt = {"status": "ACTUAL_OFFICIAL_LISTING_RETURNED", "started_utc": start.isoformat(), "ended_utc": end.isoformat(), "pid": pid, "args": argv, "exit_code": p.returncode, "tool": tool, "CSV": c.file_pin(csv) if csv.exists() else None, "seal": owner["seal"], "postflight": c.file_pin(evidence / "postflight.json"), "GUI_executed": False, "process_termination": False}
(E / "internal-C-iostore-listing-process.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
assert p.returncode == 0 and csv.is_file() and csv.stat().st_mtime >= start.timestamp()
assert not c.RED.search((out + b"\n" + err).decode("utf-8", errors="replace"))
cli = [sys.executable, "-I", "-B", "-S", "-X", "utf8", str(T / "internal_smoke.py"), "--seal", owner["seal"]["path"], "--seal-sha256", owner["seal"]["sha256"], "--release", owner["release"]["path"], "--release-sha256", owner["release"]["sha256"], "--owner-id", "RootNpcIntroC", "--verify-iostore", str(csv)]
vs = datetime.now(timezone.utc)
q = subprocess.Popen(cli, cwd=R, stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW)
(E / "internal-C-iostore-validator-start.json").write_text(json.dumps({"pid": q.pid, "started_utc": vs.isoformat(), "args": cli}, indent=2) + "\n", encoding="utf-8")
stdout, stderr = q.communicate()
(E / "internal-C-iostore-validator.stdout.json").write_bytes(stdout)
(E / "internal-C-iostore-validator.stderr.log").write_bytes(stderr)
result = json.loads(stdout.decode("utf-8-sig"))
summary = {"status": "ACTUAL_LISTING_AND_SEALED_IDENTITY_VALIDATION_RETURNED", "validator_exit": q.returncode, "validator_started_utc": vs.isoformat(), "validator_ended_utc": datetime.now(timezone.utc).isoformat(), "listing_receipt": c.file_pin(E / "internal-C-iostore-listing-process.json"), "validator_result": c.file_pin(E / "internal-C-iostore-validator.stdout.json"), "identity_status": result.get("status"), "required_packages": result.get("required_packages"), "world_partition_cells": len(result.get("world_partition_cells", [])), "export_rows": result.get("export_rows"), "payload_sha256": post["payload_sha256"], "release_eligible": False, "actual_package_UI_execution": 0, "metadata_clone_QA_or_denied_startup_record_operations": 0}
(E / "internal-C-iostore-final.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
print(json.dumps(summary, indent=2))
assert q.returncode == 0 and not stderr.strip() and result["status"] == "iostore_csv_identity_only_UNVERIFIED"
