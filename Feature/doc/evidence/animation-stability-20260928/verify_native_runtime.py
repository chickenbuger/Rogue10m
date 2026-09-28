import csv,json,math,sys
from pathlib import Path
sys.path.insert(0,str(Path('Scripts').resolve()))
from AnalyzeAnimationStability import qangle,q,v,sub,norm
base=Path('tmp/animation-stability')
def read(label,name):
 with (base/label/name).open(encoding='utf-8-sig',newline='') as f:return list(csv.DictReader(f))
after={(int(r['frame']),r['stage'],r['bone']):r for r in read('after','poses.csv')}
before={(int(r['frame']),r['stage'],r['bone']):r for r in read('before','poses.csv')}
frames={int(r['frame']):r for r in read('after','frames.csv')}
native=json.loads(Path('Feature/doc/evidence/animation-stability-20260928/secondary-stability.json').read_text())
result={'native_comparison':{},'locomotion':{},'source_appearance':{}}
for name,data in native['clips'].items():
 rows=[]
 for sample in data['runtime_samples']:
  frame=sample['frame'];t=sample['clip_s']; actual=frames.get(frame)
  if not actual or abs(float(actual['clip_s'])-t)>1e-6:continue
  if not ((name=='straight' and .30<t<.65) or (name=='hook' and .25<t<.8)):continue
  entry={'frame':frame,'clip_s':t,'bones':{}}
  for bone in ('pelvis','head','thigh_l','calf_l','foot_l','thigh_r','calf_r','foot_r'):
   c=sample['bones'][bone];b=before[frame,'source',bone];a=after[frame,'source',bone]
   entry['bones'][bone]={'before_cs_angle_deg':qangle(q(b,'raw_cs'),c['clip_cs']['rotation']),'after_cs_angle_deg':qangle(q(a,'raw_cs'),c['clip_cs']['rotation']),
     'after_local_angle_deg':qangle(q(a,'local'),c['clip_local']['rotation'])}
  rows.append(entry)
 result['native_comparison'][name]=rows
 for bone in ('pelvis','head','foot_l','foot_r'):
  print(name,bone,'n',len(rows),'maxbefore',max(r['bones'][bone]['before_cs_angle_deg'] for r in rows),'maxafter',max(r['bones'][bone]['after_cs_angle_deg'] for r in rows))
for bone in ('spine_01','thigh_l','thigh_r','calf_l','calf_r','foot_l','foot_r','lowerarm_l','lowerarm_r','hand_l','hand_r'):
 rows=[r for (f,s,b),r in after.items() if s=='appearance' and b==bone and f>=330]
 ratios=[float(r['length_ratio']) for r in rows]
 result['locomotion'][bone]={'length_ratio_min':min(ratios),'length_ratio_max':max(ratios)}
 print('locomotion',bone,min(ratios),max(ratios))
for bone in ('foot_l','foot_r'):
 vals=[]
 for frame in frames:
  a=after[frame,'appearance',bone];s=after[frame,'source',bone]
  vals.append({'frame':frame,'angle_deg':qangle(q(a,'raw_cs'),q(s,'raw_cs'))})
 result['source_appearance'][bone]={'max_raw_cs_quaternion_difference':max(vals,key=lambda x:x['angle_deg']),'all_samples':vals}
 print('stage',bone,result['source_appearance'][bone]['max_raw_cs_quaternion_difference'])
result['final_frame']={k:frames[450][k] for k in ['phase','attack','serial','third','is_falling','capsule_x','capsule_y','capsule_z']}
Path('Feature/doc/evidence/animation-stability-20260928/final-independent-runtime-metrics.json').write_text(json.dumps(result,indent=2),encoding='utf8')
