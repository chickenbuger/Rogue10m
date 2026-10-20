"""Independent UE TwoBoneIK position reproduction using retained source guard CS.
No Unreal asset writes. Reproduces flurry boundary jump and isolates Rest basis.
"""
import json, math
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[4]
source=ROOT/'Feature/doc/evidence/animation-stability-20260928/boxing-stability.json'
x=json.loads(source.read_text(encoding='utf-8-sig'))['clips']['appearance_left']['frames'][0]['bones']
v=lambda n:np.array(x[n]['world']['position'])
origin=v('spine_03'); first=.18; spacing=.09300000441074374
smooth=lambda a:(lambda b:b*b*b*(b*(b*6-15)+10))(min(1,max(0,a)))
def sample(t,side,current_rest):
    acts=[0.,0.];ph=[0.,0.]
    for k in range(6):
        c=first+k*spacing;p=(t-c)/(spacing*(.82 if t<c else .90));a=smooth(p+1) if p<0 else 1-smooth(p)
        if a>acts[k%2]:acts[k%2]=a;ph[k%2]=p
    z=math.radians((acts[1]-acts[0])*4)
    R=np.array([[math.cos(z),-math.sin(z),0],[math.sin(z),math.cos(z),0],[0,0,1]])
    suffix='l' if side==0 else 'r';sgn=1 if side==0 else -1
    a,b,c=[origin+R@(v(n+'_'+suffix)-origin) for n in ['upperarm','lowerarm','hand']];ac=acts[side]
    if ac<=0:return b,R.T@(b-origin)
    off=np.array([-sgn*16+sgn*min(1,max(-1,-ph[side]))*13,38,-8])
    rest=c if current_rest else v('hand_'+suffix)
    goal=rest*(1-ac)+(a+off)*ac
    u=np.linalg.norm(b-a);l=np.linalg.norm(c-b);delta=goal-a
    goal=a+delta*min(1,(u+l)*.94/np.linalg.norm(delta));d=np.linalg.norm(goal-a);direction=(goal-a)/d
    pole=b*(1-ac)+(a+np.array([sgn*32,8,-45]))*ac
    bd=pole-a-np.dot(pole-a,direction)*direction;bd/=np.linalg.norm(bd)
    proj=(u*u+d*d-l*l)/(2*d)
    j=a+proj*direction+math.sqrt(max(0,u*u-proj*proj))*bd
    return j,R.T@(j-origin)
report={}
for fixed in [False,True]:
    rows=[]
    for k in range(6):
        for label,t in [('start',first+k*spacing-.82*spacing),('end',first+k*spacing+.90*spacing)]:
            lo=math.floor((t-1e-7)*120)/120;hi=math.ceil((t+1e-7)*120)/120
            a,b=sample(lo,k%2,fixed),sample(hi,k%2,fixed)
            e1,e2=sample(t-1e-6,k%2,fixed),sample(t+1e-6,k%2,fixed)
            rows.append(dict(index=k,boundary=label,lo=lo,hi=hi,world_step_cm=float(np.linalg.norm(a[0]-b[0])),
                parent_relative_step_cm=float(np.linalg.norm(a[1]-b[1])),epsilon_world_step_cm=float(np.linalg.norm(e1[0]-e2[0]))))
    report['current_hand_rest' if fixed else 'fixed_guard_rest']={'max_world_step_cm':max(r['world_step_cm'] for r in rows),
        'max_epsilon_world_step_cm':max(r['epsilon_world_step_cm'] for r in rows),'boundaries':rows}
report['scope']='Independent positional triangle math with retained source guard. Not a fresh Unreal runtime run; no rotational error assertions.'
Path(__file__).with_suffix('.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
for k,v in report.items():
    if isinstance(v,dict):print(k,v['max_world_step_cm'],v['max_epsilon_world_step_cm'])
