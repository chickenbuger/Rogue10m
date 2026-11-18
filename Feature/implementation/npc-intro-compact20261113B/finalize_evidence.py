"""Read actual game logs and hash every prior input/save after the QA process exits."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib, json, types

R = Path(__file__).resolve().parents[3]
E = R / "Feature/doc/evidence/npc-intro-compact20261113B"
P = R / "Feature/implementation/map-resume-20261113/internal-smoke-cook-fix-e/strict_primitives.py"
raw = P.read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == "D9E27B5F44C8D52BDBB5ACA01459DA98378B47F67CE44A71720FB2EC93E38ACF"
c = types.ModuleType("intro_final_inventory"); c.__file__ = str(P)
exec(compile(raw, str(P), "exec"), c.__dict__)
assert not (E / "post-game-inputs.json").exists(), "Keep prior evidence immutable."
processes = json.loads((E / "processes-after-game.json").read_text(encoding="utf-8-sig"))
assert processes["engine_build_game_processes"] == [] and not processes["forced_termination"]
before = json.loads((E / "inputs-before.json").read_text(encoding="utf-8"))
after = c.input_inventory(R)
print("Completed full input scan", len(after["files"]), flush=True)
bf = {x["path"]: x for x in before["files"]}
af = {x["path"]: x for x in after["files"]}
assert all(af.get(k) == v for k, v in bf.items()), "Pre-existing input changed or missing."
author = json.loads((E / "authoring-preservation.json").read_text(encoding="utf-8"))
expected = {x["path"]: {"path": x["path"], "sha256": x["sha256"], "size": x["bytes"]} for x in author["added_files"]}
cache_review = json.loads((E / "generated-cache-owner-review.json").read_text(encoding="utf-8"))
assert cache_review["header_timestamp_and_size_match"] and cache_review["not_a_gameplay_source_change"]
assert cache_review["sha256"] == "2EC3C5A8D9C9C4841D2E9088CDEF927CAD9E8CE6A11557560E0B928C80470A79"
expected[cache_review["path"]] = {k: cache_review[k] for k in ("path", "sha256", "size")}
added = {k: v for k, v in af.items() if k not in bf}
assert added == expected, "Unexpected new Content/input file."
print("Verified all prior inputs, exact five Content additions and one identified generated cache", flush=True)
assert set(before["directories"]).issubset(after["directories"])
assert before["present_roots"] == after["present_roots"]
assert c.file_pin(R / "Binaries/Win64/UnrealEditor-Rogue10m.dll")["sha256"] == "CA647B1E2264CB5DBC5025F81D2C28E10E12CDE5BF889F552033F8B1D2CB9175"
checkpoint = json.loads((E / "physical-checkpoint.json").read_text(encoding="utf-8"))
saves = []
for item in checkpoint["protected_saves"]:
    old = json.loads((E / item["manifest"]).read_text(encoding="utf-8"))
    current = c.scan_tree(item["root"], True)
    c.compare_inventory(old, current, "protected saves after game")
    saves.append({"root": item["root"], "unchanged": True, "files": len(current["files"])})
old_delivery = R / "Feature/doc/evidence/map-resume-20261113/internal-smoke-e5/persistent-delivery05.json"
if not old_delivery.exists():
    matches = list((R / "Feature/doc/evidence").rglob("persistent-delivery05.json"))
    assert len(matches) == 1, matches
    old_delivery = matches[0]
delivery = json.loads(old_delivery.read_text(encoding="utf-8-sig"))
# Preserve the original delivery receipt and payload; the new NPC input is not in E5.
old_payload = R / "tmp/InternalWindows20261113E5_UNVERIFIED/Windows"
e5_inventory = c.payload_manifest(old_payload)
print("Completed old E5 payload scan", len(e5_inventory), flush=True)
assert c.digest(c.canonical(e5_inventory)) == "431968EB98AA22D819C458AABCB347E55A4C74172417A78926C16D7E13A1079C"
assert c.file_pin(R / "Binaries/Win64/Rogue10m.exe")["sha256"] == "597E833F08AC7EF6D4124BE87A10768A2B3B62C699E9E9E2172AA94E44024FA6"
qa_save_root = Path("C:/Users/PC/AppData/Local/Temp/Rogue10mNpcIntro20261113B/GameD1/User/Saved/SaveGames")
qa_saves = c.scan_tree(qa_save_root, True)
assert qa_saves["files"] and all("QA_CI20261113D01" in f["path"] for f in qa_saves["files"]), "Unexpected QA save slot."
log_path = E / "game-D1.log"
log = log_path.read_text(encoding="utf-8", errors="replace")
lines = log.splitlines()
rewards = [(i + 1, line) for i, line in enumerate(lines) if "Village quest rewarded quest=Q_Intro_supply" in line]
assert len(rewards) == 1 and "gold=40" in rewards[0][1]
gold = [(i + 1, line) for i, line in enumerate(lines) if "Rogue10mInventoryComponent " in line and "Inventory Component.Gold = " in line]
assert any("19.16.26" in line and line.endswith("Gold = 0") for _, line in gold)
assert any("19.19.50" in line and line.endswith("Gold = 40") for _, line in gold)
assert any("19.23.49" in line and line.endswith("Gold = 40") for _, line in gold)
new_guid = "FF9142314C7E305992A965929D7D97DF"
profile = "DF1401B94FFEEBF376824BA86AC0E2C8"
assert any("19.25.09" in line and "/Engine/Transient.Rogue10mProgressionSaveGame_4.RunId = " + new_guid in line for line in lines)
assert any("19.25.09" in line and "/Engine/Transient.Rogue10mProgressionSaveGame_4.ProfileId = " + profile in line for line in lines)
assert "QUIT_MENU confirmed; requesting_application_exit" in log and "LogExit: Exiting." in log
selected = [(i + 1, line) for i, line in enumerate(lines) if any(s in line for s in (".SaveData = ", ".RunId = ", ".ProfileId = ", "Inventory Component.Gold = ", "Village quest rewarded quest=", "Run portal opened level", "QUIT_MENU", "LogExit: Exiting."))]
result = {
    "schema": "R10-compact-intro-runtime-evidence-v1",
    "recorded_utc": datetime.now(timezone.utc).isoformat(),
    "status": "COLLECTION_VERTICAL_SLICE_OBSERVED_NOT_RELEASE_CERTIFIED",
    "runtime_kind": "UnrealEditor.exe -game; not packaged Windows EXE",
    "owned_pid": 51220,
    "original_profile_save_and_other_codex_inputs_preserved": True,
    "normal_menu_profile_hub_entry": True,
    "ordinary_hub_portal_destination": "/Game/Rogue10m/Maps/Production/WorldLayout20261112",
    "candidate_access": "additional declared developer console open; not a production Hub route",
    "developer_position_setup": "BugItGo followed by Walk to restore collision; not a physical timed route",
    "qa_profile_id": profile,
    "run_id_first": "A3C6878A4FDD458EEC2E899E1136627E",
    "run_id_after_normal_abandon_and_hub_return": new_guid,
    "same_profile_different_run_confirmed_from_exact_active_save_object": True,
    "observed_ui_sequence": ["F open/close/reopen", "accept 0/2", "first goal 1/2", "duplicate first goal remains 1/2", "second goal 2/2", "claim +40", "re-report already rewarded; actual Gold stays 40", "normal abandon confirmation and settlement +0", "Hub return", "new Run available/unaccepted 0/2"],
    "quest_reward_log_count": len(rewards),
    "native_reward_lines": rewards,
    "gold_observations": gold,
    "log_evidence": [{"line": i, "text": line} for i, line in selected],
    "f_interaction_limitation": "fast sequential developer teleports lost prompt; full detection exit and separate re-entry restored it; natural walking revisit and active-equipment F conflict not certified",
    "remaining": ["Compact production Hub portal/default map unchanged", "1800m map not cropped", "physical direct route three runs / 15-minute growth / full completion", "packaged runtime / UI7 / save-failure/restart", "combat Director/loot/production credit GUID integration", "quest progress across process persistence"],
    "known_visual_failures": ["Korean world TextRender tofu", "existing gray player material and missing /Engine/EngineMeshes/Humanoid dependency", "dialogue choice wraps to three lines"],
    "whole_game_log_red_zero": False,
    "denied_startup_approval_or_metadata_clone_QA_executed": False,
}
(E / "runtime-final-result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
(E / "post-game-inputs.json").write_text(json.dumps(after, indent=2) + "\n", encoding="utf-8")
(E / "qa-save-final-inventory.json").write_text(json.dumps(qa_saves, indent=2) + "\n", encoding="utf-8")
(E / "old-e5-payload-after-intro.json").write_text(json.dumps(e5_inventory, indent=2) + "\n", encoding="utf-8")
summary = {"status": "ALL_PRIOR_INPUTS_AND_PROTECTED_SAVES_PRESERVED", "recorded_utc": result["recorded_utc"], "prior_files_unchanged": len(bf), "new_files": list(added.values()), "final_input_files": len(af), "final_Content_files": sum(k.startswith("Content/") for k in af), "source_config_unchanged": sum(k.startswith(("Source/", "Config/")) for k in bf), "protected_saves": saves, "qa_save_files": len(qa_saves["files"]), "editor_dll": c.file_pin(R / "Binaries/Win64/UnrealEditor-Rogue10m.dll"), "old_e5_delivery_receipt": c.file_pin(old_delivery), "old_e5_payload_sha256": c.digest(c.canonical(e5_inventory)), "engine_build_game_processes_after": [], "forced_process_termination": False}
(E / "post-game-preservation.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
print(json.dumps({k: v for k, v in summary.items() if k != "new_files"}, indent=2))
