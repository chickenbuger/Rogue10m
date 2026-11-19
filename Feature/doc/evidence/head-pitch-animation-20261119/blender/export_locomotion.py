"""Export representative runtime locomotion clips through Unreal's normal FBX exporter."""

import json
import os
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parent
PREVIEW_MESH = os.getenv('ROGUE_HEAD_EXPORT_SKELETON_ONLY') != '1'
OUT = ROOT if PREVIEW_MESH else ROOT / 'skeleton-only'
BLENDSPACE = '/Game/Rogue10m/Animation/Locomotion/PalmReference/BS_Unarmed_PalmReference'
WALK = 'MF_Unarmed_Walk_Fwd_PalmReference'
JOG = 'MF_Unarmed_Jog_Fwd_PalmReference'


def main():
    OUT.mkdir(exist_ok=True)
    blendspace = unreal.load_asset(BLENDSPACE)
    samples = blendspace.get_editor_property('sample_data')
    clips = {}
    for sample in samples:
        animation = sample.get_editor_property('animation')
        value = sample.get_editor_property('sample_value')
        if animation and value.y == 0.0:
            clips['idle'] = animation
        elif animation and animation.get_name() == WALK:
            clips['walk'] = animation
        elif animation and animation.get_name() == JOG:
            clips['jog'] = animation
    if set(clips) != {'idle', 'walk', 'jog'}:
        raise RuntimeError(f'Unexpected blendspace samples: {list(clips)}')

    source = {'blendspace': blendspace.get_path_name(), 'preview_mesh': PREVIEW_MESH, 'clips': {}}
    for label, sequence in clips.items():
        options = unreal.FbxExportOption()
        options.set_editor_property('export_preview_mesh', PREVIEW_MESH)
        options.set_editor_property('export_morph_targets', False)
        options.set_editor_property('level_of_detail', False)
        task = unreal.AssetExportTask()
        task.object = sequence
        task.options = options
        task.filename = str(OUT / f'{label}.fbx')
        task.automated = True
        task.prompt = False
        task.replace_identical = False
        if not unreal.Exporter.run_asset_export_task(task) or task.errors:
            raise RuntimeError(f'Unable to export {label}: {task.errors}')
        skeleton = sequence.get_editor_property('skeleton')
        source['clips'][label] = {
            'asset': sequence.get_path_name(),
            'skeleton': skeleton.get_path_name(),
            'duration_seconds': sequence.get_play_length(),
            'root_motion': sequence.get_editor_property('enable_root_motion'),
        }
    (OUT / 'unreal-source.json').write_text(json.dumps(source, indent=2) + '\n', encoding='utf-8')


main()
