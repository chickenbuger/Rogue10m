"""Copy the verified new archive to a fresh physical local delivery, preserving E5."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib, json, shutil, types

R = Path(__file__).resolve().parents[3]
E = R / "Feature/doc/evidence/npc-intro-compact20261113B"
P = R / "Feature/implementation/native51-internal-smoke-cook-fix-f/strict_primitives.py"
raw = P.read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == "0CDDDF8F2C5F8C8894DD1D5B8992546E0993E392F479DAA8B551CC7D891D6E0C"
c = types.ModuleType("intro_delivery_owner"); c.__file__ = str(P)
exec(compile(raw, str(P), "exec"), c.__dict__)
owner = c.load(c.stable_bytes(E / "internal-windows-C-owner-result.json"))
assert owner["exit_code"] == 0 and owner["execute_requested"]
s = c.read_json_pin(owner["seal"])
ev = Path(s["outputs"]["evidence"])
post = c.load(c.stable_bytes(ev / "postflight.json"))
io = c.load(c.stable_bytes(E / "internal-C-iostore-final.json"))
assert post["status"] == "internal_smoke_artifact_built_UNVERIFIED"
assert io["validator_exit"] == 0 and io["identity_status"] == "iostore_csv_identity_only_UNVERIFIED"
assert len(io["required_packages"]) == 7 and io["payload_sha256"] == post["payload_sha256"]
source = c.physical(Path(s["outputs"]["archive"]) / "Windows", True)
target = c.physical(R / "tmp/InternalWindows20261113Native51IntroC_UNVERIFIED")
assert c.child(target, R / "tmp") and not target.exists()
payload = c.payload_manifest(source)
assert payload == post["payload"] and c.digest(c.canonical(payload)) == post["payload_sha256"]
windows = target / "Windows"
windows.mkdir(parents=True)
for f in payload:
    a = c.physical(source / f["path"], True)
    b = c.physical(windows / f["path"])
    assert c.child(a, source) and c.child(b, windows) and not b.exists()
    b.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(a, b)
    assert c.file_pin(b)["sha256"] == f["sha256"] and b.stat().st_size == f["size"]
assert c.payload_manifest(windows) == payload
delivery = {"status": "NATIVE51_INTRO_INTERNAL_DEVELOPMENT_UNVERIFIED", "created_utc": datetime.now(timezone.utc).isoformat(), "source_archive": str(source), "archive": str(windows), "files": len(payload), "payload_bytes": sum(x["size"] for x in payload), "payload_sha256": post["payload_sha256"], "payload": payload, "bootstrap": c.file_pin(windows / "Rogue10m.exe"), "game": c.file_pin(windows / "Rogue10m/Binaries/Win64/Rogue10m.exe"), "postflight": c.file_pin(ev / "postflight.json"), "iostore": c.file_pin(E / "internal-C-iostore-final.json"), "content_state": s["content_state"], "release_eligible": False, "actual_package_execution_at_delivery": 0, "UI7": "UNVERIFIED", "old_E5_contains_NPC_fix": False, "old_E5_modified": False, "denied_startup_metadata_clone_QA_operations": 0}
with (target / "DELIVERY.json").open("x", encoding="utf-8", newline="\n") as stream:
    json.dump(delivery, stream, ensure_ascii=False, indent=2); stream.write("\n")
(target / "README.ko.md").write_text("# NPC 수정·초급 의뢰 포함 내부 Windows 빌드\n\nWin64 Development 내부 미검수 버전이다. Windows/Rogue10m.exe가 패키지 진입점이다. Source의 처치 귀속 수정과 신규 IntroSupply DA가 포함됐으며 6맵+DA의 IoStore identity를 확인했다. 기존 E5를 교체하지 않았다.\n\n맵은1800×1800m이며 미축소다. 기본 메뉴/Hub와 기존 포털은 그대로이고 Compact 전용 일반 포털은 없다. 신규 의뢰는 Compact 후보에만 있다. 월드 한글 표식·기존 플레이어 재질 문제, UI7·물리 주행·15분 성장·완주·실제 패키지 저장/재시작 검증은 남아 있다. DELIVERY.json의 content_state와 검증 보고서를 함께 확인한다. 릴리스 완료품이 아니다.\n", encoding="utf-8")
summary = {k: v for k, v in delivery.items() if k != "payload"}
summary["delivery_manifest"] = c.file_pin(target / "DELIVERY.json")
with (E / "internal-windows-C-delivery.json").open("x", encoding="utf-8", newline="\n") as stream:
    json.dump(summary, stream, ensure_ascii=False, indent=2); stream.write("\n")
print(json.dumps(summary, ensure_ascii=False, indent=2))
