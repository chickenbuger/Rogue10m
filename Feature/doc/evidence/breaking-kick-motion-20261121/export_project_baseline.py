"""Read and export the project-authored Manny baseline without saving packages."""
import hashlib
import json
import os
from pathlib import Path

import unreal


OUT = Path(os.environ["ROGUE_BREAKING_KICK_WORKSPACE"]).resolve()
OUT.mkdir(exist_ok=False)
skill = unreal.load_asset("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Special")
montage = unreal.load_asset("/Game/Rogue10m/Animation/Combat/FistAuthored/AM_Knuckle_Uppercut")
tracks = montage.get_editor_property("slot_anim_tracks")
segments = tracks[0].get_editor_property("anim_track").get_editor_property("anim_segments")
sequence = segments[0].get_editor_property("anim_reference")
assert sequence.get_name() == "A_Knuckle_Uppercut" and len(tracks) == len(segments) == 1

options = unreal.FbxExportOption()
options.set_editor_property("export_preview_mesh", False)
options.set_editor_property("export_morph_targets", False)
options.set_editor_property("level_of_detail", False)
filename = OUT / "owned-manny-uppercut.fbx"
task = unreal.AssetExportTask()
task.object = sequence
task.options = options
task.filename = str(filename)
task.automated = True
task.prompt = False
task.replace_identical = False
if not unreal.Exporter.run_asset_export_task(task) or task.errors:
    raise RuntimeError(task.errors)

fields = ("input_slot", "hit_start_delay_seconds", "animation_play_rate", "attack_range", "attack_shape", "arc_angle_degrees", "hit_count")
details = {name: str(skill.get_editor_property(name)) for name in fields}
details.update({"skill": skill.get_path_name(), "montage": montage.get_path_name(), "sequence": sequence.get_path_name(),
                "current_skill_binding": skill.get_editor_property("attack_montage").get_path_name(),
                "montage_seconds": montage.get_play_length(), "sequence_seconds": sequence.get_play_length(),
                "skeleton": sequence.get_editor_property("skeleton").get_path_name(),
                "fbx_sha256": hashlib.sha256(filename.read_bytes()).hexdigest()})
(OUT / "project-baseline.json").write_text(json.dumps(details, indent=2) + "\n", encoding="utf-8")
unreal.log("RESULT=BREAKING_KICK_PROJECT_BASELINE_EXPORTED")
