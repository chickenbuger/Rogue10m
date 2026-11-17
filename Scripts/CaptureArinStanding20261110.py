"""Capture actual official Unreal viewport frames. No asset or map save."""
import base64,json,subprocess,sys,time
from pathlib import Path
import CaptureReferenceArin as cap
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'DevLog/20261110_HumanoidBossNpc/NPC/ViewportA'
def remote(expression):
    cmd='import sys,json; sys.path.insert(0,'+repr((ROOT/'Scripts/Editor').as_posix())+'); import ReviewArinStanding20261110 as review; print(json.dumps('+expression+'))'
    p=subprocess.run([sys.executable,str(ROOT/'Scripts/RunEditorRemote.py'),'--command',cmd,'--timeout','12'],capture_output=True,text=True,encoding='utf-8')
    d=json.loads(p.stdout)
    if p.returncode or not d.get('success'):raise RuntimeError(str(d))
    return json.loads(d['output'][-1]['output'])
def main():
    OUT.mkdir(parents=True,exist_ok=False)
    saved=cap.official(OUT,'saved-camera','GetCameraTransform',{})
    report={'source':'Actual official UE5.8 MCP editor viewport, existing StartHub Arin actor','captures':[],'not_gameplay_input':True}
    began=False
    try:
        report['begin']=remote('review.begin()');began=True
        for view in ['village-front','village-oblique']:
            cap.official(OUT,view+'-camera','SetCameraTransform',{'transform':{**cap.VIEWS[view],'scale':{'x':1,'y':1,'z':1}}})
            for key,t in [('before',0.),('idle',0.),('idle',2.5),('idle',5.),('talk',.8),('idle',0.)]:
                stem=view+'-'+key+'-'+str(t).replace('.','_')+'-'+str(len(report['captures']))
                remote('review.pose('+repr(key)+','+str(t)+')');time.sleep(1.5)
                state=remote('review.inspect()')
                if abs(state['position']-t)>.001 or state['play_rate']!=0:raise RuntimeError('Pose not held')
                data=cap.official(OUT,stem,'CaptureViewport',{'captureTransform':None,'annotations':None,'bShowUI':False})
                img=data.pop('image');(OUT/(stem+'.png')).write_bytes(base64.b64decode(img['data']))
                (OUT/(stem+'-response.json')).write_text(json.dumps(data,indent=2))
                report['captures'].append({'view':view,'key':key,'time':t,'file':stem+'.png','state':state})
                print(stem,flush=True)
        report['status']='captured_requires_visual_review'
    finally:
        if began:report['restore']=remote('review.end()')
        cap.official(OUT,'restore-camera','SetCameraTransform',{'transform':saved})
        (OUT/'manifest.json').write_text(json.dumps(report,indent=2))
if __name__=='__main__':main()
