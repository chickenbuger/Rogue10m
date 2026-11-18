"""Calibrate a static two-hand grip from evaluated poses, then report blade trajectories.
This produces evidence, never changes Unreal assets or native defaults by itself.
"""
from pathlib import Path
import json,math
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Feature/doc/evidence/sword-knuckle-20261110'
def add(a,b):return [x+y for x,y in zip(a,b)]
def sub(a,b):return [x-y for x,y in zip(a,b)]
def mul(a,k):return [x*k for x in a]
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def unit(a):return mul(a,1/math.sqrt(dot(a,a)))
def inv(q):return [-q[0],-q[1],-q[2],q[3]]
def qm(a,b):
 x,y,z,w=a;u,v,s,t=b
 return [w*u+x*t+y*s-z*v,w*v-x*s+y*t+z*u,w*s+x*v-y*u+z*t,w*t-x*u-y*v-z*s]
def rotate(q,p):
 v=cross(q[:3],p);vv=cross(q[:3],v)
 return add(p,mul(add(mul(v,q[3]),vv),2))
def from_basis(x,y,z):
 m=[[x[i],y[i],z[i]] for i in range(3)];tr=sum(m[i][i] for i in range(3))
 if tr>0:
  s=math.sqrt(tr+1)*2;q=[(m[2][1]-m[1][2])/s,(m[0][2]-m[2][0])/s,(m[1][0]-m[0][1])/s,s/4]
 else:
  i=max(range(3),key=lambda i:m[i][i]);j=(i+1)%3;k=(j+1)%3;s=math.sqrt(1+m[i][i]-m[j][j]-m[k][k])*2
  q=[0,0,0,0];q[i]=s/4;q[j]=(m[i][j]+m[j][i])/s;q[k]=(m[i][k]+m[k][i])/s;q[3]=(m[k][j]-m[j][k])/s
 return unit(q)
def tp(hand,grip,p):return add(hand['p'],rotate(hand['q'],add(grip['translation'],rotate(grip['quaternion'],p))))
def main():
 report={}
 for suffix in ['', '_Human']:
  idle=json.loads((OUT/('A_GreatSword_Idle'+suffix+'-poses.json')).read_text(encoding='utf-8-sig'))
  b=idle['samples'][0]['bones'];r,l=b['hand_r'],b['hand_l'];rp=mul(add(r['p'],b['middle_01_r']['p']),.5);lp=mul(add(l['p'],b['middle_01_l']['p']),.5)
  z=unit(sub(rp,lp));finger=unit(sub(b['middle_01_r']['p'],r['p']));y=unit(sub(finger,mul(z,dot(finger,z))));x=unit(cross(y,z));y=cross(z,x)
  local=lambda v:rotate(inv(r['q']),v)
  grip={'translation':local(sub(rp,r['p'])),'quaternion':from_basis(local(x),local(y),local(z)),'hand_distance_cm':math.dist(rp,lp),'basis_world':{'x':x,'y':y,'z':z},'right_hand':r,'right_palm':rp,'left_palm':lp}
  results=[]
  # Forward derived from horizontal head/foot geometry is unavailable; use the documented source +Y,
  # while human mesh's right/left shoulders orientation is reported separately in rendered runtime.
  for path in sorted(OUT.glob('A_GreatSword_*'+suffix+'-poses.json')):
   if not suffix and '_Human-' in path.name:continue
   data=json.loads(path.read_text(encoding='utf-8-sig'));rows=[]
   for sample in data['samples']:
    hand=sample['bones']['hand_r'];base=tp(hand,grip,[0,0,18]);tip=tp(hand,grip,[0,0,140])
    left=mul(add(sample['bones']['hand_l']['p'],sample['bones']['middle_01_l']['p']),.5)
    axis=unit(sub(tip,base));origin=tp(hand,grip,[0,0,0]);offhand=sub(left,origin)
    rows.append({'t':sample['t'],'base':base,'tip':tip,'offhand_axis_cm':dot(offhand,axis),'offhand_radial_error_cm':math.sqrt(max(0,dot(offhand,offhand)-dot(offhand,axis)**2))})
   speeds=[math.dist(a['tip'],b['tip'])/max(1e-6,b['t']-a['t']) for a,b in zip(rows,rows[1:])]
   result={'asset':data['asset'],'duration':data['duration'],'max_tip_speed_cm_s':max(speeds),'max_offhand_radial_error_cm':max(a['offhand_radial_error_cm'] for a in rows),'rows':rows}
   (OUT/(path.stem+'-blade.json')).write_text(json.dumps(result,indent=2),encoding='utf-8');results.append({k:v for k,v in result.items() if k!='rows'})
  report[suffix or 'Manny']={'grip':grip,'trajectories':results}
 (OUT/'grip-calibration.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps({k:v['grip'] for k,v in report.items()},indent=2))
if __name__=='__main__':main()
