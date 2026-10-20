"""Call the actual upstream stdio MCP server, serialized against live Blender."""
import argparse
import asyncio
import json
import os
from pathlib import Path

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

ROOT = Path(__file__).resolve().parents[1]
PYTHON = ROOT / "tmp/blender-mcp-venv/Scripts/python.exe"


async def run(args):
    if args.out:
        Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env.update(BLENDER_MCP_MODE="addon", BLENDER_ADDON_HOST="127.0.0.1",
               BLENDER_ADDON_PORT="9877", BLENDER_TIMEOUT_SECONDS="300")
    params = StdioServerParameters(command=str(PYTHON),
        args=["-m", "blender_codex_mcp.server"], env=env)
    async with stdio_client(params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            tool_args = json.loads(args.args)
            if args.script:
                script = str(Path(args.script).resolve())
                # Upstream's exec separates globals/locals: use an inner shared namespace.
                tool_args = {"python_code":
                    "ns = {\"bpy\": bpy, \"__name__\": \"__main__\", \"__file__\": " + repr(script) + "}\n"
                    "exec(compile(open(" + repr(script) + ", encoding='utf-8').read(), " + repr(script) + ", 'exec'), ns, ns)\n"
                    "result = ns.get('result')"}
            if args.tool == "list_tools":
                data = (await session.list_tools()).model_dump(mode="json")
            else:
                response = await session.call_tool(args.tool, tool_args)
                data = response.model_dump(mode="json")
                payload = response.structuredContent
                if payload is None:
                    for content in response.content:
                        if content.type == "text":
                            try:
                                payload = json.loads(content.text)
                            except ValueError:
                                continue
                            break
                if response.isError or (isinstance(payload, dict) and payload.get("ok") is False):
                    if args.out:
                        Path(args.out).write_text(json.dumps(data, indent=2), encoding="utf-8")
                    raise RuntimeError(json.dumps(payload or data))
                data = {"tool": args.tool, "payload": payload}
            if args.out:
                Path(args.out).parent.mkdir(parents=True, exist_ok=True)
                Path(args.out).write_text(json.dumps(data, indent=2), encoding="utf-8")
            print(json.dumps(data, ensure_ascii=False))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", default="blender_ping")
    parser.add_argument("--args", default="{}")
    parser.add_argument("--script")
    parser.add_argument("--out")
    asyncio.run(run(parser.parse_args()))
