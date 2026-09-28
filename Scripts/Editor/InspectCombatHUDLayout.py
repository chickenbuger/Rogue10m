"""Dump the current combat HUD Widget Blueprint Designer trees without modifying assets."""

from __future__ import annotations

import os
import unreal


ASSET_PATHS = [
    "/Game/Widget/WBP_Rogue10mMainHUD",
    "/Game/Widget/Parts/WBP_VitalBar",
    "/Game/Widget/Parts/WBP_LevelExperiencePanel",
    "/Game/Widget/Parts/WBP_Identity",
    "/Game/Widget/Parts/WBP_SkillSlotPanel",
    "/Game/Widget/Parts/WBP_ItemSlotPanel",
    "/Game/Widget/Parts/WBP_QuickSlot",
    "/Game/Widget/Parts/WBP_SystemLogPanel",
    "/Game/Widget/Parts/WBP_ItemAcquisitionFeed",
    "/Game/Widget/Parts/WBP_MonsterInfo",
]
OUTPUT_PATH = "D:/Project/Rogue10m/Saved/CombatHUDLayoutBefore.txt"
UMG_TOOL = unreal.get_default_object(unreal.UMGToolSet)


def call(method: str, *args):
    return UMG_TOOL.call_method(method, args)


def safe_value(obj, method_name: str):
    try:
        method = getattr(obj, method_name)
        return method()
    except Exception:
        return None


def format_slot(slot) -> str:
    if not slot:
        return "slot=None"

    parts = [f"slot={slot.get_class().get_name()}"]
    for label, method_name in [
        ("anchors", "get_anchors"),
        ("alignment", "get_alignment"),
        ("position", "get_position"),
        ("size", "get_size"),
        ("offsets", "get_offsets"),
        ("z_order", "get_z_order"),
        ("padding", "get_padding"),
        ("horizontal_alignment", "get_horizontal_alignment"),
        ("vertical_alignment", "get_vertical_alignment"),
    ]:
        value = safe_value(slot, method_name)
        if value is not None:
            parts.append(f"{label}={value}")
    return "; ".join(parts)


def main() -> None:
    if not hasattr(unreal, "UMGToolSet"):
        raise RuntimeError("UE5.8 UMGToolSet plugin is not loaded")

    lines: list[str] = []
    for asset_path in ASSET_PATHS:
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        lines.append(f"## {asset_path}")
        if not asset:
            lines.append("MISSING")
            continue

        tree = call("GetWidgets", asset)
        infos = [info for info in tree.widgets if info.widget]
        lines.append(f"widget_count={len(infos)}")
        for info in infos:
            widget = info.widget
            parent = info.parent.get_name() if info.parent else "ROOT"
            lines.append(
                f"{widget.get_name()} | class={widget.get_class().get_name()} | "
                f"parent={parent} | {format_slot(info.slot)}"
            )
        lines.append("")

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as output_file:
        output_file.write("\n".join(lines))
    unreal.log(f"[Rogue10mCombatHUD] Layout dump saved: {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
