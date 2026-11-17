"""Activate visually reviewed project-owned sword montage cuts and measured first-contact delay only.
The original Mixamo clips, normal body retargeter and every skill balance/slot/VFX field stay unchanged.
"""
from pathlib import Path
import json,runpy,math,unreal
P=Path(unreal.Paths.project_dir()).resolve();OUT=P/'Feature/doc/evidence/sword-knuckle-20261110';R='/Game/Rogue10m/Animation/Combat/GreatSword20261110'
def need(path):
 obj=unreal.load_asset(path)
 if not obj:raise RuntimeError('Missing '+path)
 return obj
def save(obj):
 if not unreal.EditorAssetLibrary.save_loaded_asset(obj,False):raise RuntimeError('Save failed '+obj.get_path_name())
def main():
 plan=json.loads((OUT/'greatsword-activation-plan.json').read_text(encoding='utf-8-sig'))
 if not plan.get('visual_review_completed') or not plan.get('preview_evidence'):raise RuntimeError('Actual pose preview review is required before activation')
 guard=runpy.run_path(str(P/'Scripts/Editor/PrepareWeaponFantasyVFX20261110.py'))['snapshot']
 entries=plan['skills'];names=['Primary01','Primary02','Primary03','Special','Charged']
 if sorted(entries)!=sorted(names):raise RuntimeError('Expected exact five existing sword skills')
 rows=[];requests=[]
 for name in names:
  data=entries[name];seq=need(R+'/A_GreatSword_'+data['source']);duration=seq.get_play_length();start=float(data.get('start',0));end=float(data.get('end',duration));hit=float(data['contact'])-start
  if not all(math.isfinite(x) for x in [start,end,hit]) or not 0<=start<end<=duration+.0001 or not 0<=hit<end-start:raise RuntimeError('Invalid source cut/contact '+name)
  skill=need('/Game/DataAsset/AttackSkill/ClassCombat/GreatSword/DA_Attack_GreatSword_'+name)
  rows.append({'name':name,'skill':skill,'before':guard(skill),'source':seq.get_path_name(),'start':start,'end':end,'hit_delay':hit})
  requests.append(R+'/AM_GreatSword_'+name+'|'+R+'/A_GreatSword_'+data['source']+'|'+str(start)+'|'+str(end))
 (OUT/'sword-preactivation-fields.json').write_text(json.dumps([{k:v for k,v in row.items() if k!='skill'} for row in rows],ensure_ascii=False,indent=2),encoding='utf-8')
 (OUT/'montage-requests.txt').write_text('\n'.join(requests),encoding='utf-8')
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();unreal.SystemLibrary.execute_console_command(world,'Rogue10m.ConfigureWeaponAnimations')
 native=json.loads((OUT/'weapon-animation-native.json').read_text(encoding='utf-8-sig'))
 if not native.get('passed'):raise RuntimeError('Native montage/graph setup failed '+str(native))
 for row in rows:
  montage=need(R+'/AM_GreatSword_'+row['name']);montage.set_editor_property('rate_scale',1.0)
  if abs(montage.get_play_length()-(row['end']-row['start']))>.0002:raise RuntimeError('Stale montage length '+row['name'])
  skill=row['skill'];skill.set_editor_properties({'attack_montage':montage,'hit_start_delay_seconds':row['hit_delay']})
  row['after']=guard(skill);row['changed_fields']=sorted(k for k in row['before'] if row['before'][k]!=row['after'][k])
  if set(row['changed_fields'])-{'attack_montage','hit_start_delay_seconds'}:raise RuntimeError('Protected skill field changed '+str(row['changed_fields']))
 for row in rows:save(need(R+'/AM_GreatSword_'+row['name']));save(row['skill'])
 save(need('/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed'));save(need(R+'/BS_GreatSword_Ground'))
 report={'result':'active_pending_rendered_runtime','preview_evidence':plan['preview_evidence'],'skills':[{k:v for k,v in row.items() if k!='skill'} for row in rows],'normal_human_retargeter_changed':False,'knuckle_motion_changed':False,'physical_weapon_collision':False}
 (OUT/'greatsword-activation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 print('RESULT=GREATSWORD_ANIMATIONS_ACTIVATED')
if __name__=='__main__':
 try:main()
 finally:unreal.SystemLibrary.quit_editor()
