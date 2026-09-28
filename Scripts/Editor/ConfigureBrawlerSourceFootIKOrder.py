"""Move existing locomotion FootIK before DefaultSlot through UE Editor graph API.
Default is read-only preview. Apply requires -Rogue10mApplySourceIKOrder.
Guards three old links, retains all node settings, compiles, and saves only on success.
"""
from pathlib import Path
import hashlib
import json
import shutil
import unreal

ASSET = '/Game/Rogue10m/Animation/Common/ABP_Common_Unarmed'
PROJECT = Path(unreal.Paths.project_dir())
OUTPUT = PROJECT / 'Feature/doc/evidence/animation-stability-20260928/source-ik-order.json'
PIN = unreal.BlueprintGraphPinLibrary


def ident(pin):
    return (PIN.get_owning_node(pin).get_name(), str(PIN.get_pin_name(pin)), str(PIN.get_pin_direction(pin)))


def edge(a, b):
    return tuple(sorted((ident(a), ident(b))))


def all_edges(nodes):
    return {edge(pin, linked) for node in nodes for pin in unreal.BlueprintEditorLibrary.list_all_pins(node)
            for linked in PIN.list_connected_pins(pin)}


def one_pin(node, name, direction):
    found = [pin for pin in unreal.BlueprintEditorLibrary.list_all_pins(node)
             if str(PIN.get_pin_name(pin)) == name and PIN.get_pin_direction(pin) == direction]
    if len(found) != 1: raise RuntimeError('Expected unique pin: ' + node.get_name() + ':' + name)
    return found[0]


def only_link(pin):
    links = list(PIN.list_connected_pins(pin))
    if len(links) != 1: raise RuntimeError('Expected exactly one link: ' + str(ident(pin)))
    return links[0]


