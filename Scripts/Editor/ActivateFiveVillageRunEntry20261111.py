"""Add a separate prototype hub entry after the editor/build owner activates it.

No Unreal import or editor mutation happens at import time. The existing hub
portal, prototype world, camera and production sources are never replaced.
"""
from __future__ import annotations

import datetime
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HUB = '/Game/Rogue10m/Maps/StartHub'
CANDIDATE = '/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110'
PORTAL_CLASS = '/Script/Rogue10m.Rogue10mFiveVillageRunPortal'
MODE_CLASS = '/Script/Rogue10m.Rogue10mFiveVillageGameMode'
TAG = 'Rogue10m.FiveVillageRuntime20261111.Portal'
EVIDENCE = ROOT / 'Feature/doc/evidence/five-village-runtime-20261111'


def activate(*, editor_handoff: str, build_verified: bool = False):
    if not editor_handoff.strip() or not build_verified:
        raise RuntimeError('The shared editor owner must confirm handoff and the new-class build first.')
    import unreal

    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    report_path = EVIDENCE / ('entry-activation-' + stamp + '.json')
    report = {'status': 'preflight', 'editor_handoff': editor_handoff,
              'new_classes_build_confirmed_by_owner': build_verified,
              'hub': HUB, 'candidate': CANDIDATE, 'tag': TAG,
              'production_entry_runtime': 'not_tested', 'physical_input': 'not_tested',
              'boss_navigation_services': 'not_enabled_by_this_script'}
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    def save_report():
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

    def dirty():
        u = unreal.EditorLoadingAndSavingUtils
        return [p.get_path_name() for p in list(u.get_dirty_map_packages()) + list(u.get_dirty_content_packages())]

    def snapshot(portal):
        location = portal.get_actor_location()
        rotation = portal.get_actor_rotation()
        scale = portal.get_actor_scale3d()
        return {'path': portal.get_path_name(), 'target': str(portal.get_editor_property('target_level_name')),
                'start_run': bool(portal.get_editor_property('start_run_timer_on_travel')),
                'options': str(portal.get_editor_property('additional_travel_options')),
                'location': [location.x, location.y, location.z],
                'rotation': [rotation.pitch, rotation.yaw, rotation.roll],
                'scale': [scale.x, scale.y, scale.z]}

    def owned_portals():
        return [a for a in actors.get_all_level_actors() if TAG in map(str, a.get_editor_property('tags'))]

    def originals():
        return [a for a in actors.get_all_level_actors() if isinstance(a, unreal.Rogue10mRunPortal)
                and TAG not in map(str, a.get_editor_property('tags'))
                and 'Rogue10m.StartVillage' in map(str, a.get_editor_property('tags'))]

    try:
        project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
        if project != (ROOT / 'Rogue10m.uproject').resolve():
            raise RuntimeError('Wrong project.')
        world = worlds.get_editor_world()
        if not world or world.get_path_name().split('.')[0] != HUB or levels.is_in_play_in_editor() or dirty():
            raise RuntimeError('A clean, idle StartHub editor world is required; no map is switched automatically.')
        if owned_portals():
            raise RuntimeError('A candidate portal already exists. Inspect it instead of creating a duplicate.')
        existing = originals()
        if len(existing) != 1:
            raise RuntimeError('Expected exactly one original project-owned hub portal.')
        before = snapshot(existing[0])
        report['original_portal_before'] = before
        candidate_file = ROOT / 'Content/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110.umap'
        if not candidate_file.exists() or not unreal.EditorAssetLibrary.does_asset_exist(CANDIDATE):
            raise RuntimeError('The saved graybox candidate is unavailable.')
        report['candidate_sha256_before'] = hashlib.sha256(candidate_file.read_bytes()).hexdigest()
        portal_class = unreal.load_class(None, PORTAL_CLASS)
        mode_class = unreal.load_class(None, MODE_CLASS)
        component_class = unreal.load_class(None, '/Script/Rogue10m.Rogue10mFiveVillageProgressComponent')
        if not portal_class or not mode_class or not component_class:
            raise RuntimeError('New runtime classes are unavailable. The editor/build owner must rebuild and restart.')
        cdo = unreal.get_default_object(portal_class)
        if (str(cdo.get_editor_property('target_level_name')) != CANDIDATE
                or not cdo.get_editor_property('start_run_timer_on_travel')
                or str(cdo.get_editor_property('additional_travel_options')) != 'game=' + MODE_CLASS):
            raise RuntimeError('Portal defaults do not match the reviewed route/travel contract.')
        # Both portal frames lie in local X-Z. Local X is their lateral axis;
        # actor right (+Y) would put the new gate behind the old trigger.
        location = existing[0].get_actor_location() + existing[0].get_actor_forward_vector() * 600.0
        ground = unreal.SystemLibrary.line_trace_single(
            world, location + unreal.Vector(0, 0, 2000), location - unreal.Vector(0, 0, 5000),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [existing[0]], unreal.DrawDebugTrace.NONE, True)
        if ground is None:
            raise RuntimeError('No ground at the separate portal position.')
        hit = ground.to_tuple()
        if (not hit[0] or hit[1] or hit[7].z < 0.8
                or not isinstance(hit[9], (unreal.StaticMeshActor, unreal.LandscapeProxy))):
            raise RuntimeError('The proposed portal position is not a verified walkable static surface.')
        point = hit[5]
        passage_axis = existing[0].get_actor_right_vector()
        clearance = unreal.SystemLibrary.capsule_trace_single(
            world, point + unreal.Vector(0, 0, 105) - passage_axis * 300.0,
            point + unreal.Vector(0, 0, 105) + passage_axis * 300.0,
            42.0, 96.0, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False,
            [existing[0]], unreal.DrawDebugTrace.NONE, True)
        if clearance is not None and clearance.to_tuple()[0]:
            raise RuntimeError('The separate entry position is obstructed; no portal was spawned.')
        if dirty():
            raise RuntimeError('Unrelated work became dirty before portal creation.')
        portal = actors.spawn_actor_from_class(portal_class, point + unreal.Vector(0, 0, 180),
                                              existing[0].get_actor_rotation())
        if not portal:
            raise RuntimeError('Could not spawn the separate portal.')
        portal.set_actor_label('SV_FiveVillageRunPortal')
        portal.set_folder_path('FiveVillageRuntime20261111/Entry')
        portal.set_editor_property('tags', [unreal.Name(TAG)])
        report['new_portal'] = snapshot(portal)
        if snapshot(existing[0]) != before:
            raise RuntimeError('Original portal differs; stop and inspect without discarding edits.')
        if not levels.save_current_level():
            raise RuntimeError('Hub save failed. Preserve the partial candidate for inspection.')
        if dirty():
            raise RuntimeError('Dirty packages remain after save. Do not switch or discard.')
        # A different saved map is loaded between saves to establish a real hub reload.
        if not levels.load_level('/Game/Rogue10m/Maps/L_Menu') or not levels.load_level(HUB):
            raise RuntimeError('Independent hub reload failed.')
        reloaded, old = owned_portals(), originals()
        if len(reloaded) != 1 or reloaded[0].get_class().get_path_name() != PORTAL_CLASS or len(old) != 1:
            raise RuntimeError('Portal count/class mismatch after reload.')
        if snapshot(old[0]) != before or snapshot(reloaded[0]) != report['new_portal']:
            raise RuntimeError('Portal readback differs after reload.')
        report['candidate_sha256_after'] = hashlib.sha256(candidate_file.read_bytes()).hexdigest()
        if report['candidate_sha256_before'] != report['candidate_sha256_after'] or dirty():
            raise RuntimeError('Candidate or dirty state changed unexpectedly.')
        report['status'] = 'saved_and_reloaded_new_entry'
        report['original_portal_unchanged'] = True
        save_report()
        return report
    except Exception as error:
        report['status'] = 'blocked_or_failed'
        report['error'] = str(error)
        save_report()
        raise


if __name__ == '__main__':
    print('Import and call activate only after the shared editor/build owner confirms handoff.')
