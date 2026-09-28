"""Measure and render SDK-evaluated Boxing.fbx motion without modifying the FBX.
Requires numpy and Pillow; JSON comes from the read-only FBX SDK inspector.
Outputs describe skeleton positions, not contact forces or a game retarget.
"""
import argparse
import hashlib
import json
import math
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, default=Path('tmp/boxing-reference/actual-motion.json'))
    parser.add_argument('--source', type=Path, default=Path('C:/Users/PC/Downloads/Boxing.fbx'))
    parser.add_argument('--output', type=Path, default=Path('Feature/doc/images/boxing-reference-20260922'))
    parser.add_argument('--evidence', type=Path, default=Path('Feature/doc/evidence/boxing-reference-20260922'))
    parser.add_argument('--frames', type=Path, default=Path('tmp/boxing-reference/frames'))
    parser.add_argument('--ffmpeg', type=Path, default=Path('tmp/recorded-reference/decoder/ffmpeg.exe'))
    args = parser.parse_args()
    d = json.loads(args.input.read_text(encoding='utf-8-sig'))
    a = np.asarray([f['positions'] for f in d['frames']], dtype=float)
    t = np.asarray([f['time'] for f in d['frames']], dtype=float)
    names = [b['name'].split(':')[-1] for b in d['bones']]
    idx = {n: i for i, n in enumerate(names)}
    assert d['axis'] == 'MayaYUp' and d['unit_cm'] == 1, 'Unsupported input coordinates'
    assert np.isfinite(a).all() and np.ptp(a, axis=0).max() > 0.01, 'Selected stack is static'
    assert len(t) > 2 and np.all(np.diff(t) > 0)
    p = lambda name: a[:, idx[name]]
    unit = lambda v: v / np.linalg.norm(v, axis=-1, keepdims=True)
    # Mixamo left/right names establish anatomical sides. Y up is declared by SDK.
    # Initial shoulder line fixes lateral basis; toe direction only disambiguates forward sign.
    # Foot splay is deliberately not treated as the attack heading.
    up = np.array([0., 1., 0.])
    forward = ((p('LeftToeBase')[0] - p('LeftFoot')[0]) +
               (p('RightToeBase')[0] - p('RightFoot')[0])) / 2
    forward[1] = 0
    toe_forward = unit(forward)
    left = p("LeftArm")[0] - p("RightArm")[0]
    left[1] = 0
    left = unit(left)
    forward = unit(np.cross(left, up))
    assert float(np.dot(forward, toe_forward)) > 0
    basis = np.stack([left, up, forward], axis=1)
    hips = p('Hips')
    shoulders = (p('LeftArm') + p('RightArm')) * 0.5
    sh_left = p('LeftArm') - p('RightArm')
    sh_left[:, 1] = 0
    sh_left = unit(sh_left)
    sh_forward = unit(np.cross(sh_left, np.broadcast_to(up, sh_left.shape)))
    assert float(np.dot(sh_forward[0], forward)) > 0
    shoulder_width = float(np.linalg.norm(p('LeftArm')[0] - p('RightArm')[0]))
    hip_left = p('LeftUpLeg') - p('RightUpLeg')
    hip_left[:, 1] = 0
    hip_left = unit(hip_left)
    def yaw_of(v):
        return np.degrees(np.unwrap(np.arctan2(v @ left, v @ forward)))
    shoulder_yaw = yaw_of(sh_forward)
    pelvis_yaw = yaw_of(np.cross(hip_left, np.broadcast_to(up, hip_left.shape)))
    arms = {}
    for side in ['Left', 'Right']:
        delta = p(side + 'Hand') - p(side + 'Arm')
        world = delta @ basis
        dynamic = np.stack([np.sum(delta * sh_left, axis=1), delta[:, 1],
                            np.sum(delta * sh_forward, axis=1)], axis=1)
        velocity = np.gradient(p(side + 'Hand') - hips, t, axis=0)
        extension = np.linalg.norm(delta, axis=1)
        peak = int(np.argmax(world[:, 2]))
        upper = p(side + 'Arm') - p(side + 'ForeArm')
        lower = p(side + 'Hand') - p(side + 'ForeArm')
        elbow_angle = np.degrees(np.arccos(np.clip(np.sum(unit(upper) * unit(lower), axis=1), -1, 1)))
        arms[side.lower()] = dict(
            peak_frame=peak, peak_time_s=float(t[peak]),
            forward_start_cm=float(world[0, 2]), forward_peak_cm=float(world[peak, 2]),
            forward_excursion_cm=float(world[:, 2].max() - world[:, 2].min()),
            start_elbow_angle_deg=float(elbow_angle[0]), peak_elbow_angle_deg=float(elbow_angle[peak]),
            max_shoulder_hand_distance_cm=float(extension.max()),
            start_hand_pelvis_cm=((p(side + 'Hand')[0] - hips[0]) @ basis).tolist(),
            start_elbow_pelvis_cm=((p(side + 'ForeArm')[0] - hips[0]) @ basis).tolist(),
            start_hand_head_cm=((p(side + 'Hand')[0] - p('Head')[0]) @ basis).tolist(),
            peak_hand_head_cm=((p(side + 'Hand')[peak] - p('Head')[peak]) @ basis).tolist(),
            peak_pelvis_relative_speed_cm_s=float(np.linalg.norm(velocity, axis=1).max()),
            shoulder_fixed_coordinates_cm=world.tolist(),
            shoulder_rotating_coordinates_cm=dynamic.tolist(),
            shoulder_width_normalized= (dynamic / shoulder_width).tolist(),
            elbow_angles_deg=elbow_angle.tolist())
    foot_mid = (p('LeftFoot') + p('RightFoot')) * 0.5
    hip_from_support = (hips - foot_mid) @ basis
    hip_travel = (hips - hips[0]) @ basis
    metrics = dict(
        input=str(args.input), source=str(args.source), source_sha256=hashlib.sha256(args.source.read_bytes()).hexdigest(),
        stack=d['stack'], frame_count=len(t), sample_fps=30, duration_s=float(t[-1]-t[0]), mesh_count=d['mesh_count'],
        coordinate_order=['anatomical_left', 'up', 'forward'], fixed_basis_columns=basis.tolist(),
        basis_evidence='MayaYUp Y-up; anatomical sides from Mixamo bone names; fixed forward perpendicular to initial shoulder line, with foot-to-toe direction validating its sign. Rotating coordinates track shoulders per frame.',
        shoulder_width_cm=shoulder_width, arms=arms,
        pelvis_yaw_deg=pelvis_yaw.tolist(), shoulder_yaw_deg=shoulder_yaw.tolist(),
        pelvis_yaw_excursion_deg=float(np.ptp(pelvis_yaw)), shoulder_yaw_excursion_deg=float(np.ptp(shoulder_yaw)),
        pelvis_translation_range_cm=np.ptp(hip_travel, axis=0).tolist(),
        head_translation_range_cm=np.ptp((p('Head')-p('Head')[0]) @ basis, axis=0).tolist(),
        pelvis_vs_foot_midpoint_cm=hip_from_support.tolist(),
        initial_left_minus_right_foot_cm=((p('LeftFoot')[0]-p('RightFoot')[0])@basis).tolist(),
        left_foot_travel_range_cm=np.ptp((p('LeftFoot')-p('LeftFoot')[0])@basis, axis=0).tolist(),
        right_foot_travel_range_cm=np.ptp((p('RightFoot')-p('RightFoot')[0])@basis, axis=0).tolist(),
        caveats=['Pelvis over foot midpoint is a weight-shift proxy, not measured center of mass or contact force.',
                 'Clip contains one stepping left straight punch; the right hand remains guarding. No right cross/hook exemplar is claimed.',
                 'Rendered lines are source skeleton positions, not the source meshes or a game animation.'])
    for path in [args.output, args.evidence, args.frames]:
        path.mkdir(parents=True, exist_ok=True)
    (args.evidence/'boxing-motion-analysis.json').write_text(json.dumps(metrics, indent=2), encoding='utf-8')
    (args.evidence/'boxing-source-motion.json').write_bytes(args.input.read_bytes())
    colors = {'Left': (66, 224, 201), 'Right': (255, 190, 80)}
    font = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 23)
    small = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 16)
    valid = {'Hips','Spine','Spine1','Spine2','Neck','Head','HeadTop_End','LeftShoulder','LeftArm',
             'LeftForeArm','LeftHand','RightShoulder','RightArm','RightForeArm','RightHand',
             'LeftUpLeg','LeftLeg','LeftFoot','LeftToeBase','RightUpLeg','RightLeg','RightFoot','RightToeBase'}
    view_angles = [25, 90]
    camera = {}
    # Fixed framing preserves the source's actual forward stepping rather than root-locking it.
    for yaw in view_angles:
        rad = math.radians(yaw)
        pts = a[:, [idx[n] for n in valid]].reshape(-1, 3)
        u = pts[:, 0]*math.cos(rad)+pts[:, 2]*math.sin(rad)
        depth = -pts[:, 0]*math.sin(rad)+pts[:, 2]*math.cos(rad)
        v = pts[:, 1]-depth*.17
        scale = min(430/(np.ptp(u)+10), 340/(v.max()-min(0,v.min())+10))
        camera[yaw] = (scale,(u.min()+u.max())*.5,v.max())
    def project(pos, yaw, cx):
        x,y,z=pos;rad=math.radians(yaw)
        u=x*math.cos(rad)+z*math.sin(rad);depth=-x*math.sin(rad)+z*math.cos(rad)
        scale,mid,top=camera[yaw]
        return (cx+(u-mid)*scale,104+(top-y+depth*.17)*scale),depth
    frames=[]
    for k in range(len(t)):
        im=Image.new('RGB',(1040,600),(15,21,31));draw=ImageDraw.Draw(im)
        draw.text((24,14),'Boxing.fbx · 실제 골격 모션 분석',font=font,fill=(238,242,248))
        draw.text((24,48),'mixamo.com / 왼손 전진 타격 · 청록: 왼쪽 · 금색: 오른쪽 / 게임 적용 전',font=small,fill=(173,189,209))
        for yaw,cx,label in [(25,260,'사선 시점'),(90,780,'측면 시점')]:
            for grid in range(-75,151,25):
                for ends in [((grid,0,-60),(grid,0,150)),((-75,0,grid),(100,0,grid))]:
                    draw.line([project(v,yaw,cx)[0] for v in ends],fill=(32,42,57),width=1)
            for side,color in colors.items():
                points=[project(a[q,idx[side+'Hand']],yaw,cx)[0] for q in range(max(0,k-10),k+1)]
                if len(points)>1:draw.line(points,fill=tuple(int(x*.52) for x in color),width=2)
            segments=[]
            for i,b in enumerate(d['bones']):
                parent=b['parent'];name=names[i]
                if parent<0 or name not in valid or names[parent] not in valid:continue
                point1,dep1=project(a[k,parent],yaw,cx);point2,dep2=project(a[k,i],yaw,cx)
                color=colors['Left'] if name.startswith('Left') else colors['Right'] if name.startswith('Right') else (207,217,235)
                segments.append(((dep1+dep2)*.5,point1,point2,color,name))
            for _,p1,p2,color,name in sorted(segments,reverse=True):
                draw.line((p1,p2),fill=color,width=8 if 'Leg' in name or 'Spine' in name else 6)
                for x,y in [p1,p2]:draw.ellipse((x-3,y-3,x+3,y+3),fill=color)
            head,_=project(p('Head')[k],yaw,cx)
            draw.ellipse((head[0]-10,head[1]-16,head[0]+10,head[1]+3),outline=(235,239,247),width=3)
            draw.text((cx-45,472),label,font=small,fill=(202,212,229))
        draw.text((24,508),f'{t[k]:.2f} / {t[-1]:.2f}초    왼손 어깨기준 전방 {arms["left"]["shoulder_fixed_coordinates_cm"][k][2]:.1f}cm',font=small,fill=(221,231,244))
        draw.text((24,538),'발 이동 + 골반 하강 + 어깨 회전 + 타격 후 가드 복귀',font=small,fill=(171,188,208))
        draw.rectangle((24,579,1016,583),fill=(46,58,76));draw.rectangle((24,579,24+992*k/(len(t)-1),583),fill=(66,224,201))
        im.save(args.frames/f'frame_{k:04d}.png');frames.append(im)
    picks=sorted(set([0,5,10,arms['left']['peak_frame'],20,30,40,len(t)-1]))
    sheet=Image.new('RGB',(1040,300*math.ceil(len(picks)/2)),(15,21,31))
    for j,k in enumerate(picks):sheet.paste(frames[k].resize((520,300)),((j%2)*520,(j//2)*300))
    sheet.save(args.output/'boxing-motion-sheet.jpg',quality=88)
    sheet.resize((650,round(sheet.height*650/sheet.width))).save(args.output/'boxing-motion-review.jpg',quality=73)
    if args.ffmpeg.exists():
        command=[str(args.ffmpeg.resolve()),'-hide_banner','-y','-framerate','30','-i',str(args.frames/'frame_%04d.png'),
                 '-c:v','libopenh264','-b:v','3M','-pix_fmt','yuv420p','-movflags','+faststart',str(args.output/'boxing-original-motion.mp4')]
        result=subprocess.run(command,capture_output=True,text=True)
        (args.evidence/'encode.log').write_text(result.stdout+result.stderr,encoding='utf-8')
        result.check_returncode()
    print(json.dumps({k:v for k,v in metrics.items() if k not in ['arms','pelvis_vs_foot_midpoint_cm','shoulder_yaw_deg','pelvis_yaw_deg']},indent=2))
    for side, values in arms.items():print(side,json.dumps({k:v for k,v in values.items() if not isinstance(v,list) or len(v)==3}))


if __name__=='__main__':
    main()
