"""Physical candidate checkpoint and read-only input/save inventory, no Unreal execution."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib, json, shutil, subprocess, types

R = Path(__file__).resolve().parents[3]
E = R / "Feature/doc/evidence/npc-intro-compact20261113B"
B = R / "tmp/NpcIntroCompact20261113B/before"
P = R / "Feature/implementation/map-resume-20261113/internal-smoke-cook-fix-e/strict_primitives.py"
raw = P.read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == "D9E27B5F44C8D52BDBB5ACA01459DA98378B47F67CE44A71720FB2EC93E38ACF"
c = types.ModuleType("intro_inventory"); c.__file__ = str(P)
exec(compile(raw, str(P), "exec"), c.__dict__)
assert not E.exists() and not B.exists(), "Use a new checkpoint version; never overwrite."
before = c.input_inventory(R)
old = c.load(c.stable_bytes(R / "Feature/doc/evidence/npc-combat-credit-20261113A/post-native-inputs.json"))
c.compare_inventory(old, before, "post-NPC input baseline")
assert c.file_pin(R / "Binaries/Win64/UnrealEditor-Rogue10m.dll")["sha256"] == "CA647B1E2264CB5DBC5025F81D2C28E10E12CDE5BF889F552033F8B1D2CB9175"
prefixes = ["Content/Rogue10m/Maps/Production/WorldLayoutCompact20261113A.umap",
 "Content/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompact20261113A",
 "Content/__ExternalObjects__/Rogue10m/Maps/Production/WorldLayoutCompact20261113A"]
selected = []
for prefix in prefixes:
 p = R / prefix
 assert p.exists(), prefix
 selected.extend([p] if p.is_file() else sorted(x for x in p.rglob("*") if x.is_file()))
E.mkdir(parents=True); B.mkdir(parents=True)
copies = []
for source in selected:
 pin = c.file_pin(source)
 destination = B / source.relative_to(R)
 destination.parent.mkdir(parents=True, exist_ok=True)
 assert not destination.exists()
 shutil.copy2(source, destination)
 assert c.file_pin(destination)["sha256"] == pin["sha256"]
 copies.append({"path":source.relative_to(R).as_posix(), "bytes":source.stat().st_size,"sha256":pin["sha256"]})
seal = c.read_json_pin({"path":"C:/Users/PC/AppData/Local/Temp/R10InternalSmoke20261113D/smoke-e5.sealed.json","sha256":"536066AEE494178F4814D629F0C695BBD2D38B99B8E1F562B869D046301308B7"})
saves = []
for item in seal["protected_saves"]:
 snapshot = c.scan_tree(item["root"], True)
 c.compare_inventory(c.read_json_pin(item["manifest"]),snapshot,"protected saves before intro authoring")
 name = "protected-save-" + str(len(saves)) + ".json"
 (E/name).write_text(json.dumps(snapshot,indent=2)+"\n",encoding="utf-8")
 saves.append({"root":item["root"],"manifest":name,"sha256":c.file_pin(E/name)["sha256"]})
status = subprocess.run(["git","status","--short"],cwd=R,capture_output=True,check=True).stdout
(E/"git-before.txt").write_bytes(status)
(E/"inputs-before.json").write_text(json.dumps(before,indent=2)+"\n",encoding="utf-8")
out = {"status":"CANDIDATE_PHYSICAL_CHECKPOINT_COMPLETE_NO_EDITOR_EXECUTION","time_utc":datetime.now(timezone.utc).isoformat(),"map":"/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A","input_files":len(before["files"]),"backup_root":str(B),"backup_files":len(copies),"backup_bytes":sum(x["bytes"] for x in copies),"files":copies,"protected_saves":saves,"content_mutated":False,"denied_metadata_or_clone_QA_executed":False}
(E/"physical-checkpoint.json").write_text(json.dumps(out,indent=2)+"\n",encoding="utf-8")
print(json.dumps({k:v for k,v in out.items() if k!="files"},indent=2))
