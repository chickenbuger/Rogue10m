from pathlib import Path
from PIL import Image
import csv,json,hashlib
root=Path('D:/Project/Rogue10m')
out=root/'Feature/doc/evidence/full-body-shadow-20260928'
paths=[root/'Saved/Screenshots/WindowsEditor/AppearanceShadow'/f'frame_{n:04d}.png' for n in (20,50)]
images=[Image.open(p).convert('RGB') for p in paths]
assert images[0].size==images[1].size
result={'images':[{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths],'image_size':images[0].size,'regions':{}}
for name,box in {'head':(560,170,640,250),'unchanged_ground':(900,100,1000,200),'torso_interior':(580,320,620,370)}.items():
 a,b=[list(im.crop(box).getdata()) for im in images]
 la=[.2126*r+.7152*g+.0722*bl for r,g,bl in a];lb=[.2126*r+.7152*g+.0722*bl for r,g,bl in b]
 dark=[x-y for x,y in zip(la,lb)]
 result['regions'][name]={'rect_xyxy':box,'pixel_count':len(a),'before_mean_luma':sum(la)/len(a),'after_mean_luma':sum(lb)/len(b),
 'darkened_by_more_than_30_count':sum(v>30 for v in dark),'absolute_delta_more_than_30_count':sum(abs(v)>30 for v in dark),
 'mean_absolute_luma_difference':sum(abs(v) for v in dark)/len(a)}
with (root/'tmp/appearance-shadow/frames.csv').open(encoding='utf-8-sig',newline='') as f:rows=list(csv.DictReader(f))
result['rows']=len(rows)
result['active_samples']=sum(r['active']=='1' for r in rows)
result['third_person_samples']=sum(r['third']=='1' for r in rows)
result['max_pose_position_error_cm']=max(float(r['max_pose_position_error']) for r in rows)
result['max_pose_rotation_error_deg']=max(float(r['max_pose_rotation_error']) for r in rows)
result['native_shadow_policy_violations']=sum(int(r['body_shadow']) != int(r['active']=='0') for r in rows)
result['proxy_shadow_policy_violations']=sum(r['proxy_shadow']!=r['active'] for r in rows)
result['seen_attack_ids_active']=sorted({r['attack'] for r in rows if r['active']=='1' and int(r['attack'])>0})
result['last_serial']=rows[-1]['serial']
(out/'pixel-and-state-review.json').write_text(json.dumps(result,indent=2),encoding='utf8')
print(json.dumps(result,indent=2))
