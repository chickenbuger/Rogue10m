"""Fictional offline contracts only: no UE, build, game or GUI is launched."""
import contextlib
import copy
from datetime import datetime, timedelta, timezone
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

import internal_smoke as m


class SmokeTests(unittest.TestCase):
    def setUp(self):
        self.c = m.helpers()
        scratch = m.HERE / "scratch"; scratch.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="smoke-synthetic-", dir=scratch)
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)

    def write(self, name, value):
        p = self.base / name; p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(value if isinstance(value, bytes) else json.dumps(value).encode())
        return self.c.file_pin(p)

    def test_primitives_are_exact_previous_bytes(self):
        self.assertEqual(self.c.file_pin(m.HERE / "strict_primitives.py")["sha256"], m.HELPER_SHA)

    def test_unsealed_never_emits_executable_arguments(self):
        old_argv = sys.argv
        sys.argv = ["internal_smoke.py", "--seal", str(m.HERE / "smoke.UNSEALED.json"), "--seal-sha256", self.c.file_pin(m.HERE / "smoke.UNSEALED.json")["sha256"], "--plan"]
        out = io.StringIO()
        try:
            with contextlib.redirect_stdout(out): code = m.main()
        finally:
            sys.argv = old_argv
        data = json.loads(out.getvalue())
        self.assertEqual(code, 2); self.assertFalse(data["executed"])
        self.assertNotIn("bcr_args", data); self.assertEqual(data["acceptance_flags"], m.FLAGS)
        self.assertEqual(data["known_limitations"]["map_extent_m"], [1800, 1800])

    def test_release_or_ui_upgrade_is_never_carried_forward(self):
        value = m.stamp({"release_eligible": True, "acceptance_flags": {"UI7": "passed"}, "known_limitations": {"map_crop": "done"}})
        self.assertFalse(value["release_eligible"]); self.assertEqual(value["acceptance_flags"]["UI7"], "UNVERIFIED")
        self.assertEqual(value["known_limitations"], m.LIMITS)

    def test_strict_seal_cannot_enter_smoke_contract(self):
        pin = self.write("strict.json", {"schema": "R10-candidate-windows-seal-v1", "status": "sealed"})
        with self.assertRaisesRegex(ValueError, "UNSEALED"):
            m.validate(self.c, pin, {}, "owner")

    def test_mutated_ui_flag_is_rejected_before_any_inputs(self):
        s = m.stamp({"schema": "R10-internal-smoke-seal-v1", "status": "sealed"})
        s["acceptance_flags"]["UI7"] = "PASSED"
        with self.assertRaisesRegex(ValueError, "cannot be upgraded"):
            m.validate(self.c, self.write("badflags.json", s), {}, "owner")

    def test_source_added_or_changed_detected_by_reused_exact_inventory(self):
        root = self.base / "project"
        for d in ("Source", "Config", "Content"): (root / d).mkdir(parents=True, exist_ok=True)
        self.write("project/Rogue10m.uproject", b"{}")
        old = self.c.input_inventory(root)
        self.write("project/Source/AfterFreeze.cpp", b"SYNTHETIC")
        with self.assertRaisesRegex(ValueError, "changed"):
            self.c.compare_inventory(old, self.c.input_inventory(root), "smoke inputs")

    def test_recipe_game_cook_iostore_archive_and_exact_map_are_fixed(self):
        root = self.base / "project"
        s = {"root": str(root), "project": str(root / "Rogue10m.uproject"), "engine": str(self.base / "engine"),
             "outputs": {k: str(self.base / k) for k in self.c.OUTPUTS}, "output_root": str(self.base),
             "run_tag": "SmokeTest", "runtime": {"python": {"path": "fake", "sha256": "A" * 64}}, "tool_pins": [],
             "cook_maps": ["/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A"], "active_destination": "/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A",
             "prior_game_products": []}
        p = m.plan(self.c, s, "A" * 64, "B" * 64, {"count": 43, "executed_here": False})
        for flag in ("-build", "-cook", "-stage", "-pak", "-iostore", "-package", "-archive", "-nocompileeditor"):
            self.assertIn(flag, p["bcr_args"])
        self.assertIn("-map=" + s["active_destination"], p["bcr_args"])
        self.assertEqual(p["schema"], "R10-internal-smoke-plan-v1"); self.assertFalse(p["release_eligible"])
        self.assertEqual(p["native43_save_isolation"], "NOT_ESTABLISHED_BY_RETAINED_TESTS")
        self.assertNotIn("-game", p["bcr_args"]); self.assertNotIn("-run", p["bcr_args"])

    def test_smoke_release_exact_scope_and_project_write_authority(self):
        now = datetime.now(timezone.utc)
        s = {"project": str(self.base / "Game.uproject"), "run_tag": "SmokeTest", "active_destination": "/Game/Map",
             "runtime": {"powershell": {"path": str(self.base / "pwsh.exe"), "sha256": "C" * 64}}}
        r = {"schema": "R10-internal-smoke-release-v1", "status": "released_for_internal_smoke", "seal_sha256": "A" * 64,
             "project": s["project"], "run_tag": s["run_tag"], "active_destination": s["active_destination"], "owner_id": "owner",
             "scopes": ["Win64 Development internal smoke"], "project_build_outputs_authorized": True, "gui_forbidden": True,
             "release_eligible": False, "released_utc": (now-timedelta(seconds=1)).isoformat(), "expires_utc": (now+timedelta(minutes=1)).isoformat(),
             "owner_process": {"pid": 1, "start_utc_ticks": 1, "executable": s["runtime"]["powershell"]["path"], "sha256": "C" * 64}}
        m.release(self.c, s, "A" * 64, self.write("release.json", r), "owner")
        for field, bad in (("schema", "R10-candidate-execution-release-v1"), ("project_build_outputs_authorized", False),
                           ("gui_forbidden", False), ("release_eligible", True), ("scopes", ["Win64 Development packaging"])):
            changed = {**r, field: bad}
            with self.subTest(field=field), self.assertRaises(ValueError):
                m.release(self.c, s, "A" * 64, self.write("release-bad.json", changed), "owner")

    def test_csv_identity_missing_compact_or_bulk_only_fails(self):
        package = "/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A"
        raw = f'PackageName, Filename, ChunkType\n{package}, ../../../Rogue10m/Content/{package[6:]}.umap, ExportBundleData\n'
        self.c.validate_iostore(raw.encode(), [package])
        for value in (raw.replace("ExportBundleData", "BulkData"), raw.replace("Compact20261113A", "Compact20261113AOld")):
            with self.assertRaises(ValueError): self.c.validate_iostore(value.encode(), [package])

    def test_validator_contains_no_process_or_gui_api(self):
        import ast
        tree = ast.parse((m.HERE / "internal_smoke.py").read_text(encoding="utf-8"))
        for node in ast.walk(tree):
            if isinstance(node, ast.Import): self.assertFalse({n.name for n in node.names} & {"subprocess", "pyautogui", "pywinauto"})
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute): self.assertNotIn(node.func.attr, {"Popen", "run", "system", "startfile", "kill", "terminate"})


if __name__ == "__main__": unittest.main(verbosity=2)
