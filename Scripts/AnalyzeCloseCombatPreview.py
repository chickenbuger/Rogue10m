"""Validate sampled preview data and optionally draw scientific bone pose traces.

Run with UE's Python; Pillow is optional and can use the existing project .venv.
These figures are data plots, not Unreal screenshots or a skinned animation QA.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys


def qangle(a,b):
    norm=math.sqrt(sum(v*v for v in a)*sum(v*v for v in b))
    return math.degrees(2*math.acos(min(1.,abs(sum(x*y for x,y in zip(a,b)))/norm)))


def qmul(a,b):
    x,y,z,w=a;xx,yy,zz,ww=b
    return [w*xx+x*ww+y*zz-z*yy,w*yy-x*zz+y*ww+z*xx,
            w*zz+x*yy-y*xx+z*ww,w*ww-x*xx-y*yy-z*zz]


def motion_delta(q,start):
    return qmul(q,[-start[0],-start[1],-start[2],start[3]])


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--evidence',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--draw',action='store_true')
    args=parser.parse_args()
    audit=json.loads((args.evidence/'editor-audit.json').read_text(encoding='utf-8'))
    result={'errors':[],'clips':{},'gameplay_contract_unchanged':audit['gameplay_contract_unchanged']}
    for label,entry in audit['clips'].items():
        source,target=entry['source'],entry['manny']
        ratios=target['limb_length_ref_ratios']
        if not target['skeleton'].startswith('/Game/Characters/Mannequins/Meshes/SK_Mannequin.'):
            result['errors'].append(label+' wrong skeleton')
        if abs(target['duration']-source['duration'])>1/60:
            result['errors'].append(label+' duration mismatch')
        low,high=min(v[0] for v in ratios.values()),max(v[1] for v in ratios.values())
        if low<.995 or high>1.005:
            result['errors'].append(label+' limb length outside 0.5% reference tolerance')
        if len(source['frames'])!=len(target['frames']):
            result['errors'].append(label+' frame count mismatch')
        rot_errors=[]
        camera_body_rot_errors=[]
        source_start=source['frames'][0]['bones']
        target_start=target['frames'][0]['bones']
        for a,b in zip(source['frames'],target['frames']):
            for bone in ['pelvis','head','hand_l','hand_r','foot_l','foot_r']:
                rot_errors.append(qangle(a['bones'][bone]['q'],b['bones'][bone]['q']))
                if bone in ['pelvis','head']:
                    camera_body_rot_errors.append(qangle(
                        motion_delta(a['bones'][bone]['q'],source_start[bone]['q']),
                        motion_delta(b['bones'][bone]['q'],target_start[bone]['q'])))
        locked=entry['manny_with_package_root_lock']
        result['clips'][label]={'limb_ref_ratio_min':low,'limb_ref_ratio_max':high,
            'max_source_to_target_component_rotation_difference_deg':max(rot_errors),
            'max_source_to_target_head_pelvis_motion_delta_difference_deg':max(camera_body_rot_errors),
            'raw_root_travel_cm':target['summary']['root']['max_displacement_from_start_cm'],
            'package_locked_root_travel_cm':locked['summary']['root']['max_displacement_from_start_cm'],
            'force_root_lock':target['force_root_lock'],'frame_count':len(target['frames'])}
        if max(camera_body_rot_errors)>.01:
            result['errors'].append(label+' source head/pelvis rotation preservation exceeds 0.01 degrees')
        if not target['force_root_lock'] or locked['summary']['root']['max_displacement_from_start_cm']>.001:
            result['errors'].append(label+' preview root lock did not hold within 0.001 cm')
    if not audit['gameplay_contract_unchanged']:
        result['errors'].append('gameplay contract changed')
    result['result']='PASS' if not result['errors'] else 'FAIL'
    result['limits']='UE4/Manny reference axes differ: absolute orientation differences are recorded, not used as motion errors. Head/pelvis deltas relative to first pose are checked. Naturalness, skin, montage blending, camera aim and damage are untested.'
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'pose-validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    if args.draw:
        project=args.evidence.resolve().parents[3]
        sys.path.insert(0,str(project/'.venv/Lib/site-packages'))
        from PIL import Image,ImageDraw,ImageFont
        try:font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',16)
        except OSError:font=ImageFont.load_default()
        links=[('pelvis','head'),('pelvis','upperarm_l'),('upperarm_l','lowerarm_l'),('lowerarm_l','hand_l'),
               ('pelvis','upperarm_r'),('upperarm_r','lowerarm_r'),('lowerarm_r','hand_r'),
               ('pelvis','thigh_l'),('thigh_l','calf_l'),('calf_l','foot_l'),
               ('pelvis','thigh_r'),('thigh_r','calf_r'),('calf_r','foot_r')]
        for filename,labels in [('attack-pose-traces.png',['LeftHook','RightUppercut','LowRightThaiKick','StraightKick']),
                                ('evade-pose-traces.png',['FrontEvade','BackEvade','LeftEvade','RightEvade'])]:
            img=Image.new('RGB',(1720,1060),'#f3f6fa');draw=ImageDraw.Draw(img)
            draw.text((20,12),'Manny bone pose traces | 60 Hz UE samples | root translation removed per frame',font=font,fill='#172a45')
            for row,label in enumerate(labels):
                data=audit['clips'][label]['manny'];ybase=80+row*235
                draw.text((20,ybase),label+'  '+format(data['duration'],'.3f')+' s',font=font,fill='#172a45')
                frames=data['frames']
                for col in range(6):
                    frame=frames[round((len(frames)-1)*col/5)]
                    bones=frame['bones'];root=bones['root']['p'];cx=320+col*265
                    def xy(name):
                        p=bones[name]['p'];x=p[0]-root[0];y=p[1]-root[1];z=p[2]-root[2]
                        return (cx+(.75*y+.5*x)*.8,ybase+190-z*.8)
                    draw.line([(cx-110,ybase+190),(cx+110,ybase+190)],fill='#ccd3df',width=1)
                    for a,b in links:
                        color='#048b8d' if b.endswith('_l') else '#d85e32' if b.endswith('_r') else '#172a45'
                        draw.line([xy(a),xy(b)],fill=color,width=3)
                    for name in bones:
                        px,py=xy(name);r=5 if name=='head' else 2
                        draw.ellipse((px-r,py-r,px+r,py+r),fill='#172a45')
                    draw.text((cx-40,ybase+201),format(frame['t'],'.3f')+' s',font=font,fill='#172a45')
            draw.text((20,1025),'Scientific projection only. No mesh, rendered camera, contact target or hit-volume verification.',font=font,fill='#515f73')
            img.save(args.output/filename)
    print(json.dumps({'result':result['result'],'errors':result['errors'],'clips':len(result['clips'])}))
    if result['errors']:raise SystemExit(1)


if __name__=='__main__':main()
