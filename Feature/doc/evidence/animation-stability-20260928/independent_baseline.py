import csv,json,importlib.util,sys
from pathlib import Path
root=Path('D:/Project/Rogue10m')
capture=sys.argv[1] if len(sys.argv)>1 else 'before'
spec=importlib.util.spec_from_file_location('a',root/'Scripts/AnalyzeAnimationStability.py');a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)
def read(name):
 with (root/'tmp/animation-stability'/capture/name).open(encoding='utf-8-sig',newline='') as f:rows=list(csv.DictReader(f))
 for x in rows:
  for field in ['bone','parent']:
   if field in x:x[field]=x[field].casefold()
 return rows
frames={int(x['frame']):x for x in read('frames.csv')};rows=read('poses.csv');by={(int(x['frame']),x['stage'],x['bone']):x for x in rows}
ref={}
for x in read('skeletons.csv'):
 p=ref.get((x['stage'],x['parent']));ref[x['stage'],x['bone']]=a.mul(p,a.q(x,'reference_local')) if p else a.q(x,'reference_local')
bones=['root','pelvis','head','hand_l','hand_r','thigh_l','thigh_r','calf_l','calf_r','foot_l','foot_r']
out={'capture':capture,'warning':'Attack windows include only active montage samples. Cross-rig reference deltas can include retarget-pose offsets; derivatives and A/B isolate causality.','windows':{},'outlier_frames':{},'scale_range':{}}
for serial in range(5):
 ids=[n for n,f in frames.items() if (n<45 if serial==0 else int(f['serial'])==serial and f['montage'])]
 if not ids:continue
 window={'frames':[min(ids),max(ids)],'samples':len(ids),'montages':sorted(set(frames[n]['montage'] for n in ids)),'bones':{}}
 for b in bones:
  res={}
  for stage in ['source','appearance']:
   rr=[by[n,stage,b] for n in ids];mx=max(rr,key=lambda r:float(r['step_deg']))
   res[stage]={'max_step_deg':float(mx['step_deg']),'max_step_frame':int(mx['frame']),'max_rotation_from_capture_idle_deg':max(a.qangle(a.q(by[0,stage,b],'raw_cs'),a.q(r,'raw_cs')) for r in rr),'local_length_ratio':[min(float(r['length_ratio']) for r in rr),max(float(r['length_ratio']) for r in rr)]}
  dif=[]
  for n in ids:
   sd=a.mul(a.q(by[n,'source',b],'raw_cs'),a.inverse(ref['source',b]));td=a.mul(a.q(by[n,'appearance',b],'raw_cs'),a.inverse(ref['appearance',b]));dif.append((a.qangle(sd,td),n))
  res['max_reference_relative_stage_difference']={'degrees':max(dif)[0],'frame':max(dif)[1]};window['bones'][b]=res
 out['windows'][str(serial)]=window
for n in [49,50,51,52,60,61,71,72,251,252,253,261,262]:
 out['outlier_frames'][str(n)]={'state':frames[n],'bones':{stage:{b:{'local_position':a.v(by[n,stage,b],'local'),'local_scale':[float(by[n,stage,b]['local_s'+axis]) for axis in 'xyz'],'step_deg':float(by[n,stage,b]['step_deg']),'length_ratio':float(by[n,stage,b]['length_ratio'])} for b in bones} for stage in ['source','appearance']}}
for stage in ['source','appearance']:
 rr=[r for r in rows if r['stage']==stage];out['scale_range'][stage]={axis:[min(float(r['local_s'+axis]) for r in rr),max(float(r['local_s'+axis]) for r in rr)] for axis in 'xyz'}
p=root/'Feature/doc/evidence/animation-stability-20260928'/('independent-'+capture+'-metrics.json');p.write_text(json.dumps(out,indent=2),encoding='utf-8')
print(json.dumps({'output':str(p),'scale_range':out['scale_range'],'windows':{k:{'frames':v['frames'],'samples':v['samples'],'head':v['bones']['head'],'foot_r':v['bones']['foot_r'],'thigh_l':v['bones']['thigh_l']} for k,v in out['windows'].items()}},indent=2))