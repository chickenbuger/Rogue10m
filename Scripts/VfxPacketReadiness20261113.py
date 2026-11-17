"""Offline, read-only, exact-hash readiness check; never applies a packet.

Manifest: {"schema_version": 1, "entries": [{"path": "Scripts/example.py",
"base_sha256": null, "candidate_sha256": "<64 hex digits>"}]}. A null base
means the path must be absent. Hashes describe file bytes, not Git blobs.
Additional manifest metadata is allowed; entry paths use forward slashes.

Usage: python -B VfxPacketReadiness20261113.py --source-root PROJECT
       --manifest packet.json [--report NEW_JSON_OUTSIDE_PROJECT]
Exit codes: 0 ready, 2 blocked, 1 invalid input or operational error.
Reports are deterministic for unchanged inputs and exclusively create a new
file in an existing physical directory. No source files are opened for writing.
This is a point-in-time observation, not a filesystem lock or authorization to
integrate. Before/after identity checks detect observable changes during reads;
they cannot prove that an adversarial change was made and completely restored
between checks. Candidate hashes are supplied intent, not verified payloads.
Optional report creation also uses point-in-time parent checks, so it must not
be used in a directory whose ancestors another process is actively replacing.
"""

import argparse
import hashlib
import json
import ntpath
import os
from pathlib import Path
import re
import stat
import sys


STATUSES = ("already_integrated", "ready_add", "ready_update", "conflict",
            "missing_baseline", "unsafe_path", "unreadable")
READY = frozenset(STATUSES[:3])
HEX = re.compile(r"[0-9a-fA-F]{64}\Z")
RESERVED = re.compile(r"(?:CON|PRN|AUX|NUL|CONIN\$|CONOUT\$|COM[1-9\u00b9\u00b2\u00b3]|LPT[1-9\u00b9\u00b2\u00b3])(?:\.|$)", re.I)


class PacketError(ValueError):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


def _unsafe_relative(path):
    if not path or "\\" in path or ntpath.splitdrive(path)[0] or path.startswith("/"):
        return True
    for part in path.split("/"):
        if (part in ("", ".", "..") or part.endswith((" ", "."))
                or RESERVED.match(part)
                or any(ord(c) < 32 or 0xD800 <= ord(c) <= 0xDFFF for c in part)
                or any(c in '<>:"|?*' for c in part)):
            return True
    return False


def validate_manifest(manifest):
    if (not isinstance(manifest, dict) or type(manifest.get("schema_version")) is not int
            or manifest["schema_version"] != 1):
        raise PacketError("invalid_schema", "schema_version must be integer 1")
    entries = manifest.get("entries")
    if not isinstance(entries, list) or not entries:
        raise PacketError("invalid_schema", "entries must be a nonempty array")
    result, seen = [], set()
    for entry in entries:
        if (not isinstance(entry, dict) or not isinstance(entry.get("path"), str)
                or "base_sha256" not in entry
                or not isinstance(entry.get("candidate_sha256"), str)
                or not HEX.fullmatch(entry["candidate_sha256"])
                or (entry["base_sha256"] is not None
                    and (not isinstance(entry["base_sha256"], str)
                         or not HEX.fullmatch(entry["base_sha256"])))):
            raise PacketError("invalid_schema", "each entry needs path, base_sha256 (hash or null), and candidate_sha256")
        key = entry["path"].casefold()
        if key in seen:
            raise PacketError("invalid_schema", "duplicate paths (case insensitive) are ambiguous")
        seen.add(key)
        result.append({"path": entry["path"],
                       "base_sha256": entry["base_sha256"].lower() if entry["base_sha256"] else None,
                       "candidate_sha256": entry["candidate_sha256"].lower()})
    for key in seen:
        parts = key.split("/")
        if any("/".join(parts[:index]) in seen for index in range(1, len(parts))):
            raise PacketError("invalid_schema", "entry paths cannot be ancestors of other entry paths (case insensitive)")
    return sorted(result, key=lambda entry: (entry["path"].casefold(), entry["path"]))


def _is_reparse(info):
    return stat.S_ISLNK(info.st_mode) or bool(getattr(info, "st_file_attributes", 0) & 0x400)


def _identity(info):
    return (info.st_dev, info.st_ino, info.st_mode, getattr(info, "st_file_attributes", 0))


def _file_identity(info):
    return _identity(info) + (info.st_size, info.st_mtime_ns, info.st_ctime_ns, info.st_nlink)


def _walk(path):
    """lstat every absolute ancestor; never resolve through a known link."""
    chain = []
    parts = [*reversed(path.parents), path]
    for index, part in enumerate(parts):
        try:
            info = os.lstat(part)
        except FileNotFoundError:
            return chain, None
        if _is_reparse(info):
            raise PacketError("unsafe_path", "symlink or reparse point in path")
        if index < len(parts) - 1 and not stat.S_ISDIR(info.st_mode):
            raise OSError("ancestor is not a directory")
        chain.append((str(part), _identity(info)))
    return chain, info


def physical_source_root(source_root):
    root = Path(os.path.abspath(source_root))
    try:
        _, info = _walk(root)
        if info is None or not stat.S_ISDIR(info.st_mode):
            raise PacketError("invalid_source_root", "source root must be an existing physical directory")
    except OSError as exc:
        raise PacketError("invalid_source_root", "source root cannot be inspected") from exc
    return root


def _read_digest(handle):
    digest = hashlib.sha256()
    while True:
        chunk = handle.read(1024 * 1024)
        if not chunk:
            return digest.hexdigest()
        digest.update(chunk)


