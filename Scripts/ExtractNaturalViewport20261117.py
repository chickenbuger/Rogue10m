"""Decode native MCP PNG evidence without changing pixels."""
import argparse
import base64
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]/'Feature/doc/evidence/landscape-natural-boundary20261117'


def find(value):
    if isinstance(value,str):
        try:return find(json.loads(value))
        except (ValueError,TypeError):return None
    if isinstance(value,dict):
        if value.get('mimeType')=='image/png' and isinstance(value.get('data'),str):return value['data']
        for v in value.values():
            found=find(v)
            if found:return found
    if isinstance(value,list):
        for v in value:
            found=find(v)
            if found:return found
    return None


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('input',type=Path);p.add_argument('output',type=Path);args=p.parse_args()
    for value in (args.input,args.output):
        if not value.resolve().is_relative_to(ROOT.resolve()):raise RuntimeError('Exact evidence directory required')
    encoded=find(json.loads(args.input.read_text(encoding='utf-8-sig')))
    if encoded is None:raise RuntimeError('Native PNG missing')
    raw=base64.b64decode(encoded,validate=True)
    if not raw.startswith(b'\x89PNG\r\n\x1a\n'):raise RuntimeError('PNG signature missing')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('xb') as f:f.write(raw)
    print(json.dumps({'path':str(args.output.resolve()),'bytes':len(raw),'pixels_modified':False}))