def main(apply=False):
    bp = unreal.load_asset(ASSET)
    if not bp: raise RuntimeError('Missing blueprint')
    graph = next(g for g in unreal.BlueprintEditorLibrary.list_graphs(bp) if g.get_name() == 'AnimGraph')
    editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
    nodes = list(editor.list_all_nodes())
    def unique(class_name):
        found = [n for n in nodes if n.get_class().get_name() == class_name]
        if len(found) != 1: raise RuntimeError('Expected unique node: ' + class_name)
        return found[0]
    slot, rig, root = unique('AnimGraphNode_Slot'), unique('AnimGraphNode_ControlRig'), unique('AnimGraphNode_Root')
    if str(slot.get_editor_property('node').get_editor_property('slot_name')) != 'DefaultSlot':
        raise RuntimeError('Unexpected slot name')
    if 'CR_Mannequin_FootIK' not in rig.get_editor_property('node').export_text():
        raise RuntimeError('Unexpected ControlRig')
    input_dir, output_dir = unreal.EdGraphPinDirection.EGPD_INPUT, unreal.EdGraphPinDirection.EGPD_OUTPUT
    si, so = one_pin(slot, 'Source', input_dir), one_pin(slot, 'Pose', output_dir)
    ri, ro = one_pin(rig, 'Source', input_dir), one_pin(rig, 'Pose', output_dir)
    end = one_pin(root, 'Result', input_dir)
    before = all_edges(nodes)
    already = PIN.is_same_native_pin(only_link(si), ro) and PIN.is_same_native_pin(only_link(end), so)
    if already:
        unreal.log('SOURCE_IK_ORDER_ALREADY_APPLIED')
        return
    upstream = only_link(si)
    if PIN.get_pin_direction(upstream) != output_dir: raise RuntimeError('Invalid upstream direction')
    if not PIN.is_same_native_pin(only_link(so), ri) or not PIN.is_same_native_pin(only_link(ri), so):
        raise RuntimeError('Expected Slot -> FootIK link')
    if not PIN.is_same_native_pin(only_link(ro), end) or not PIN.is_same_native_pin(only_link(end), ro):
        raise RuntimeError('Expected FootIK -> Root link')
    old_pairs = [(upstream, si), (so, ri), (ro, end)]
    new_pairs = [(upstream, ri), (ro, si), (so, end)]
    expected = (before - {edge(a,b) for a,b in old_pairs}) | {edge(a,b) for a,b in new_pairs}
    report = {'asset': ASSET, 'apply': apply, 'before_edges': sorted(before), 'expected_edges': sorted(expected),
              'nodes_before': sorted(n.get_name() for n in nodes), 'removed_edges': [edge(a,b) for a,b in old_pairs],
              'added_edges': [edge(a,b) for a,b in new_pairs], 'compile_pass': None, 'saved': False}
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    if not apply:
        OUTPUT.with_name('source-ik-order-preview.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        unreal.log('SOURCE_IK_ORDER_PREVIEW_PASS')
        return
    asset_file = PROJECT / 'Content/Rogue10m/Animation/Common/ABP_Common_Unarmed.uasset'
    backup = PROJECT / 'tmp/animation-stability/source-abp-before/ABP_Common_Unarmed.uasset'
    digest = hashlib.sha256(asset_file.read_bytes()).hexdigest()
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists():
        if hashlib.sha256(backup.read_bytes()).hexdigest() != digest: raise RuntimeError('Existing backup differs; refusing overwrite')
    else: shutil.copy2(asset_file, backup)
    report['backup'] = str(backup)
    report['before_sha256'] = digest
    original_properties = {n.get_name(): n.get_editor_property('node').export_text() for n in (slot, rig)}
    with unreal.ScopedEditorTransaction('Move locomotion FootIK before attack slot'):
        try:
            for a,b in old_pairs:
                if not PIN.break_single_pin_link(a,b): raise RuntimeError('Failed to break expected old link')
            for a,b in new_pairs:
                if not PIN.try_create_connection(a,b): raise RuntimeError('Failed to create new pose link')
            after_nodes = list(editor.list_all_nodes())
            if sorted(n.get_name() for n in after_nodes) != report['nodes_before']: raise RuntimeError('Unexpected node addition/removal')
            if all_edges(after_nodes) != expected: raise RuntimeError('Unexpected graph edge changes')
            for n in (slot,rig):
                if n.get_editor_property('node').export_text() != original_properties[n.get_name()]:
                    raise RuntimeError('Unexpected animation node property mutation')
            if not unreal.BlueprintEditorLibrary.compile_blueprint(bp): raise RuntimeError('Blueprint compile failed')
            report['compile_pass'] = True
            report['error_nodes'] = [n.get_name() for n in editor.list_nodes_with_errors()]
            if report['error_nodes']: raise RuntimeError('Graph has compiler error nodes')
            report['warning_nodes'] = [n.get_name() for n in editor.list_nodes_with_warnings()]
            if all_edges(list(editor.list_all_nodes())) != expected: raise RuntimeError('Compile changed graph wiring')
            if not unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False): raise RuntimeError('Save failed')
            report['saved'] = True
            report['after_edges'] = sorted(all_edges(list(editor.list_all_nodes())))
            report['after_sha256'] = hashlib.sha256(asset_file.read_bytes()).hexdigest()
        except Exception as error:
            report['error'] = str(error)
            if not report['saved']:
                for a,b in new_pairs: PIN.break_single_pin_link(a,b)
                for a,b in old_pairs: PIN.try_create_connection(a,b)
                report['rollback_links_verified'] = all_edges(list(editor.list_all_nodes())) == before
                report['rollback_compile_pass'] = unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            OUTPUT.write_text(json.dumps(report,indent=2),encoding='utf-8')
            raise
    OUTPUT.write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log('SOURCE_IK_ORDER_APPLY_PASS')


if __name__ == '__main__':
    try: main('-Rogue10mApplySourceIKOrder' in unreal.SystemLibrary.get_command_line())
    finally: unreal.SystemLibrary.quit_editor()