def _observe(path):
    before_chain, before = _walk(path)
    if before is None:
        after_chain, after = _walk(path)
        if before_chain != after_chain or after is not None:
            raise PacketError("changed_during_read", "path changed during inspection")
        return None
    if not stat.S_ISREG(before.st_mode):
        raise OSError("target is not a regular file")
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0)
    descriptor = os.open(path, flags)
    with os.fdopen(descriptor, "rb") as handle:
        opened = os.fstat(handle.fileno())
        if _is_reparse(opened) or _file_identity(before) != _file_identity(opened):
            raise PacketError("changed_during_read", "opened file identity changed")
        digest = _read_digest(handle)
        after_opened = os.fstat(handle.fileno())
        try:
            after_chain, after = _walk(path)
        except (OSError, PacketError) as exc:
            raise PacketError("changed_during_read", "path changed during hashing") from exc
        if (before_chain != after_chain or after is None
                or _file_identity(before) != _file_identity(after_opened)
                or _file_identity(before) != _file_identity(after)):
            raise PacketError("changed_during_read", "file or ancestor identity changed during hashing")
    return digest


def compare_packet(source_root, manifest):
    entries = validate_manifest(manifest)
    root = physical_source_root(source_root)
    root_chain, root_info = _walk(root)
    if root_info is None:
        raise PacketError("invalid_source_root", "source root disappeared before inspection")
    results = []
    for entry in entries:
        row = dict(entry, observed_sha256=None, uncertain=False, reason=None)
        if _unsafe_relative(entry["path"]):
            row.update(status="unsafe_path", reason="noncanonical_relative_path")
        else:
            try:
                observed = _observe(root.joinpath(*entry["path"].split("/")))
                row["observed_sha256"] = observed
                if observed == entry["candidate_sha256"]:
                    row["status"] = "already_integrated"
                elif observed is None:
                    row["status"] = "ready_add" if entry["base_sha256"] is None else "missing_baseline"
                elif entry["base_sha256"] == observed:
                    row["status"] = "ready_update"
                else:
                    row["status"] = "conflict"
            except PacketError as exc:
                row.update(status="unsafe_path" if exc.code == "unsafe_path" else "unreadable",
                           uncertain=exc.code == "changed_during_read", reason=exc.code)
            except OSError:
                # An I/O failure can occur after an earlier stat succeeded. Fail closed.
                row.update(status="unreadable", uncertain=True, reason="io_error")
        results.append(row)
    try:
        after_chain, after_root = _walk(root)
        root_changed = after_root is None or after_chain != root_chain
    except (OSError, PacketError):
        root_changed = True
    if root_changed:
        for row in results:
            if row["status"] in READY:
                row.update(status="unreadable", uncertain=True, observed_sha256=None,
                           reason="source_root_changed")
    ready = all(row["status"] in READY for row in results)
    return {"schema_version": 1, "inspection": "offline_read_only_point_in_time",
            "source_root": str(root), "status": "ready" if ready else "blocked", "ready": ready,
            "source_root_uncertain": root_changed,
            "counts": {status: sum(row["status"] == status for row in results) for status in STATUSES},
            "entries": results}


def _report_target(report_path, source_root):
    target = Path(os.path.abspath(report_path))
    try:
        inside = os.path.commonpath((os.path.normcase(target), os.path.normcase(source_root))) == os.path.normcase(source_root)
    except ValueError:  # Different physical drives are outside one another.
        inside = False
    if inside:
        raise PacketError("unsafe_report_path", "report must be outside the inspected source root")
    try:
        chain, info = _walk(target.parent)
        if info is None or not stat.S_ISDIR(info.st_mode):
            raise PacketError("unsafe_report_path", "report parent must be an existing physical directory")
        if os.path.lexists(target):
            raise PacketError("unsafe_report_path", "report target must be new (no overwrite or links)")
    except OSError as exc:
        raise PacketError("unsafe_report_path", "report destination cannot be inspected") from exc
    return target, chain


def _write_report(target, parent_chain, payload):
    # Exclusive creation also prevents truncating an outside hardlink to source.
    chain, _ = _walk(target.parent)
    if chain != parent_chain:
        raise PacketError("unsafe_report_path", "report parent changed before creation")
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
    with os.fdopen(os.open(target, flags, 0o600), "wb") as handle:
        chain, _ = _walk(target.parent)
        if chain != parent_chain:
            raise PacketError("unsafe_report_path", "report parent changed during creation")
        handle.write(payload.encode("utf-8"))


class _Parser(argparse.ArgumentParser):
    def error(self, message):
        raise PacketError("invalid_arguments", message)


def main(argv=None):
    try:
        parser = _Parser(description=__doc__)
        parser.add_argument("--source-root", required=True)
        parser.add_argument("--manifest", required=True)
        parser.add_argument("--report", help="exclusively create a new JSON file outside source root")
        args = parser.parse_args(argv)
        root = physical_source_root(args.source_root)
        target = _report_target(args.report, root) if args.report else None
        try:
            with open(args.manifest, "r", encoding="utf-8-sig") as handle:
                manifest = json.load(handle)
        except (OSError, ValueError) as exc:
            raise PacketError("invalid_manifest", "manifest must be readable schema-versioned JSON") from exc
        report = compare_packet(root, manifest)
        payload = json.dumps(report, indent=2, sort_keys=True, ensure_ascii=True) + "\n"
        if target:
            _write_report(*target, payload)
        sys.stdout.write(payload)
        return 0 if report["ready"] else 2
    except (PacketError, OSError) as exc:
        report = {"schema_version": 1, "status": "error", "ready": False,
                  "error": {"code": exc.code if isinstance(exc, PacketError) else "io_error",
                            "message": str(exc) if isinstance(exc, PacketError) else "operation could not complete"}}
        sys.stdout.write(json.dumps(report, indent=2, sort_keys=True) + "\n")
        return 1


if __name__ == "__main__":
    sys.exit(main())
