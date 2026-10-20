"""Capture fixed requested poses using actual official Unreal viewport pixels."""
import base64
import json
from pathlib import Path
import subprocess
import sys
import time
import CaptureReferenceArin as capture

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Feature/doc/evidence/arin-walk-talk-20261009/ue-motion'

def remote(expression):
    command="import sys,json; sys.path.insert(0,"+repr((ROOT/'Scripts/Editor').as_posix())+"); import ReviewArinMotion as motion; print(json.dumps("+expression+"))"
    result=subprocess.run([sys.executable,str(ROOT/'Scripts/RunEditorRemote.py'),'--command',command,'--timeout','12'],capture_output=True,text=True,encoding='utf-8')
    data=json.loads(result.stdout)
    if result.returncode or not data.get('success'):
        raise RuntimeError(str(data))
    return json.loads(data['output'][-1]['output'])

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    saved=capture.official(OUT,'saved-camera','GetCameraTransform',{})
    report={'source':'Actual Epic official Unreal MCP CaptureViewport','renders':[],'asset_mutation_requested':False}
    begun=False
    try:
        report['begin']=remote('motion.begin()'); begun=True
        view=capture.VIEWS['village-oblique']
        capture.official(OUT,'review-camera','SetCameraTransform',{'transform':{**view,'scale':{'x':1,'y':1,'z':1}}})
        for key,frame in [('Walk',1),('Walk',10),('Walk',19),('Walk',28),('Talk',24),('Talk',68)]:
            stem=key.lower()+'-'+str(frame).zfill(3)
            remote('motion.pose('+repr(key)+','+str(frame)+')')
            time.sleep(1.)
            observed=remote('motion.inspect()')
            if abs(observed['position']-(frame-1)/30.)>.0001 or observed['play_rate']!=0:
                raise RuntimeError('Engine capture did not hold requested animation pose')
            result=capture.official(OUT,stem,'CaptureViewport',{'captureTransform':None,'annotations':None,'bShowUI':False})
            image=result.pop('image')
            png=OUT/(stem+'.png')
            png.write_bytes(base64.b64decode(image['data']))
            report['renders'].append({'key':key,'frame':frame,'file':str(png),'observed':observed,'capture':result})
        report['status']='captured_requires_visual_review'
    finally:
        if begun:
            report['component_restoration']=remote('motion.end()')
        capture.official(OUT,'restored-camera','SetCameraTransform',{'transform':saved})
        restored=capture.official(OUT,'restored-camera-verify','GetCameraTransform',{})
        report['camera_restored']=capture.same_camera(saved,restored)
        if not report['camera_restored']:
            raise RuntimeError('Camera restoration mismatch')
        (OUT/'manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('ARIN_MOTION_VIEWPORT_CAPTURED')

if __name__=='__main__':
    main()
