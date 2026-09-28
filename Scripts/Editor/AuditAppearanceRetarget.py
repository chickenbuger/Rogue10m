"""Read-only audit of live Appearance retarget assets. Never edits or saves Unreal assets."""
from pathlib import Path
import json
import unreal

ROOT = '/Game/Character/Customization/Retargeting'
CODES = ('Hu_M', 'Hu_F', 'Dw_M', 'Dw_F', 'Or_M', 'Or_F')
BONES = ('root','pelvis','spine_01','spine_02','spine_03','neck_01','head',
         'clavicle_l','upperarm_l','lowerarm_l','hand_l','clavicle_r','upperarm_r','lowerarm_r','hand_r',
         'thigh_l','calf_l','foot_l','ball_l','thigh_r','calf_r','foot_r','ball_r')

def readable(value):
    if value is None or isinstance(value, (bool, int, float, str)): return value
    if hasattr(value, 'get_path_name'): return value.get_path_name()
    if isinstance(value, (list, tuple)): return [readable(x) for x in value]
    if hasattr(value, 'export_text'):
        try: return value.export_text()
        except Exception: pass
    return str(value)

def attempt(call):
    try: return readable(call())
    except Exception as exc: return {'unavailable': str(exc)}

def prop(obj, name): return attempt(lambda: obj.get_editor_property(name))

def graph_audit(path):
    asset = unreal.load_asset(path)
    if not asset: return {'missing': path}
    out = {'asset': path, 'graphs': []}
    for graph in unreal.BlueprintEditorLibrary.list_graphs(asset):
        editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
        nodes = []
        for node in editor.list_all_nodes():
            row = {'name':node.get_name(), 'class':node.get_class().get_name()}
            if isinstance(node, unreal.AnimGraphNode_RetargetPoseFromMesh):
                runtime = node.get_editor_property('node')
                row['settings'] = {name:prop(runtime, name) for name in
                    ('ik_retargeter_asset','retarget_from','lod_threshold','lod_threshold_for_ik','custom_retarget_profile','use_custom_override_sets')}
            nodes.append(row)
        out['graphs'].append({'name':graph.get_name(),'nodes':nodes})
    return out

def audit():
    report={'read_only':True, 'engine':unreal.SystemLibrary.get_engine_version(), 'retargeters':[]}
    for code in CODES:
        path=f'{ROOT}/IKR_Manny_To_{code}'
        asset=unreal.load_asset(path)
        if not asset:
            report['retargeters'].append({'asset':path,'missing':True}); continue
        controller=unreal.IKRetargeterController.get_controller(asset)
        row={'asset':path,'ops':[],'rigs':{},'poses':{}}
        for index in range(controller.get_num_retarget_ops()):
            op=controller.get_op_controller(index)
            row['ops'].append({'index':index,'name':str(controller.get_op_name(index)),
                'enabled':controller.get_retarget_op_enabled(index),
                'controller_class':op.get_class().get_name() if op else None,
                'settings':attempt(lambda op=op:op.get_settings())})
        for label,side in (('source',unreal.RetargetSourceOrTarget.SOURCE),('target',unreal.RetargetSourceOrTarget.TARGET)):
            rig=controller.get_ik_rig(side)
            rig_controller=unreal.IKRigController.get_controller(rig)
            chains=[]
            for chain in rig_controller.get_retarget_chains():
                name=chain.get_editor_property('chain_name')
                entry={key:prop(chain,key) for key in ('chain_name','start_bone','end_bone','ik_goal_name')}
                if label=='target': entry['source_chain']=attempt(lambda name=name:controller.get_source_chain(name))
                chains.append(entry)
            row['rigs'][label]={'asset':readable(rig),'root':attempt(rig_controller.get_retarget_root),
                'chains':chains,'num_solvers':rig_controller.get_num_solvers(),
                'solvers':[{'index':i,'enabled':rig_controller.get_solver_enabled(i),
                    'controller':attempt(lambda i=i:rig_controller.get_solver_controller(i))} for i in range(rig_controller.get_num_solvers())]}
            row['poses'][label]={'current':str(controller.get_current_retarget_pose_name(side)),
                'pose':attempt(lambda side=side:controller.get_current_retarget_pose(side)),
                'bone_offsets':{bone:attempt(lambda bone=bone,side=side:controller.get_rotation_offset_for_retarget_pose_bone(bone,side)) for bone in BONES}}
        row['anim_blueprint']=attempt(lambda code=code:graph_audit(f'{ROOT}/ABP_Retarget_{code}'))
        report['retargeters'].append(row)
    report['source_blueprint']=attempt(lambda:graph_audit('/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed'))
    destination=Path(unreal.Paths.project_dir())/'tmp/appearance-retarget/retarget-audit.json'
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log(f'RESULT=APPEARANCE_RETARGET_AUDIT_COMPLETE output={destination}')

if __name__=='__main__': audit()
