"""Capture paused candidate particles in the leased editor using official Unreal MCP.

The existing editor Python bridge controls transient simulation only. Assets are
authored by native MCP tools. No gameplay input, map save, or asset edit is made.
"""
from pathlib import Path
import base64
import hashlib
import json
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Scripts/Editor'))
from InspectWeaponFantasyVFX20261110 import Client
OUT=ROOT/'Feature/doc/evidence/magic-particle-candidates-20261110/MagicMcp01'/('render-clean' if '--clean-view' in sys.argv else 'render')


def remote(expression):
    command='import json;import ReviewMagicParticleCandidates20261110 as review;print(json.dumps('+expression+'))'
    run=subprocess.run([sys.executable,str(ROOT/'Scripts/RunEditorRemote.py'),'--command',command,'--timeout','12'],capture_output=True,text=True,encoding='utf-8')
    body=json.loads(run.stdout)
    if run.returncode or not body.get('success'):raise RuntimeError(str(body)+run.stderr)
    return json.loads(body['output'][-1]['output'])


def decoded(value):
    while isinstance(value,str):value=json.loads(value)
    return value


def main():
    if '--lease-granted' not in sys.argv:raise RuntimeError('Editor lease required')
    if OUT.exists():raise RuntimeError('Preserve earlier capture evidence')
    OUT.mkdir(parents=True)
    c=Client('http://127.0.0.1:8001/mcp',OUT)
    def call(name,args,label):
        return decoded(c.tool('call_tool',{'toolset_name':'EditorToolset.EditorAppToolset','tool_name':name,'arguments':args},label))
    camera=call('GetCameraTransform',{},'camera-before')
    if not camera:raise RuntimeError('No original camera')
    manifest={'source':'actual official Unreal MCP CaptureViewport','world':'/Engine/Maps/Entry.Entry',
              'gameplay_input':False,'render_mode':'D3D11 offscreen editor viewport',
              'simulation':'manual fixed 1/240 second steps then paused; requested age is not measured dataset age',
              'captures':{},'original_camera':camera}
    def capture(name,x,y,age):
        pose=remote('review.pose('+repr(age)+')')
        transform={'location':{'x':x,'y':y,'z':100},'rotation':{'pitch':0,'yaw':180,'roll':0},'scale':{'x':1,'y':1,'z':1}}
        call('SetCameraTransform',{'transform':transform},name+'-camera')
        time.sleep(1.5)
        body=call('CaptureViewport',{'captureTransform':None,'annotations':None,'bShowUI':False},name)
        pic=body.pop('image')
        if pic['mimeType']!='image/png':raise RuntimeError('Expected PNG')
        path=OUT/(name+'.png');path.write_bytes(base64.b64decode(pic['data'],validate=True))
        body['image']={'path':path.name,'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
        body['preview_state']=pose
        (OUT/(name+'.json')).write_text(json.dumps(body,indent=2),encoding='utf-8')
        manifest['captures'][name]=body
        print('CAPTURED='+name,flush=True)
    try:
        for name,age in [('all-early',.15),('all-mid',.35),('all-late',.7),('all-expired',3.)]:capture(name,300,180,age)
        for i,role in enumerate(['telegraph','circle','release','impact']):capture(role+'-mid',160,i*120,.35)
    finally:
        call('SetCameraTransform',{'transform':camera},'camera-restore')
        manifest['final_preview_state']=remote('review.inspect()')
        manifest['restored_camera']=call('GetCameraTransform',{},'camera-after')
        (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print('RESULT=MAGIC_ACTUAL_VIEWPORT_CAPTURES_SAVED',flush=True)


if __name__=='__main__':main()
