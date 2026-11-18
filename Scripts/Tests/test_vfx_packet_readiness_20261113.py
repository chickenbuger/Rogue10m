"""Temporary-directory tests; no Unreal, shared project, network, or Git access."""

import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import VfxPacketReadiness20261113 as guard


def sha(data):
    return hashlib.sha256(data).hexdigest()


def entry(path, base=b"base", candidate=b"candidate"):
    return {"path": path, "base_sha256": sha(base) if base is not None else None,
            "candidate_sha256": sha(candidate)}


def packet(*entries):
    return {"schema_version": 1, "entries": list(entries)}


class PacketReadinessTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="vfx-packet-readiness-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.source = self.work / "source"
        self.source.mkdir()

    def write(self, name, content):
        target = self.source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)
        return target

    def cli(self, manifest, *args):
        path = self.work / "packet.json"
        path.write_text(json.dumps(manifest), encoding="utf-8")
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            code = guard.main(["--source-root", str(self.source), "--manifest", str(path), *map(str, args)])
        return code, json.loads(output.getvalue()), output.getvalue()

    def junction(self, target, link):
        if os.name == "nt":
            if os.environ.get("VFX_PACKET_NATIVE_LINK_TESTS") != "1":
                self.skipTest("native junction fixture opt-in disabled; this environment returned WinError 5; injected lstat cases run")
            import _winapi
            try:
                _winapi.CreateJunction(str(target), str(link))
            except PermissionError:
                self.skipTest("environment denies native junction creation; injected lstat cases still run")
            self.addCleanup(lambda: os.rmdir(link) if os.path.lexists(link) else None)
        else:
            link.symlink_to(target, target_is_directory=True)
            self.addCleanup(lambda: link.unlink() if link.is_symlink() else None)

    def reparse_stat(self, target, symlink=False):
        """Inject OS metadata at a real path, retaining real stat values elsewhere."""
        original = os.lstat
        def inspect(path, *args, **kwargs):
            info = original(path, *args, **kwargs)
            if Path(path) == target:
                return SimpleNamespace(
                    st_dev=info.st_dev, st_ino=info.st_ino,
                    st_mode=stat.S_IFLNK if symlink else info.st_mode,
                    st_file_attributes=getattr(info, "st_file_attributes", 0) | 0x400,
                    st_size=info.st_size, st_mtime_ns=info.st_mtime_ns,
                    st_ctime_ns=info.st_ctime_ns, st_nlink=info.st_nlink)
            return info
        return mock.patch.object(guard.os, "lstat", side_effect=inspect)

    def test_ready_states_and_exact_candidate_takes_precedence(self):
        self.write("nested/update.txt", b"base")
        self.write("integrated.txt", b"candidate")
        self.write("same.txt", b"candidate")
        code, report, _ = self.cli(packet(entry("nested/update.txt"), entry("integrated.txt"),
                                          entry("missing/deep/add.txt", None),
                                          entry("same.txt", b"candidate")))
        self.assertEqual(code, 0)
        self.assertTrue(report["ready"])
        self.assertEqual(report["status"], "ready")
        self.assertEqual({r["path"]: r["status"] for r in report["entries"]}, {
            "integrated.txt": "already_integrated", "nested/update.txt": "ready_update",
            "missing/deep/add.txt": "ready_add", "same.txt": "already_integrated"})
        self.assertFalse((self.source / "missing").exists())

    def test_conflict_missing_baseline_and_unreadable_block_whole_packet(self):
        self.write("changed.txt", b"user edits")
        self.write("unexpected-add.txt", b"user file")
        (self.source / "directory.txt").mkdir()
        code, report, _ = self.cli(packet(entry("absent.txt"), entry("changed.txt"),
                                          entry("directory.txt"), entry("unexpected-add.txt", None)))
        self.assertEqual(code, 2)
        self.assertFalse(report["ready"])
        self.assertEqual([r["status"] for r in report["entries"]],
                         ["missing_baseline", "conflict", "unreadable", "conflict"])
        self.assertEqual(report["counts"]["ready_add"], 0)

    def test_file_ancestor_is_unreadable_not_a_ready_add(self):
        self.write("file", b"base")
        report = guard.compare_packet(self.source, packet(entry("file/child", None)))
        self.assertEqual(report["entries"][0]["status"], "unreadable")

    def test_path_rejections_do_not_open_targets(self):
        paths = ["", "/absolute", "//server/share", "\\\\server\\share", "C:/drive", "C:relative",
                 "a/../b", "../escape", "a\\..\\b", "./a", "a//b", "a/", "a:b", "CON",
                 "NUL.txt", "LPT9.log", "COM\u00b9", "CONIN$", "a.", "a ", "a/\x00b", "wild*", "question?", 'a"b', "a\ud800"]
        for path in paths:
            with self.subTest(path=path), mock.patch.object(guard, "_observe") as observe:
                report = guard.compare_packet(self.source, packet(entry(path, None)))
                self.assertEqual(report["entries"][0]["status"], "unsafe_path")
                self.assertFalse(report["ready"])
                observe.assert_not_called()

    def test_manifest_rejects_ambiguous_or_invalid_schema(self):
        good = entry("ok.txt")
        variants = [None, [], {}, {"schema_version": True, "entries": [good]},
                    {"schema_version": 2, "entries": [good]}, packet(),
                    packet(good, dict(good)), packet(good, dict(good, path="OK.TXT")),
                    packet({"path": "ok.txt", "candidate_sha256": sha(b"candidate")}),
                    packet(dict(good, base_sha256="wrong")), packet(dict(good, candidate_sha256=None)),
                    packet(dict(good, candidate_sha256="x" * 64)), packet(dict(good, path=123))]
        for manifest in variants:
            with self.subTest(manifest=manifest):
                code, report, _ = self.cli(manifest)
                self.assertEqual(code, 1)
                self.assertEqual(report["error"]["code"], "invalid_schema")

    def test_uppercase_hashes_are_normalized_and_metadata_allowed(self):
        self.write("file.txt", b"candidate")
        manifest = packet(dict(entry("file.txt"), candidate_sha256=sha(b"candidate").upper()))
        manifest["packet_id"] = "generic-packet"
        report = guard.compare_packet(self.source, manifest)
        self.assertTrue(report["ready"])
        self.assertEqual(report["entries"][0]["candidate_sha256"], sha(b"candidate"))

    def test_manifest_rejects_file_ancestor_collisions_in_either_order_and_case(self):
        for parent, child in (("new", "new/child.py"), ("New", "nEW/Child.py"),
                              ("dir/New", "DIR/new/deep/child.py")):
            for paths in ((parent, child), (child, parent)):
                with self.subTest(paths=paths), mock.patch.object(guard, "_observe") as observe:
                    code, report, _ = self.cli(packet(*(entry(path, None) for path in paths)))
                    self.assertEqual(code, 1)
                    self.assertEqual(report["error"]["code"], "invalid_schema")
                    observe.assert_not_called()
        code, report, _ = self.cli(packet(entry("new", None), entry("newer/child.py", None)))
        self.assertEqual(code, 0)
        self.assertTrue(report["ready"])

    def test_report_is_deterministic_and_entry_order_is_canonical(self):
        first = packet(entry("z.txt", None), entry("a.txt", None))
        second = packet(*reversed(first["entries"]))
        _, _, a = self.cli(first)
        _, _, b = self.cli(second)
        self.assertEqual(a, b)

    def test_permission_failure_is_reported_without_escaping(self):
        self.write("private.txt", b"base")
        with mock.patch.object(guard.os, "open", side_effect=PermissionError("fixture denial")):
            report = guard.compare_packet(self.source, packet(entry("private.txt")))
        self.assertEqual(report["entries"][0]["status"], "unreadable")
        self.assertTrue(report["entries"][0]["uncertain"])

    def test_edit_during_hash_is_uncertain_even_when_bytes_have_same_length(self):
        path = self.write("changing.txt", b"base")
        original_read = guard._read_digest
        def mutate(handle):
            digest = original_read(handle)
            path.write_bytes(b"edit")
            return digest
        with mock.patch.object(guard, "_read_digest", side_effect=mutate):
            report = guard.compare_packet(self.source, packet(entry("changing.txt")))
        row = report["entries"][0]
        self.assertEqual(row["status"], "unreadable")
        self.assertEqual(row["reason"], "changed_during_read")
        self.assertTrue(row["uncertain"])
        self.assertIsNone(row["observed_sha256"])

    def test_disappearing_source_root_cannot_report_ready_add(self):
        original_observe = guard._observe
        def remove_root(path):
            self.source.rmdir()
            return original_observe(path)
        with mock.patch.object(guard, "_observe", side_effect=remove_root):
            report = guard.compare_packet(self.source, packet(entry("new.txt", None)))
        self.assertFalse(report["ready"])
        self.assertTrue(report["source_root_uncertain"])
        self.assertEqual(report["entries"][0]["reason"], "source_root_changed")

    def test_replacement_during_hash_is_detected_by_identity(self):
        path = self.write("replaced.txt", b"base")
        replacement = self.write("replacement.tmp", b"base")
        original_read = guard._read_digest
        def replace(handle):
            digest = original_read(handle)
            # Windows may refuse replacement while the handle is open. Either
            # replacement or an I/O failure must block readiness.
            os.replace(replacement, path)
            return digest
        with mock.patch.object(guard, "_read_digest", side_effect=replace):
            report = guard.compare_packet(self.source, packet(entry("replaced.txt")))
        self.assertEqual(report["entries"][0]["status"], "unreadable")
        self.assertTrue(report["entries"][0]["uncertain"])

    def test_native_junction_in_entry_is_rejected_without_hashing(self):
        outside = self.work / "outside"
        outside.mkdir()
        (outside / "file.txt").write_bytes(b"base")
        self.junction(outside, self.source / "linked")
        with mock.patch.object(guard, "_read_digest") as read:
            report = guard.compare_packet(self.source, packet(entry("linked/file.txt")))
        self.assertEqual(report["entries"][0]["status"], "unsafe_path")
        read.assert_not_called()

    def test_reparse_metadata_on_leaf_or_ancestor_prevents_read(self):
        leaf = self.write("nested/file.txt", b"base")
        for target, symlink in ((leaf.parent, False), (leaf, False), (leaf, True)):
            with self.subTest(target=target, symlink=symlink), self.reparse_stat(target, symlink), mock.patch.object(guard, "_read_digest") as read:
                report = guard.compare_packet(self.source, packet(entry("nested/file.txt")))
                self.assertEqual(report["entries"][0]["status"], "unsafe_path")
                read.assert_not_called()

    def test_reparse_metadata_in_source_root_ancestry_prevents_inspection(self):
        for target in (self.source, self.work):
            with self.subTest(target=target), self.reparse_stat(target), self.assertRaises(guard.PacketError):
                guard.compare_packet(self.source, packet(entry("file.txt", None)))

    def test_reparse_metadata_in_report_parent_prevents_source_write(self):
        outside = self.work / "report-parent"
        outside.mkdir()
        with self.reparse_stat(outside):
            code, report, _ = self.cli(packet(entry("add.txt", None)), "--report", outside / "report.json")
        self.assertEqual(code, 1)
        self.assertEqual(report["status"], "error")
        self.assertFalse((outside / "report.json").exists())

    def test_native_junction_source_root_or_ancestor_is_rejected(self):
        link = self.work / "linked-source"
        self.junction(self.source, link)
        (self.source / "nested").mkdir()
        for root in (link, link / "nested"):
            with self.subTest(root=root), self.assertRaises(guard.PacketError):
                guard.compare_packet(root, packet(entry("file.txt", None)))

    def test_native_junction_report_parent_is_rejected(self):
        self.junction(self.source, self.work / "report-link")
        code, report, _ = self.cli(packet(entry("add.txt", None)), "--report", self.work / "report-link" / "report.json")
        self.assertEqual(code, 1)
        self.assertEqual(report["status"], "error")
        self.assertFalse((self.source / "report.json").exists())

    def test_report_in_source_or_nested_absent_directory_is_rejected(self):
        for target in (self.source / "report.json", self.source / "missing" / "report.json", self.source):
            with self.subTest(target=target):
                code, report, _ = self.cli(packet(entry("add.txt", None)), "--report", target)
                self.assertEqual(code, 1)
                self.assertEqual(report["error"]["code"], "unsafe_report_path")
        self.assertEqual(list(self.source.iterdir()), [])

    def test_report_outside_source_is_exact_stdout_and_source_is_unchanged(self):
        self.write("file.txt", b"base")
        before = {p.relative_to(self.source).as_posix(): p.read_bytes() for p in self.source.rglob("*") if p.is_file()}
        # A prefix-sharing sibling is outside source, not a containment match.
        sibling = self.work / "source-reports"
        sibling.mkdir()
        target = sibling / "readiness.json"
        code, report, output = self.cli(packet(entry("file.txt"), entry("new.txt", None)), "--report", target)
        self.assertEqual(code, 0)
        self.assertTrue(report["ready"])
        self.assertEqual(target.read_bytes(), output.encode("utf-8"))
        after = {p.relative_to(self.source).as_posix(): p.read_bytes() for p in self.source.rglob("*") if p.is_file()}
        self.assertEqual(before, after)

    def test_existing_report_or_hardlink_is_never_overwritten(self):
        source = self.write("file.txt", b"base")
        regular = self.work / "existing.json"
        regular.write_bytes(b"preserve report")
        hardlink = self.work / "hardlink.json"
        os.link(source, hardlink)
        for target in (regular, hardlink):
            with self.subTest(target=target):
                code, _, _ = self.cli(packet(entry("file.txt")), "--report", target)
                self.assertEqual(code, 1)
        self.assertEqual(source.read_bytes(), b"base")
        self.assertEqual(regular.read_bytes(), b"preserve report")

    def test_absent_report_parent_is_not_created(self):
        parent = self.work / "absent"
        code, _, _ = self.cli(packet(entry("file.txt", None)), "--report", parent / "report.json")
        self.assertEqual(code, 1)
        self.assertFalse(parent.exists())

    def test_bad_json_missing_source_and_bad_arguments_return_error_one(self):
        manifest = self.work / "bad.json"
        manifest.write_text("{broken", encoding="utf-8")
        cases = [["--source-root", str(self.source), "--manifest", str(manifest)],
                 ["--source-root", str(self.work / "missing"), "--manifest", str(manifest)], []]
        for args in cases:
            output = io.StringIO()
            with self.subTest(args=args), contextlib.redirect_stdout(output):
                self.assertEqual(guard.main(args), 1)
                self.assertEqual(json.loads(output.getvalue())["status"], "error")


if __name__ == "__main__":
    unittest.main()
