"""Read the live, project-authored FistAuthored shoulder skill baseline."""
import hashlib
import json
import os
from pathlib import Path

import unreal


output = Path(os.environ["ROGUE_SHOULDER_WORKSPACE"]).resolve()
output.mkdir(exist_ok=False)
skill = unreal.load_asset("/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Charged")
montage = skill.get_editor_property("attack_montage")
slots = montage.get_editor_property("slot_anim_tracks")
segments = slots[0].get_editor_property("anim_track").get_editor_property("anim_segments")
clip = segments[0].get_editor_property("anim_reference")
assert len(slots) == len(segments) == 1 and clip.get_name() == "A_Knuckle_ShortBlow"
options = unreal.FbxExportOption()
options.set_editor_properties({"export_preview_mesh": False, "export_morph_targets": False, "level_of_detail": False})
path = output / "ShortBlow.fbx"
task = unreal.AssetExportTask()
task.set_editor_properties({"object": clip, "options": options, "filename": str(path), "prompt": False,
                            "automated": True, "replace_identical": False})
assert unreal.Exporter.run_asset_export_task(task) and not task.errors, task.errors
properties = ("input_slot", "hit_start_delay_seconds", "animation_play_rate", "attack_range", "attack_shape",
              "hit_count", "charge_seconds", "effect_attach_socket")
details = {name: str(skill.get_editor_property(name)) for name in properties}
details.update({"skill": skill.get_path_name(), "montage": montage.get_path_name(), "sequence": clip.get_path_name(),
                "montage_seconds": montage.get_play_length(), "sequence_seconds": clip.get_play_length(),
                "skeleton": clip.get_editor_property("skeleton").get_path_name(),
                "fbx_sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
(output / "reference.json").write_text(json.dumps(details, indent=2) + "\n", encoding="utf-8")
unreal.log("RESULT=SHOULDER_BASELINE_EXPORTED")
