"""Record the actual saved NPC checkpoint as internal build inputs; no engine execution."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib, json, types

R = Path(__file__).resolve().parents[3]
E = R / "Feature/doc/evidence/npc-intro-compact20261113B"
T = R / "Feature/implementation/native51-internal-smoke-cook-fix-f"
A = Path("C:/Users/PC/AppData/Local/Temp/R10Native51IntroSmoke20261113C")
C = A / "capture01"
entry = T / "internal_smoke.py"
raw = entry.read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == "5ACCE0EB5443ED9C5DB9ECDF8B76BB227E1DE4E66771114DAB3A1DD3102EA192"
m = types.ModuleType("intro_smoke_owner"); m.__file__ = str(entry)
exec(compile(raw, str(entry), "exec"), m.__dict__)
c = m.helpers()
s = c.load(c.stable_bytes(C / "smoke.UNSEALED.json"))
assert s["status"] == "UNSEALED" and s["packaging_status"] == "NOT_EXECUTED"
assert s["root"] == str(R) and s["owner_area"] == str(A)
assert len(s["cook_maps"]) == 6 and s["cook_maps"][-2:] == ["/Game/Rogue10m/Maps/Production/WorldLayout20261112", "/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A"]
inputs = c.read_json_pin(s["input_manifest"])
assert inputs == c.load(c.stable_bytes(E / "post-game-inputs.json"))
assert len(inputs["files"]) == 10466
assert sum(x["path"].startswith("Content/") for x in inputs["files"]) == 9526
proof = c.load(c.stable_bytes(E / "post-game-preservation.json"))
assert proof["status"] == "ALL_PRIOR_INPUTS_AND_PROTECTED_SAVES_PRESERVED" and proof["prior_files_unchanged"] == 10460
assert proof["source_config_unchanged"] == 368
assert s["engine_build_id"] == "55116800"
assert s["native51"]["build_result"] == s["editor_build"]["result"]
assert proof["editor_dll"] in s["editor_products"]
assert s["preserved_artifacts"] and s["preserved_artifacts"][0] == proof["old_e5_delivery_receipt"]
out = A / "run01"
assert not out.exists()
out.mkdir()
s["run_tag"] = "Native51IntroC01"
s["output_root"] = str(out)
s["outputs"] = {k: str(out / k) for k in c.OUTPUTS}
s["smoke_map_selection_final"] = True
s["required_iostore_packages"] = list(s["cook_maps"]) + ["/Game/Rogue10m/World/Production/WorldLayoutCompact20261113A/Quests/DA_IntroSupply_Compact20261113B"]
s["content_state"] = {
    "build_ABI_correction": "Compile default LiveCoding enabled; UBT -NoLiveCoding override removed to align installed CoreUObject registration ABI. Runtime/cooker flag remains disabled. Actual fixed Game startup not yet observed.",
    "status": "OWNER_RECORDED_FINAL",
    "map_extent_m": [1800, 1800],
    "npc_placement": "Saved/cold-reloaded additive DA1+Director1; runtime Serin1+collection2; accept/reward/duplicate/new-Run UI observed in Editor-game with declared developer position/map setup. Physical/packaged play not certified.",
    "default_map": "Unchanged L_Menu/StartHub; existing Hub portals preserved; Compact has no ordinary Hub portal. Both original WorldLayout and Compact are explicitly cooked.",
    "remaining": ["Landscape not cropped", "world Korean TextRender tofu", "existing gray player/material and missing vendor skeleton dependency", "ordinary walking F revisit and equipped-skill conflicts unverified", "packaged UI7/save-failure/restart/physical route/full run unverified", "separate denied startup approval and metadata/clone QA not executed"],
    "new_content_files": 5,
    "identified_editor_generated_cache_files": 1,
    "release_complete": False,
}
frozen = {
    "schema": "R10-native51-content-freeze-v1", "status": "frozen_for_internal_smoke",
    "project": s["project"], "active_destination": s["active_destination"],
    "input_manifest_sha256": s["input_manifest"]["sha256"], "map_manifest_sha256": s["map_manifest"]["sha256"],
    "editor_dll_sha256": proof["editor_dll"]["sha256"],
    "recorded_utc": datetime.now(timezone.utc).isoformat(), "recorder": "Root shared integration Work, actual saved checkpoint identity",
    "content_state": s["content_state"],
    "evidence": [c.file_pin(E / p) for p in ("authoring-result.json", "authoring-save-intent.json", "cold-reload-result.json", "runtime-final-result.json", "post-game-preservation.json", "generated-cache-owner-review.json", "hub-portals-live.json", "processes-after-game.json")],
    "note": "Records actual inputs for an internal Development artifact. This is not the denied root-startup approval, metadata/clone QA permission, raw UE test evidence or release certification.",
}
fp = C / "content-freeze.recorded.json"
with fp.open("x", encoding="utf-8", newline="\n") as stream:
    json.dump(frozen, stream, ensure_ascii=False, indent=2); stream.write("\n")
s["content_freeze"] = c.file_pin(fp)
s["status"] = "sealed"
sp = C / "smoke.sealed.json"
with sp.open("x", encoding="utf-8", newline="\n") as stream:
    json.dump(s, stream, ensure_ascii=False, indent=2); stream.write("\n")
summary = {"status": "ACTUAL_CONTENT_INPUTS_SEALED_INTERNAL_DEVELOPMENT_NOT_EXECUTED", "seal": c.file_pin(sp), "content_freeze": s["content_freeze"], "input_manifest": s["input_manifest"], "map_manifest": s["map_manifest"], "output_root": s["output_root"], "input_files": len(inputs["files"]), "cook_maps": s["cook_maps"], "release_eligible": False, "owner_execution_release": "NOT_CREATED", "UE_UAT_game_executions_by_this_step": 0}
with (E / "internal-smoke-C-content-checkpoint.json").open("x", encoding="utf-8", newline="\n") as stream:
    json.dump(summary, stream, ensure_ascii=False, indent=2); stream.write("\n")
print(json.dumps(summary, ensure_ascii=False, indent=2))
