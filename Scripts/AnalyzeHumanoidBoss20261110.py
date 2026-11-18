"""Summarize actual boss runtime frames, keeping numeric and visual acceptance separate."""
import argparse,csv,json,math,re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--label',required=True);p.add_argument('--binary-label',required=True)
a=p.parse_args()
if not re.fullmatch(r'[A-Za-z0-9_]{1,24}',a.label): raise ValueError('Invalid QA label')
root=Path(__file__).resolve().parents[1]
d=root/'Feature/doc/evidence/boss-readability-20261103'/a.label
out=root/'Feature/doc/evidence/humanoid-boss-npc-20261110'/a.label
rows=list(csv.DictReader((d/'animation-frames.csv').open(encoding='utf-8-sig')))
frames=list(csv.DictReader((d/'frames.csv').open(encoding='utf-8-sig')))
cases=[]
for key in sorted({r['case'] for r in rows}):
    current=[r for r in rows if r['case']==key]
    contact=next((r for r in current if r['state']=='2'),None)
    authored=.5 if key.startswith('basic') else 2.2333333 if key.startswith('instant_wave') else 3.1666667
    f=[r for r in frames if r['case']==key]
    lead=1.0 if key.startswith('basic') else 1.6 if key.startswith('instant_wave') else 1.9
    if 'enraged' in key and not key.startswith('basic'): lead*=.75
    windup=[r for r in current if r['state']=='1']
    pre_error=max((abs(float(r['position'])-authored*float(r['elapsed_s'])/lead) for r in windup),default=None)
    movement=max(max(float(r['left_'+x]) for r in current)-min(float(r['left_'+x]) for r in current) for x in 'xyz')
    cases.append({'case':key,'frames':len(current),'observed_contact_s':float(contact['elapsed_s']) if contact else None,
      'source_contact_s':authored,'montage_contact_position_s':float(contact['position']) if contact else None,
      'position_error_s':abs(float(contact['position'])-authored) if contact else None,
      'hand_l_span_cm':{x:max(float(r['left_'+x]) for r in current)-min(float(r['left_'+x]) for r in current) for x in 'xyz'},
      'boss_move_max_cm':max(float(r['boss_move_cm']) for r in f),'max_pre_contact_position_error_s':pre_error,'meaningful_hand_motion':movement>5.0})
cols=['position','rate','left_x','left_y','left_z','right_x','right_y','right_z','foot_l_x','foot_l_y','foot_l_z','foot_r_x','foot_r_y','foot_r_z']
report={'binary_label':a.binary_label,'input':'programmatic Controller InputKey','frame_count':len(rows),'case_count':len(cases),
    'all_coordinates_finite':all(math.isfinite(float(r[k])) for r in rows for k in cols),
    'max_contact_position_error_s':max((c['position_error_s'] for c in cases if c['position_error_s'] is not None),default=None),
    'max_boss_actor_motion_cm':max((c['boss_move_max_cm'] for c in cases),default=None),
    'png_count':len(list((root/('DevLog/20261103_BossReadability_'+a.label)).glob('*.png'))),
    'max_pre_contact_position_error_s':max((c['max_pre_contact_position_error_s'] for c in cases if c['max_pre_contact_position_error_s'] is not None),default=None),
    'all_cases_meaningful_hand_motion':all(c['meaningful_hand_motion'] for c in cases),
    'visual_status':'requires separate actual PNG review; contact snap precision alone is not continuity evidence','cases':cases}
(out/'animation-analysis.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k!='cases'},indent=2))
if not report['all_coordinates_finite'] or not report['all_cases_meaningful_hand_motion'] or len(cases)!=18 or any(c['position_error_s'] is None or c['position_error_s']>.15 or c['max_pre_contact_position_error_s'] is None or c['max_pre_contact_position_error_s']>.15 for c in cases): raise SystemExit(1)
