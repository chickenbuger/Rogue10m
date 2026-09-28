"""Read-only IK goal/solver audit; no asset mutations or saves."""
from pathlib import Path
import json
import unreal

def main():
    rows=[]
    for path in ('/Game/Polyart/SharedResources/Characters/AnimationStuff/Retargeting/IK_Mannequin',
                 '/Game/Character/Customization/Retargeting/IK_Target_Hu_M'):
        rig=unreal.load_asset(path)
        if not rig:raise RuntimeError('Missing '+path)
        ctl=unreal.IKRigController.get_controller(rig)
        goals=list(ctl.get_all_goals())
        row={'asset':path,'goals':[],'solvers':[]}
        for goal in goals:
            row['goals'].append({name:str(goal.get_editor_property(name)) for name in
                ('goal_name','bone_name','position_alpha','rotation_alpha','initial_transform')})
        for i in range(ctl.get_num_solvers()):
            solver=ctl.get_solver_controller(i)
            data={'index':i,'enabled':ctl.get_solver_enabled(i),'class':solver.get_class().get_name()}
            if isinstance(solver,unreal.IKRigFBIKController):
                data['settings']=solver.get_solver_settings().export_text()
                data['goals']={str(g.get_editor_property('goal_name')):solver.get_goal_settings(g.get_editor_property('goal_name')).export_text() for g in goals}
                data['bones']={bone:solver.get_bone_settings(bone).export_text() for bone in
                    ('pelvis','thigh_l','calf_l','foot_l','ball_l','thigh_r','calf_r','foot_r','ball_r')}
            row['solvers'].append(data)
        rows.append(row)
    path=Path(unreal.Paths.project_dir())/'tmp/appearance-retarget/ik-goals.json'
    path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(rows,indent=2),encoding='utf8')
    unreal.log('RESULT=APPEARANCE_IK_GOALS_AUDIT_COMPLETE')
if __name__=='__main__':main()
