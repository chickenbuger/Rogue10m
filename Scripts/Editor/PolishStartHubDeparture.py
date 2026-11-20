"""Inspect, apply, or verify the existing StartHub north gate in an isolated editor.

Pass this file and its arguments as one -ExecutePythonScript value. Use a fresh
editor process for verification after apply. The script saves only the two
obsolete sign actors and the two project-owned gate materials.
"""

import argparse
import hashlib
import json
import math
import traceback
from pathlib import Path

import unreal


MAP = "/Game/Rogue10m/Maps/StartHub"
EXTERNAL = "/Game/__ExternalActors__/Rogue10m/Maps/StartHub/"
MATERIALS = "/Game/Rogue10m/World/StartVillage/Materials/"
GUIDE = "/Game/Rogue10m/World/StartVillage/Tutorial/DA_StartVillageGuide"
SIGN_PACKAGES = {
    "SV_PortalDestinationSign": ("C/7B/IE34V7XW7A6DIMCAY2FC4M", "COMBAT PORTAL"),
    "SV_PortalPrototypeSign": ("A/O5/QUATKN33RUPGEIT3LCJVER", "ENTER TO BEGIN A RUN"),
}
COLOR_TUNING = {
    "M_StartVillage_PortalGlow": ((0.15, 2.5, 3.4, 1.0), (0.32, 0.55, 0.52, 1.0)),
    "M_StartVillage_PortalSurface": ((0.08, 0.9, 1.3, 1.0), (0.09, 0.23, 0.25, 1.0)),
}
OPACITY_TUNING = (0.34, 0.06)
RUNE_IDLE_OPACITY = 0.07
GUIDE_COPY = ("welcome_text", "reward_received_text", "reward_already_received_text")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def dirty_packages():
    editor = unreal.EditorLoadingAndSavingUtils
    packages = list(editor.get_dirty_map_packages()) + list(editor.get_dirty_content_packages())
    return {item.get_name(): item for item in packages}


def package_hash(root, package):
    suffix = ".umap" if package == MAP else ".uasset"
    return hashlib.sha256((root / "Content" / (package.removeprefix("/Game/") + suffix)).read_bytes()).hexdigest()


def matches(left, right):
    return all(math.isclose(a, b, abs_tol=1e-5) for a, b in zip(left, right, strict=True))


def actor_state(actor):
    return {
        "package": actor.get_package().get_name(),
        "location": tuple(actor.get_actor_location().to_tuple()),
        "rotation": tuple(actor.get_actor_rotation().to_tuple()),
        "scale": tuple(actor.get_actor_scale3d().to_tuple()),
    }


def load_gate_actors():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    require(not levels.is_in_play_in_editor() and not dirty_packages(), "Use a clean, idle editor")
    world = editor.get_editor_world()
    if not world or world.get_package().get_name() != MAP:
        require(levels.load_level(MAP), "Could not load the existing StartHub")
        world = editor.get_editor_world()
    require(world and world.get_package().get_name() == MAP and not dirty_packages(),
            "A clean StartHub is required")
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def checked_actor(actors, label, actor_type):
    found = [actor for actor in actors if actor.get_actor_label() == label]
    require(len(found) == 1 and isinstance(found[0], actor_type), "Unexpected actor: " + label)
    actor = found[0]
    require(actor.get_package().get_name().startswith(EXTERNAL)
            and "Rogue10m.StartVillage" in map(str, actor.get_editor_property("tags")),
            "Actor is outside the existing StartHub set: " + label)
    return actor


def material_operations(actors):
    library = unreal.MaterialEditingLibrary
    operations = []
    materials = {}
    for name, (original, target) in COLOR_TUNING.items():
        material = unreal.load_asset(MATERIALS + name)
        require(isinstance(material, unreal.Material), "Missing project-owned material: " + name)
        color = library.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        require(isinstance(color, unreal.MaterialExpressionVectorParameter)
                and str(color.get_editor_property("parameter_name")) == "Color", "Unexpected emissive graph: " + name)
        operations.append((material, color, original, target))
        materials[name] = material
    surface = materials["M_StartVillage_PortalSurface"]
    opacity = library.get_material_property_input_node(surface, unreal.MaterialProperty.MP_OPACITY)
    require(isinstance(opacity, unreal.MaterialExpressionScalarParameter)
            and str(opacity.get_editor_property("parameter_name")) == "Opacity", "Unexpected portal surface opacity graph")
    operations.append((surface, opacity, (OPACITY_TUNING[0],), (OPACITY_TUNING[1],)))

    holders = {"M_StartVillage_PortalGlow": {f"SV_PortalRune_{index:02d}" for index in range(24)},
               "M_StartVillage_PortalSurface": {"SV_PortalEnergySurface"}}
    for name, material in materials.items():
        uses = {actor.get_actor_label() for actor in actors if isinstance(actor, unreal.StaticMeshActor)
                and material in actor.get_component_by_class(unreal.StaticMeshComponent).get_materials()}
        require(uses == holders[name], "Unexpected StartHub use of " + name + ": " + str(sorted(uses)))
    return operations, materials["M_StartVillage_PortalGlow"]


