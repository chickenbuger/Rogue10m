"""Read Unreal output logs without suppressing or changing their contents.

Examples: python -B Scripts/CheckUnrealOutputLog.py --log path/to/editor.log
          python -B Scripts/CheckUnrealOutputLog.py --watch --duration-seconds 60
Exit codes: 0=no errors in selected interval, 1=errors, 2=input/I/O failure.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import re
import sys
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
STAMP = r"\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3}"
PREFIX = re.compile(r"^(?:\[(?P<time>" + STAMP + r")\]\[\s*\d+\])?"
                    r"(?P<category>\w+):\s*(?:(?P<severity>Fatal|Error|Warning|Display|Log|Verbose|VeryVerbose):\s*)?(?P<message>.*)$")
SIGNATURE = re.compile(rb"(?m)^(?:\[\d{4}[^\r\n]*?\]\[\s*\d+\])?LogInit:|^Log file open")
CRITICAL = re.compile(r"\b(?:Fatal error:|Ensure condition failed:|Blueprint Runtime Error:)", re.I)


def utc(value):
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo is None or parsed.utcoffset().total_seconds() != 0:
        raise ValueError("--since must include UTC Z or +00:00")
    return parsed.astimezone(timezone.utc)


def discover(root=ROOT):
    paths = [p for base in (root / "Saved/Logs", root / "Feature/doc/evidence")
             if base.exists() for p in base.rglob("*.log") if p.is_file()]
    for path in sorted(paths, key=lambda p: p.stat().st_mtime_ns, reverse=True):
        with path.open("rb") as stream:
            if SIGNATURE.search(stream.read(65536)):
                return path.resolve()
    raise FileNotFoundError("No Unreal session log found; supply --log explicitly")


def decode(data):
    return data.decode("utf-16" if data.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig", errors="replace")


def scan(data, path):
    timestamp = None
    for line, text in enumerate(decode(data).splitlines(), 1):
        match = PREFIX.match(text)
        if match:
            category, severity, message = match["category"], match["severity"] or "Log", match["message"]
            if match["time"]:
                timestamp = datetime.strptime(match["time"], "%Y.%m.%d-%H.%M.%S:%f").replace(tzinfo=timezone.utc)
        elif CRITICAL.match(text):
            category, severity, message = "Uncategorized", "Error", text
        else:
            continue
        critical = CRITICAL.search(message)
        implicit_load_error = match and category == "LoadErrors" and match["severity"] is None
        if severity not in ("Fatal", "Error") and not implicit_load_error and not critical:
            continue
        if critical and "Ensure" in critical.group():
            severity = "Ensure"
        elif critical and "Fatal" in critical.group():
            severity = "Fatal"
        elif severity not in ("Fatal", "Error"):
            severity = "Error"
        yield {"path": str(path), "line": line, "category": category, "severity": severity,
               "message": message, "timestamp_utc": timestamp.isoformat() if timestamp else None}


def inspect(path, *, after_line=None, since=None, watch=False, duration=60, poll=1):
    if ((after_line is not None and after_line < 0) or not math.isfinite(duration)
            or not math.isfinite(poll) or duration <= 0 or duration > 3600 or poll <= 0):
        raise ValueError("after-line >= 0, 0 < duration-seconds <= 3600, poll-seconds > 0 required")
    path = path.resolve()
    first = path.read_bytes()
    if watch and after_line is None and since is None:
        initial_text = decode(first)
        after_line = len(initial_text.splitlines()) - int(bool(initial_text) and not initial_text.endswith(("\r", "\n")))
    after_line = after_line or 0
    report = {"path": str(path), "after_line": after_line, "since_utc": since.isoformat() if since else None,
              "watch": watch, "existing_errors": [], "new_errors": [], "events": []}
    seen, previous, identity, generation = {}, None, None, 0
    deadline = time.monotonic() + duration
    while True:
        stat = path.stat()
        data = first if previous is None else path.read_bytes()
        current_id = (stat.st_dev, stat.st_ino)
        if previous is not None and (current_id != identity or not data.startswith(previous)):
            generation += 1
            after_line = 0
            report["events"].append({"kind": "log_replaced_or_truncated", "generation": generation,
                                     "message": "Preserved prior errors; rescanning replacement from line 1"})
        for issue in scan(data, path):
            key = (generation, issue["line"])
            if key in seen:
                seen[key].update(issue)
                continue
            # Untimed errors cannot safely be declared older than a time boundary.
            selected = issue["line"] > after_line and (since is None or issue["timestamp_utc"] is None
                                                        or datetime.fromisoformat(issue["timestamp_utc"]) >= since)
            issue["generation"] = generation
            seen[key] = issue
            report["new_errors" if selected else "existing_errors"].append(issue)
        previous, identity = data, current_id
        if not watch or time.monotonic() >= deadline:
            break
        time.sleep(min(poll, max(0, deadline - time.monotonic())))
    report["existing_error_count"] = len(report["existing_errors"])
    report["new_error_count"] = len(report["new_errors"])
    report["total_error_count"] = report["existing_error_count"] + report["new_error_count"]
    report["scanned_line_count"] = len(decode(previous).splitlines())
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path)
    parser.add_argument("--after-line", type=int, help="Only lines strictly after this 1-based boundary fail the check; explicit 0 includes all lines")
    parser.add_argument("--since", type=utc, help="UTC ISO-8601 inclusive boundary, e.g. 2026-11-16T12:00:00Z")
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--watch", action="store_true")
    parser.add_argument("--duration-seconds", type=float, default=60)
    parser.add_argument("--poll-seconds", type=float, default=1)
    args = parser.parse_args(argv)
    try:
        path = args.log.resolve() if args.log else discover()
        if args.json_output and (args.json_output.resolve() == path
                                 or (args.json_output.exists() and args.json_output.samefile(path))):
            raise ValueError("JSON output must not overwrite the input log")
        if args.json_output and args.json_output.suffix.lower() != ".json":
            raise ValueError("--json-output requires a .json destination")
        report = inspect(path, after_line=args.after_line, since=args.since, watch=args.watch,
                         duration=args.duration_seconds, poll=args.poll_seconds)
        if args.json_output:
            args.json_output.parent.mkdir(parents=True, exist_ok=True)
            args.json_output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"Log: {path}\nExisting errors: {report['existing_error_count']}; selected/new errors: {report['new_error_count']}")
        for event in report["events"]:
            print("Warning: " + event["message"])
        for issue in report["new_errors"]:
            print(f"{issue['line']}: {issue['category']} {issue['severity']}: {issue['message']}")
        return int(report["new_error_count"] > 0)
    except (OSError, ValueError) as exc:
        print(f"Log check failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
