"""Asset-read-only audit for the serial editor owner; does not assign or save assets.

Run only after the coordinator grants the editor slot. Output requires a fresh label.
This script inspects transient skeletal components, never map actors or project settings.
"""
from pathlib import Path
import hashlib
import json
import re
import unreal


def vector(value):
    return [value.x, value.y, value.z]


def main():
    root = Path(unreal.Paths.project_dir()).resolve()
    command_line = unreal.SystemLibrary.get_command_line()
    if not re.search(r"(?:^|\s)-RogueStaffAuditLease=CoordinatorGranted(?=\s|$)", command_line):
        raise RuntimeError("Coordinator-granted serial editor slot must be asserted explicitly.")
    match = re.search(r"(?:^|\s)-RogueStaffAuditLabel=([A-Za-z0-9_]{1,24})(?=\s|$)",
                      command_line)
    if not match:
        raise RuntimeError("Pass a fresh -RogueStaffAuditLabel=<label>; no default evidence overwrite.")
    output = root / "Feature/doc/evidence/staff-equipment-20261110" / match.group(1)
    output.mkdir(parents=True, exist_ok=False)
    candidates = ["/Game/Stylized_Village/Assets/Architecture/Wood/Pieces/SM_Rod_" + name
                  for name in ("Short", "Medium", "Long", "XL")]
    candidates += ["/Game/Stylized_Village/Assets/Architecture/Wood/Pieces/SM_Beam_Long_1"]
    files = [root / ("Content/" + path.removeprefix("/Game/") + ".uasset") for path in candidates]
    before = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in files if path.is_file()}
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    mesh_rows = []
    for path in candidates:
        asset = unreal.load_asset(path)
        row = {"path": path, "class": asset.get_class().get_name() if asset else None,
               "selected_for_staff": False, "ownership_reverified": False}
        if isinstance(asset, unreal.StaticMesh):
            bounds = asset.get_bounding_box()
            row.update(bounds_min_cm=vector(bounds.min), bounds_max_cm=vector(bounds.max),
                       lod_count=subsystem.get_lod_count(asset),
                       lod0_triangles=asset.get_num_triangles(0),
                       lod0_vertices=asset.get_num_vertices(0),
                       simple_collision_count=subsystem.get_simple_collision_count(asset),
                       materials=[slot.get_editor_property("material_interface").get_path_name()
                                  if slot.get_editor_property("material_interface") else None
                                  for slot in asset.get_editor_property("static_materials")])
        mesh_rows.append(row)
    bodies = []
    for path in ("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple",
                 "/Game/StylizedCharacter/Meshes/Character/Human/Male/SK_Hu_M_FullBody"):
        mesh = unreal.load_asset(path)
        row = {"path": path, "loaded": isinstance(mesh, unreal.SkeletalMesh)}
        if row["loaded"]:
            component = unreal.SkeletalMeshComponent()
            component.set_skeletal_mesh_asset(mesh)
            skeleton = mesh.get_editor_property("skeleton")
            row.update(skeleton=skeleton.get_path_name() if skeleton else None,
                       hand_r_index=component.get_bone_index("hand_r"),
                       hand_l_index=component.get_bone_index("hand_l"),
                       hand_r_socket_exists=component.does_socket_exist("hand_r"))
        bodies.append(row)
    skills = []
    for suffix in ("Primary01", "Primary02", "Primary03", "Special", "Charged"):
        path = "/Game/DataAsset/AttackSkill/ClassCombat/Staff/DA_Attack_Staff_" + suffix
        skill = unreal.load_asset(path)
        montage = skill.get_editor_property("attack_montage") if skill else None
        skeleton = montage.get_editor_property("skeleton") if montage else None
        row = {"path": path, "loaded": bool(skill),
               "montage": montage.get_path_name() if montage else None,
               "skeleton": skeleton.get_path_name() if skeleton else None,
               "animation_play_rate": skill.get_editor_property("animation_play_rate") if skill else None,
               "hit_start_delay_seconds": skill.get_editor_property("hit_start_delay_seconds") if skill else None,
               "segments": []}
        if montage:
            row["montage_rate_scale"] = montage.get_editor_property("rate_scale")
            for slot in montage.get_editor_property("slot_anim_tracks"):
                track = slot.get_editor_property("anim_track")
                for segment in track.get_editor_property("anim_segments"):
                    sequence = segment.get_editor_property("anim_reference")
                    row["segments"].append({
                        "slot": str(slot.get_editor_property("slot_name")),
                        "sequence": sequence.get_path_name() if sequence else None,
                        "sequence_rate_scale": sequence.get_editor_property("rate_scale") if sequence else None,
                        "sequence_length": sequence.get_play_length() if sequence else None,
                        "start_pos": segment.get_editor_property("start_pos"),
                        "start_seconds": segment.get_editor_property("anim_start_time"),
                        "end_seconds": segment.get_editor_property("anim_end_time"),
                        "segment_play_rate": segment.get_editor_property("anim_play_rate"),
                        "loop_count": segment.get_editor_property("looping_count")})
        skills.append(row)
    after = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
             for path in files if path.is_file()}
    report = {"status": "read_only_metadata_audit", "candidate_meshes": mesh_rows,
              "body_socket_metadata": bodies, "loaded_staff_skills": skills,
              "candidate_packages_preserved": before == after, "package_sha256": after,
              "staff_art_selected": False, "runtime_equipped": False,
              "animation_grip_or_render_validated": False,
              "actual_runtime_playback_mesh_inspected": False,
              "note": "Class/bounds/socket metadata do not validate staff design, terms, grip or retarget motion."}
    (output / "audit.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    if before != after:
        raise RuntimeError("Candidate source packages changed during read-only audit; inspect concurrent work.")
    unreal.log("RESULT=STAFF_METADATA_AUDIT_RECORDED " + str(output))


if __name__ == "__main__":
    main()