def value_of(expression):
    value = expression.get_editor_property("default_value")
    return tuple(float(getattr(value, name)) for name in "rgba") if isinstance(value, unreal.LinearColor) else (float(value),)


def rune_surface(material):
    opacity = unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY)
    require(opacity is None or (isinstance(opacity, unreal.MaterialExpressionScalarParameter)
            and str(opacity.get_editor_property("parameter_name")) == "IdleOpacity"), "Unexpected rune opacity graph")
    return material.get_editor_property("blend_mode"), None if opacity is None else value_of(opacity)[0]


def sign_state(component):
    return {
        "text": str(component.get_editor_property("text")),
        "visible": bool(component.get_editor_property("visible")),
        "hidden_in_game": bool(component.get_editor_property("hidden_in_game")),
        "collision": str(component.get_collision_enabled()),
    }


def run(root, mode, report):
    actors = load_gate_actors()
    portal = checked_actor(actors, "SV_RunPortal_ToOpenWorld", unreal.Rogue10mRunPortal)
    guide_actor = checked_actor(actors, "SV_TutorialGuide_Arin", unreal.Rogue10mTutorialNPC)
    guide = unreal.load_asset(GUIDE)
    require(isinstance(guide, unreal.Rogue10mTutorialDataAsset)
            and guide_actor.get_editor_property("tutorial_data") == guide, "StartHub Arin guide data differs from baseline")
    portal_state = actor_state(portal)
    report["portal"] = portal_state
    require(portal_state["package"] == EXTERNAL + "4/6L/9ETBXYOM6DWLFQ0NWB8O0H"
            and matches(portal_state["location"][:2], (0.0, 2135.0))
            and str(portal.get_editor_property("target_level_name")) == "/Game/Rogue10m/Maps/OpenRunMap"
            and portal.get_editor_property("start_run_timer_on_travel"), "Canonical portal placement or target differs")

    defaults = unreal.get_default_object(unreal.Rogue10mTutorialDataAsset)
    guide_text = {key: str(guide.get_editor_property(key)) for key in GUIDE_COPY}
    default_text = {key: str(defaults.get_editor_property(key)) for key in GUIDE_COPY}
    report["guide_copy"] = {"asset": guide_text, "native_defaults": default_text}
    if mode != "inspect":
        require(guide_text == default_text and all("북쪽 관문 앞에서 F를 눌러 대화" in text for text in guide_text.values())
                and "20분" in guide_text["welcome_text"], "Arin does not read the updated native instructions")

    signs = {}
    report["signs"] = {}
    for label, (subpath, expected_text) in SIGN_PACKAGES.items():
        actor = checked_actor(actors, label, unreal.TextRenderActor)
        require(actor.get_package().get_name() == EXTERNAL + subpath, "Sign package differs: " + label)
        component = actor.get_component_by_class(unreal.TextRenderComponent)
        before = sign_state(component)
        require(before["text"] == expected_text and component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION,
                "Unexpected sign text or collision: " + label)
        signs[label] = (actor, component)
        report["signs"][label] = {"actor": actor_state(actor), "before": before}

    operations, rune_material = material_operations(actors)
    report["materials"] = {}
    for material, expression, original, target in operations:
        name = material.get_name() + "." + str(expression.get_editor_property("parameter_name"))
        report["materials"][name] = {"before": value_of(expression), "original": original, "target": target}
        require(matches(value_of(expression), original) or matches(value_of(expression), target), "Unexpected material value: " + name)

    rune_blend, rune_opacity = rune_surface(rune_material)
    original_rune_surface = rune_blend == unreal.BlendMode.BLEND_OPAQUE and rune_opacity is None
    finished_rune_surface = rune_blend == unreal.BlendMode.BLEND_TRANSLUCENT and rune_opacity is not None \
        and math.isclose(rune_opacity, RUNE_IDLE_OPACITY, abs_tol=1e-5)
    require(original_rune_surface or finished_rune_surface, "Unexpected initial rune surface")
    report["rune_surface"] = {"before": [str(rune_blend), rune_opacity]}

    allowed = {EXTERNAL + subpath for subpath, _ in SIGN_PACKAGES.values()}
    allowed.update(MATERIALS + name for name in COLOR_TUNING)
    monitored = allowed | {MAP, GUIDE, portal_state["package"], guide_actor.get_package().get_name()}
    before_hashes = {p: package_hash(root, p) for p in sorted(monitored)}
    require(not dirty_packages(), "Packages became dirty during StartHub inspection")
    changed = set()
    if mode == "apply":
        for actor, component in signs.values():
            if component.get_editor_property("visible") or not component.get_editor_property("hidden_in_game"):
                actor.modify()
                component.modify()
                component.set_visibility(False)
                component.set_hidden_in_game(True)
                changed.add(actor.get_package().get_name())

        changed_materials = set()
        if original_rune_surface:
            # The opaque cards still stood out in the rendered dialogue even with a lower emissive value.
            rune_material.modify()
            rune_material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
            library = unreal.MaterialEditingLibrary
            opacity = library.create_material_expression(rune_material, unreal.MaterialExpressionScalarParameter, -320, 280)
            opacity.set_editor_property("parameter_name", "IdleOpacity")
            opacity.set_editor_property("default_value", RUNE_IDLE_OPACITY)
            require(library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY), "Could not connect rune opacity")
            changed_materials.add(rune_material)
            changed.add(rune_material.get_package().get_name())
        for material, expression, _, target in operations:
            if not matches(value_of(expression), target):
                material.modify()
                expression.modify()
                expression.set_editor_property("default_value", unreal.LinearColor(*target) if len(target) == 4 else target[0])
                changed_materials.add(material)
                changed.add(material.get_package().get_name())
        for material in sorted(changed_materials, key=lambda item: item.get_name()):
            errors = list(unreal.MaterialEditingLibrary.recompile_material(material))
            require(not errors, "Material recompile failed: " + str(errors))

    for label, (actor, component) in signs.items():
        after = sign_state(component)
        record = report["signs"][label]
        record["after"] = after
        require(actor_state(actor) == record["actor"] and after["collision"] == record["before"]["collision"],
                "Sign transform or collision changed: " + label)
        require(mode == "inspect" or (not after["visible"] and after["hidden_in_game"]), "Obsolete sign is still visible: " + label)
    for material, expression, _, target in operations:
        record = report["materials"][material.get_name() + "." + str(expression.get_editor_property("parameter_name"))]
        record["after"] = value_of(expression)
        require(mode == "inspect" or matches(record["after"], target), "Unexpected gate material readback: " + material.get_name())

    rune_blend, rune_opacity = rune_surface(rune_material)
    report["rune_surface"]["after"] = [str(rune_blend), rune_opacity]
    require(mode == "inspect" or (rune_blend == unreal.BlendMode.BLEND_TRANSLUCENT and rune_opacity is not None
            and math.isclose(rune_opacity, RUNE_IDLE_OPACITY, abs_tol=1e-5)), "Rune translucency did not apply")

    dirty = dirty_packages()
    report["dirty_before_save"] = sorted(dirty)
    require(set(dirty) == changed and changed <= allowed, "Unexpected dirty package set: " + str(sorted(dirty)))
    report["saved_packages"] = []
    if dirty:
        require(unreal.EditorLoadingAndSavingUtils.save_packages([dirty[name] for name in sorted(dirty)], True), "Scoped package save failed")
        report["saved_packages"] = sorted(dirty)
    after_hashes = {p: package_hash(root, p) for p in sorted(monitored)}
    report["package_hashes_before"] = before_hashes
    report["package_hashes_after"] = after_hashes
    require({p for p in monitored if before_hashes[p] != after_hashes[p]} == changed and not dirty_packages(),
            "Saved package hashes do not match the scoped dirty set")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("inspect", "apply", "verify"), default="inspect")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    root = Path(unreal.Paths.project_dir()).resolve()
    require(Path(__file__).resolve().parents[2] == root and (root / "Content").resolve() == root / "Content",
            "Run the script from the same physical project checkout as the editor")
    output = args.report.resolve()
    require(output.is_relative_to(root) and not output.exists(), "Use a fresh report path inside this checkout")
    output.parent.mkdir(parents=True, exist_ok=True)
    report = {"mode": args.mode, "success": False}
    try:
        run(root, args.mode, report)
        report["success"] = True
        unreal.log("ROGUE_STARTHUB_DEPARTURE: " + args.mode.upper() + " PASSED")
    except Exception:
        report["error"] = traceback.format_exc()
        raise
    finally:
        output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
