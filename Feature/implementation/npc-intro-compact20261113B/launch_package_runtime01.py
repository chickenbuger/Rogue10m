"""Launch only the fresh built bootstrap into an empty QA user root; no UI input."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib, json, subprocess, types

R = Path(__file__).resolve().parents[3]
E = R / "Feature/doc/evidence/npc-intro-compact20261113B"
P = R / "Feature/implementation/native51-internal-smoke-20261113B/strict_primitives.py"
raw = P.read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == "D9E27B5F44C8D52BDBB5ACA01459DA98378B47F67CE44A71720FB2EC93E38ACF"
c = types.ModuleType("package_probe_owner"); c.__file__ = str(P)
exec(compile(raw, str(P), "exec"), c.__dict__)
delivery = c.load(c.stable_bytes(E / "internal-windows-delivery.json"))
assert delivery["status"] == "NATIVE51_INTRO_INTERNAL_DEVELOPMENT_UNVERIFIED" and not delivery["release_eligible"]
c.read_pin(delivery["delivery_manifest"])
windows = c.physical(delivery["archive"], True)
assert c.digest(c.canonical(c.payload_manifest(windows))) == delivery["payload_sha256"]
c.read_pin(delivery["bootstrap"]); c.read_pin(delivery["game"])
owner = c.load(c.stable_bytes(E / "internal-windows-owner-result.json"))
s = c.read_json_pin(owner["seal"])
for protected in s["protected_saves"]:
    c.compare_inventory(c.read_json_pin(protected["manifest"]), c.scan_tree(protected["root"], True), "protected saves before packaged bootstrap")
probe = c.physical(Path(s["owner_area"]) / "PackageRuntime01")
assert not probe.exists() and not (E / "package-runtime01-launch.json").exists()
user = probe / "User"
user.mkdir(parents=True)
assert not (user / "Saved/SaveGames").exists()
log = E / "package-runtime01.log"
assert not log.exists()
args = [delivery["bootstrap"]["path"], "-windowed", "-ResX=1280", "-ResY=720", "-NoSplash", "-NoSound", "-NoLiveCoding", "-NoRemoteShaderCompile", "-DisablePython", "-dx12", "-culture=en", "-language=en", "-locale=en-US", "-UserDir=" + str(user), "-AbsLog=" + str(log), "-RogueProgressionSavePrefix=Rogue10m_Progression_QA_CI20261113N01", "-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_CI20261113N01", "-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False"]
args += ["-ExecCmds=getall GameEngine GameScreenshotSaveDirectory,GetIni Engine:PlatformFeatures SaveGameSystemModule,shot showui"]
stdout = (E / "package-runtime01-bootstrap.stdout.log").open("xb")
stderr = (E / "package-runtime01-bootstrap.stderr.log").open("xb")
try:
    started = datetime.now(timezone.utc)
    p = subprocess.Popen(args, cwd=windows, stdout=stdout, stderr=stderr, creationflags=subprocess.CREATE_NO_WINDOW)
finally:
    stdout.close(); stderr.close()
result = {"status": "REAL_PACKAGED_BOOTSTRAP_LAUNCHED_RUNTIME_AND_ROUTING_NOT_YET_OBSERVED", "started_utc": started.isoformat(), "bootstrap_pid": p.pid, "expected_child_game": delivery["game"], "bootstrap": delivery["bootstrap"], "args": args, "user_dir": str(user), "expected_saved_root": str(user / "Saved"), "expected_save_games": str(user / "Saved/SaveGames"), "profile_override": "NOT_PASSED; packaged Profile WITH_EDITOR override is unavailable; generic default slot requires actual root probe before profile creation", "profile_creation_authorized_by_this_step": False, "routing_probe": "NOT_OBSERVED", "GUI_input_by_this_step": 0, "release_eligible": False, "denied_startup_or_metadata_clone_QA": False}
with (E / "package-runtime01-launch.json").open("x", encoding="utf-8", newline="\n") as stream:
    json.dump(result, stream, indent=2); stream.write("\n")
print(json.dumps(result, indent=2))
