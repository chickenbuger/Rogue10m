"""Apply the guarded native AnimGraph edit, or reload and audit it read-only.

Run in an editor process with -RogueMovingAttackApply to apply. The default
performs no asset save. Compilation and precise graph mutation belong to the
editor-only native authoring command. Never write .uasset bytes here.
"""
import json
import sys
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
EVIDENCE = ROOT / 'Feature/doc/evidence/moving-upper-body-attack-20261009'
sys.path.insert(0, str(ROOT / 'Scripts/Editor'))
import PrepareFighterCombatStance as stance


def connected(input_pin, output):
    return stance.PIN.is_same_native_pin(stance.one_link(input_pin), output)


def alpha_blend(nodes, variable):
    found = []
    for node in nodes:
        if not isinstance(node, unreal.AnimGraphNode_TwoWayBlend):
            continue
        upstream = stance.one_link(stance.one_pin(node, 'Alpha', stance.INPUT))
        if str(stance.PIN.get_pin_name(upstream)) == variable:
            found.append(node)
    if len(found) != 1:
        raise RuntimeError('Expected unique blend driven by ' + variable)
    return found[0]


def validate_graph():
    bp, editor = stance.graph_context()
    nodes = list(editor.list_all_nodes())
    # UE's Python wrappers omit the weak SaveCachedPoseNode property. Read its
    # actual identity through the native read-only auditor, never a cached report.
    native_path = EVIDENCE / 'native-audit.json'
    old_mtime = native_path.stat().st_mtime_ns if native_path.exists() else None
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, 'Rogue10m.AuditMovingAttackUpperBody')
    if not native_path.exists() or native_path.stat().st_mtime_ns == old_mtime:
        raise RuntimeError('Native read-only graph auditor did not write a fresh result')
    native = json.loads(native_path.read_text(encoding='utf-8-sig'))
    if native.get('result') != 'PASSED':
        raise RuntimeError('Native graph audit failed: ' + native.get('error', 'unknown'))
    if not native.get('read_only') or native.get('compiled') or native.get('changed') or native.get('saved'):
        raise RuntimeError('Native graph audit was not read-only')
    if native.get('package_dirty_before') != native.get('package_dirty_after'):
        raise RuntimeError('Native graph audit changed package dirtiness')
    cache_targets = {row['name']: row['save_cached_pose_node'] for row in native['graph']['nodes']
                     if 'save_cached_pose_node' in row}
    by_name = {n.get_name(): n for n in nodes}
    def saved_pose(consumer):
        target = by_name.get(cache_targets.get(consumer.get_name()))
        if not isinstance(target, unreal.AnimGraphNode_SaveCachedPose):
            raise RuntimeError('Invalid native saved cache target for ' + consumer.get_name())
        return target
    slot = stance.unique(nodes, 'AnimGraphNode_Slot')
    root = stance.unique(nodes, 'AnimGraphNode_Root')
    layer = stance.unique(nodes, 'AnimGraphNode_LayeredBoneBlend')
    guard = alpha_blend(nodes, 'FighterCombatStanceAlpha')
    final = alpha_blend(nodes, 'MovingAttackUpperBodyAlpha')
    if str(slot.get_editor_property('node').get_editor_property('slot_name')) != 'DefaultSlot':
        raise RuntimeError('Existing montage slot changed')
    if not connected(stance.one_pin(root, 'Result', stance.INPUT), stance.one_pin(final, 'Pose', stance.OUTPUT)):
        raise RuntimeError('Upper body selection does not drive Root')
    if not connected(stance.one_pin(final, 'B', stance.INPUT), stance.one_pin(layer, 'Pose', stance.OUTPUT)):
        raise RuntimeError('Masked moving pose does not drive selection B')
    source_nodes = [stance.PIN.get_owning_node(stance.one_link(stance.one_pin(n, p, stance.INPUT)))
                    for n, p in [(slot, 'Source'), (layer, 'BasePose')]]
    if any(not isinstance(n, unreal.AnimGraphNode_UseCachedPose) for n in source_nodes):
        raise RuntimeError('Locomotion is not shared through cached pose consumers')
    saved_locomotion = [saved_pose(n) for n in source_nodes]
    if saved_locomotion[0] is None or saved_locomotion[0] != saved_locomotion[1]:
        raise RuntimeError('The two locomotion consumers do not share the same saved pose')
    if not connected(stance.one_pin(saved_locomotion[0], 'Pose', stance.INPUT),
                     stance.one_pin(guard, 'Pose', stance.OUTPUT)):
        raise RuntimeError('Shared locomotion cache does not preserve the original guard')
    rig = stance.unique(nodes, 'AnimGraphNode_ControlRig')
    if not connected(stance.one_pin(guard, 'A', stance.INPUT), stance.one_pin(rig, 'Pose', stance.OUTPUT)):
        raise RuntimeError('FootIK is not upstream of the guard')
    if 'CR_Mannequin_FootIK' not in rig.get_editor_property('node').export_text():
        raise RuntimeError('Original foot IK ControlRig changed')
    attack_pins = [p for p in unreal.BlueprintEditorLibrary.list_all_pins(layer)
                   if str(stance.PIN.get_pin_name(p)).startswith('BlendPoses')
                   and stance.PIN.get_pin_direction(p) == stance.INPUT]
    if len(attack_pins) != 1:
        raise RuntimeError('Expected exactly one upper-body montage layer')
    attack_consumers = [stance.PIN.get_owning_node(stance.one_link(p))
                        for p in [attack_pins[0], stance.one_pin(final, 'A', stance.INPUT)]]
    if any(not isinstance(n, unreal.AnimGraphNode_UseCachedPose) for n in attack_consumers):
        raise RuntimeError('Montage slot result must be shared to avoid duplicate slot evaluation')
    saved_attack = [saved_pose(n) for n in attack_consumers]
    if saved_attack[0] is None or saved_attack[0] != saved_attack[1] or saved_attack[0] == saved_locomotion[0]:
        raise RuntimeError('Invalid shared montage cache')
    if not connected(stance.one_pin(saved_attack[0], 'Pose', stance.INPUT), stance.one_pin(slot, 'Pose', stance.OUTPUT)):
        raise RuntimeError('Shared attack cache is not the original montage slot')
    if unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp).get_path_name() != stance.PARENT:
        raise RuntimeError('Unexpected source native parent')
    runtime = layer.get_editor_property('node')
    filters = runtime.get_editor_property('layer_setup')[0].get_editor_property('branch_filters')
    branches = [(str(f.get_editor_property('bone_name')), int(f.get_editor_property('blend_depth'))) for f in filters]
    if branches != [('spine_01', 1), ('ik_hand_root', 1)]:
        raise RuntimeError('Unexpected upper body mask ' + str(branches))
    if not runtime.get_editor_property('mesh_space_rotation_blend'):
        raise RuntimeError('Mesh space rotation blending disabled')
    if list(runtime.get_editor_property('blend_weights')) != [1.0]:
        raise RuntimeError('Upper-body montage layer weight changed')
    if runtime.get_editor_property('root_space_rotation_blend') or runtime.get_editor_property('mesh_space_scale_blend'):
        raise RuntimeError('Unexpected upper-body rotation/scale mode')
    if not runtime.get_editor_property('blend_root_motion_based_on_root_bone'):
        raise RuntimeError('Root motion weighting changed')
    if editor.list_nodes_with_errors():
        raise RuntimeError('Saved graph compiler errors')
    return {'blueprint': stance.BLUEPRINT, 'parent': unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp).get_path_name(),
            'branches': branches, 'slot': slot.get_name(), 'guard': guard.get_name(),
            'locomotion_cache': saved_locomotion[0].get_name(), 'montage_cache': saved_attack[0].get_name(),
            'selection': final.get_name(), 'layer': layer.get_name(), 'graph': stance.graph_dump(editor),
            'errors': [], 'warnings': [n.get_name() for n in editor.list_nodes_with_warnings()]}


