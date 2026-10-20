"""Execute Python source in the currently open Unreal Editor through Remote Execution."""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

REMOTE_EXECUTION_DIR = Path(
    r"D:\Program Files\UE_5.8\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python"
)


def main() -> int:
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--file", type=Path)
    group.add_argument("--command")
    parser.add_argument("--timeout", type=float, default=8.0)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()

    sys.path.insert(0, str(REMOTE_EXECUTION_DIR))
    import remote_execution

    command_text = (
        args.file.resolve().read_text(encoding="utf-8")
        if args.file
        else args.command
    )
    session = remote_execution.RemoteExecution()
    session.start()
    try:
        deadline = time.monotonic() + args.timeout
        expected_root = args.project_root.resolve().as_posix().rstrip('/').casefold()
        def matching_nodes():
            return [node for node in session.remote_nodes
                    if str(node.get('project_root', '')).replace('\\', '/').rstrip('/').casefold() == expected_root]
        while not matching_nodes() and time.monotonic() < deadline:
            time.sleep(0.1)
        nodes = matching_nodes()
        if len(nodes) != 1:
            print(f"Expected exactly one Unreal Remote Execution node for {expected_root}; found {len(nodes)}.", file=sys.stderr)
            return 2
        node = nodes[0]
        session.open_command_connection(node["node_id"])
        result = session.run_command(
            command_text,
            unattended=True,
            exec_mode=remote_execution.MODE_EXEC_FILE,
            raise_on_failure=False,
        )
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0 if result.get("success") else 1
    finally:
        session.stop()


if __name__ == "__main__":
    raise SystemExit(main())
