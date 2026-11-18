"""Capture disposable UE boss preview poses with official viewport API."""
from pathlib import Path
import json,subprocess,sys,time,base64
from CaptureArinQuality import official
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'DevLog/20261110_HumanoidBossNpc/Boss/PreviewC'
OUT.mkdir(parents=True,exist_ok=True)
views={'front':{'location':{'x':1400,'y':-1600,'z':110},'rotation':{'pitch':-3,'yaw':180,'roll':0},'scale':{'x':1,'y':1,'z':1}},'oblique':{'location':{'x':1320,'y':-1870,'z':150},'rotation':{'pitch':-8,'yaw':140,'roll':0},'scale':{'x':1,'y':1,'z':1}}}
for name,t in [('Idle',.3),('Hook_Raw',.5),('Wave_Raw',2.0),('Zone_Raw',2.7)]:
 command='import PreviewHumanBoss20261110 as boss; print(boss.pose('+repr(name)+','+str(t)+'))'
 p=subprocess.run([sys.executable,str(ROOT/'Scripts/RunEditorRemote.py'),'--command',command],capture_output=True,text=True,encoding='utf-8')
 if p.returncode: raise RuntimeError(p.stdout)
 time.sleep(2)
 for v,transform in views.items():
  stem=name+'-'+v
  data=official(OUT,stem,'CaptureViewport',{'captureTransform':transform,'annotations':None,'bShowUI':False})
  (OUT/(stem+'.png')).write_bytes(base64.b64decode(data['image']['data'],validate=True))
  (OUT/(stem+'-response.json')).write_text(json.dumps({'source':'actual Unreal Editor CaptureViewport','pose':name,'time':t,'camera':transform}),encoding='utf-8')
  print(stem,flush=True)
