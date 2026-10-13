"""Read-only saved Blueprint/Texture contract check for the two-corner HUD."""
from __future__ import annotations
import json
import sys
from pathlib import Path
import unreal

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
import ApplySplitCombatHUD as split
from ApplyClassThemedBottomHUD import widget_infos, load_widget
from ApplyResponsiveCombatHUD import blueprint_class


def main():
    project = Path(unreal.Paths.project_dir()).resolve()
    assert project.as_posix().casefold() == 'd:/project/rogue10m'
    dirty_before = {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    assets = []
    for path in split.OUTPUTS:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        assert asset is not None, path
        assert asset.get_outermost().get_name() not in dirty_before, 'Unsaved package: ' + path
        assets.append(path)
    bottom = load_widget(split.BOTTOM_HUD_PATH)
    names = {info.widget.get_name(): info.widget for info in widget_infos(bottom)}
    required = {'UI_LeftCombatScaleBox', 'UI_LeftCombatDesignSize', 'UI_RightCombatScaleBox',
                'UI_RightCombatDesignSize', 'HealthBarWidget', 'StaminaBarWidget', 'ManaBarWidget',
                'ProgressionWidget', 'UI_LevelText', 'IdentityWidget', 'SkillSlotContainer', 'ItemSlotContainer'}
    assert required.issubset(names), sorted(required-names.keys())
    assert 'UI_CompactPanel' not in names and 'UI_CombatScaleBox' not in names
    defaults = unreal.get_default_object(blueprint_class(split.BOTTOM_HUD_PATH))
    assert defaults.get_editor_property('use_split_combat_layout')
    assert defaults.get_editor_property('left_combat_design_size') == unreal.Vector2D(560,180)
    assert defaults.get_editor_property('right_combat_design_size') == unreal.Vector2D(600,270)
    assert defaults.get_editor_property('split_minimum_center_gap') == 96
    assert defaults.get_editor_property('combat_side_margin') == 24
    assert defaults.get_editor_property('combat_bottom_margin') == 24
    assert names['SkillSlotContainer'].get_children_count() == 5
    assert names['ItemSlotContainer'].get_children_count() == 4
    identity = load_widget(split.IDENTITY)
    info = {i.widget.get_name(): i for i in widget_infos(identity)}
    percent = info['UI_IdentityPercentText']
    assert percent.slot.get_position().y == 123 and percent.slot.get_size().y == 18
    assert info['UI_WeaponNameText'].slot.get_position().y == 99
    identity_defaults = unreal.get_default_object(blueprint_class(split.IDENTITY))
    overrides = identity_defaults.get_editor_property('weapon_icon_overrides')
    assert overrides[unreal.Rogue10mWeaponType.UNARMED].get_path_name() == split.TEXTURES[0]+'.T_Identity_Unarmed'
    assert overrides[unreal.Rogue10mWeaponType.GREAT_SWORD].get_path_name() == split.TEXTURES[1]+'.T_Identity_GreatSword'
    assert identity_defaults.get_editor_property('use_circular_resource_gauge')
    for path in split.TEXTURES:
        texture = unreal.EditorAssetLibrary.load_asset(path)
        assert texture.get_editor_property('never_stream')
        assert texture.get_editor_property('lod_group') == unreal.TextureGroup.TEXTUREGROUP_UI
        assert texture.get_editor_property('mip_gen_settings') == unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    dirty = list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    assert not dirty, [package.get_name() for package in dirty]
    report = {'result': 'SPLIT_HUD_SAVED_CONTRACT_PASSED', 'saved_assets': assets,
              'required_bindings': sorted(required), 'skills': 5, 'items': 4,
              'identity_percent_bottom': 141, 'dirty_packages': []}
    (project/'Feature/doc/evidence/split-hud-20261014/saved-contract.json').write_text(
        json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False))


if __name__ == '__main__':
    main()
