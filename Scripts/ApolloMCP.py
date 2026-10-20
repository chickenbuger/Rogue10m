"""Call the unmodified Apollo stdio MCP server and preserve its actual result."""
import argparse
import asyncio
import json
import os
from pathlib import Path
import sys

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'Feature/doc/evidence/arin-apollo-20261005'


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--tool', required=True)
    parser.add_argument('--arguments', type=Path)
    parser.add_argument('--name', required=True)
    args = parser.parse_args()
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    arguments = json.loads(args.arguments.read_text(encoding='utf-8')) if args.arguments else {}
    env = dict(os.environ, APOLLO_URL='http://127.0.0.1:8734', PYTHONIOENCODING='utf-8')
    params = StdioServerParameters(command=sys.executable,
                                   args=['-m', 'apollo_mcp.server'], env=env)
    with (EVIDENCE / (args.name + '-mcp-stderr.txt')).open('w', encoding='utf-8') as errors:
        async with stdio_client(params, errlog=errors) as (read, write):
            async with ClientSession(read, write) as session:
                await session.initialize()
                result = await session.call_tool(args.tool, arguments)
                serialized = result.model_dump(mode='json')
                (EVIDENCE / (args.name + '-mcp.json')).write_text(
                    json.dumps(serialized, ensure_ascii=False, indent=2), encoding='utf-8')
                if result.isError:
                    raise RuntimeError('Apollo MCP failed; inspect ' + args.name + '-mcp.json')
                content = [json.loads(c.text) for c in result.content if c.type == 'text']
                if len(content) != 1:
                    raise RuntimeError('Unexpected Apollo MCP result count')
                value = content[0]
                (EVIDENCE / (args.name + '.json')).write_text(
                    json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')
                job = value.get('job')
                pending = isinstance(job, dict) and job.get('status') in (
                    'queued', 'running', 'cancel_requested')
                if value.get('ok') is False and not pending:
                    raise RuntimeError('Apollo reported failure; inspect ' + args.name + '.json')
                print(json.dumps({'tool': args.tool, 'evidence': args.name,
                                  'ok': value.get('ok'), 'asset_id': value.get('asset_id'),
                                  'node_id': value.get('node_id'),
                                  'job': {k: value.get('job', {}).get(k) for k in
                                          ('id', 'job_id', 'status', 'stage', 'error')}
                                         if isinstance(value.get('job'), dict) else None,
                                  'verdict': value.get('verdict'),
                                  'pack_dir': value.get('pack_dir')},
                                 ensure_ascii=False), flush=True)


if __name__ == '__main__':
    asyncio.run(main())
