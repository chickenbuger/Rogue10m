"""Independent fixed-action-window comparison; includes attack entry and recovery.
Usage: python independent_attack_comparison.py AFTER_CAPTURE_FOLDER_NAME
"""
from pathlib import Path
import csv,json,math,sys
ROOT=Path('D:/Project/Rogue10m')
EVIDENCE=ROOT/'Feature/doc/evidence/animation-stability-20260928'
WINDOWS={'idle':(0,44),'left_jab':(45,56),'right_jab_and_recovery':(57,134),'straight_and_recovery':(135,209),'hook_charge_attack_recovery':(210,329)}
BONES=['head','pelvis','spine_01','hand_l','hand_r','upperarm_l','upperarm_r','lowerarm_l','lowerarm_r','thigh_l','thigh_r','calf_l','calf_r','foot_l','foot_r']


def load(capture):
 folder=ROOT/'tmp/animation-stability'/capture
 def read(name):
  with (folder/name).open(encoding='utf-8-sig',newline='') as f:return list(csv.DictReader(f))
 frames={int(r['frame']):r for r in read('frames.csv')}
 poses={(int(r['frame']),r['stage'],r['bone'].casefold()):r for r in read('poses.csv')}
 if not set(range(330)).issubset(frames):raise RuntimeError('Missing common action frames')
 return frames,poses


def summarize(capture):
 frames,poses=load(capture)
 out={'capture':capture,'total_frames':len(frames),'windows':{}}
 for label,(start,end) in WINDOWS.items():
  ids=list(range(start,end+1));result={'frames':[start,end],'samples':len(ids),'bones':{}}
  result['montages']=sorted({frames[i]['montage'] for i in ids if frames[i]['montage']})
  for bone in BONES:
   result['bones'][bone]={}
   for stage in ['source','appearance']:
    rows=[poses[i,stage,bone] for i in ids]
    r=max(rows,key=lambda x:float(x['step_deg']))
    p=max(rows,key=lambda x:float(x['step_cm']))
    ratio=[float(x['length_ratio']) for x in rows]
    result['bones'][bone][stage]={'max_step_deg':float(r['step_deg']),'max_step_frame':int(r['frame']),
      'max_step_cm':float(p['step_cm']),'max_position_step_frame':int(p['frame']),
      'local_length_ratio_min':min(ratio),'local_length_ratio_max':max(ratio)}
  out['windows'][label]=result
 return out


def main():
 capture=sys.argv[1]
 before,after=summarize('before'),summarize(capture)
 comparison={'method':'Fixed action windows include entry and recovery, not only live montage frames. First 330 frames are common; later walking/jump frames are outside this comparison.','before':before,'after':after}
 path=EVIDENCE/('independent-before-vs-'+capture+'.json')
 path.write_text(json.dumps(comparison,indent=2),encoding='utf-8')
 print(json.dumps({'output':str(path),'windows':{label:{b:{stage:{'before':before['windows'][label]['bones'][b][stage], 'after':after['windows'][label]['bones'][b][stage]} for stage in ['source','appearance']} for b in ['head','hand_r','thigh_l','foot_l','foot_r']} for label in WINDOWS}},indent=2))


if __name__=='__main__':main()