def main():
    apply = '-RogueMovingAttackApply' in unreal.SystemLibrary.get_command_line()
    report = {'apply': apply, 'saved': False, 'status': 'failed'}
    try:
        if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
            raise RuntimeError('Exit PIE before editing the AnimGraph')
        if apply:
            checkpoint = EVIDENCE / 'checkpoint/ABP_Common_Unarmed.uasset'
            if not checkpoint.is_file():
                raise RuntimeError('Current AnimBP checkpoint missing')
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            native_path = EVIDENCE / 'native-authoring.json'
            old_mtime = native_path.stat().st_mtime_ns if native_path.exists() else None
            unreal.SystemLibrary.execute_console_command(world, 'Rogue10m.AuthorMovingAttackUpperBody')
            if not native_path.exists() or native_path.stat().st_mtime_ns == old_mtime:
                raise RuntimeError('Native command did not write a fresh result')
            native = json.loads(native_path.read_text(encoding='utf-8-sig'))
            report['native'] = native
            if native.get('result') != 'PASSED' or not native.get('compile_passed'):
                raise RuntimeError('Native authoring/compile failed: ' + native.get('error', 'unknown'))
        report['graph'] = validate_graph()
        if apply:
            bp, _ = stance.graph_context()
            if not unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False):
                raise RuntimeError('Verified AnimBP save failed')
            report['saved'] = True
        report['status'] = 'passed'
    except Exception:
        report['exception'] = traceback.format_exc()
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / ('graph-apply.json' if apply else 'graph-reload.json')).write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    if report['status'] != 'passed':
        unreal.log_error(report['exception'])
    else:
        unreal.log('RESULT=MOVING_ATTACK_GRAPH_' + ('APPLY' if apply else 'RELOAD') + '_PASSED')
    if '-RogueMovingAttackKeepEditor' not in unreal.SystemLibrary.get_command_line():
        unreal.SystemLibrary.execute_console_command(
            unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), 'QUIT_EDITOR')
    return report


if __name__ == '__main__':
    main()
