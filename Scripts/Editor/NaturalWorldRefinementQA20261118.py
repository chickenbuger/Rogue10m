"""Inert, read-only contracts for the explicitly approved visual refinement.

The original numeric checkpoint remains a separate validation stage. No saves,
loading of levels, actor edits or callbacks are performed by this module.
"""
from __future__ import annotations

from collections import Counter
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "Feature/doc/evidence/landscape-natural-boundary20261117"
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
OWN = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117"
PROFILE = "20261118-rounded-sine-profile"
ORIGINAL_HEIGHT_SHA256 = "81c7e6cc3ac485edbf5afa4d4c125617e82e07be7a9ba23e2bdf7ff75157b773"
BOSS_PLAN = BASE / "boss-threshold-plan20261118B.json"
BOSS_SHA256 = "7a267a6eecfc961a551215bc635a43e0efc4aedfd8018a973764f1bb624d09a0"
FLOOR_PLAN = BASE / "village-earth-plan20261118.json"
FLOOR_SHA256 = "e6af4032bdf2d1a12aa1a2f6a9a752842457c0c8de086e4dcb020718e0ca5e90"
FLOOR_LABELS = {"NW18_VillageEarth_"+hub for hub in ("H", "M", "T")}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def read(file):
    return json.loads(Path(file).read_text(encoding="utf-8-sig"))


def stage(requested):
    require(requested in ("auto", "baseline", "height_only", "boss_only", "complete"), "Unknown strict refinement stage")
    if requested != "auto":
        return requested
    if not (BASE/"target-height-plan-rounded-ridge.json").exists():
        return "baseline"
    return "complete" if FLOOR_PLAN.exists() else "boss_only" if BOSS_PLAN.exists() else "height_only"


def rounded_grid(original):
    """Independent SDF + sine-squared recipe, without importing the author code."""
    polygon = original["polygon_world_cm"]
    bounds, values = original["vertex_bounds"], []
    for iy in range(bounds[1], bounds[3]+1):
        y = -90000+iy*(1250/7)
        for ix in range(bounds[0], bounds[2]+1):
            x = -90000+ix*(1250/7)
            inside, squared = False, float("inf")
            for a, b in zip(polygon, polygon[1:]+polygon[:1]):
                dx, dy = b[0]-a[0], b[1]-a[1]
                t = min(1.0, max(0.0, ((x-a[0])*dx+(y-a[1])*dy)/(dx*dx+dy*dy)))
                squared = min(squared, (x-a[0]-t*dx)**2+(y-a[1]-t*dy)**2)
                if (a[1] > y) != (b[1] > y) and x < (b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:
                    inside = not inside
            signed = math.sqrt(squared)*(-1 if inside else 1)
            s = signed+150*math.sin(x/1900+y/2700)+100*math.sin(y/1500-x/3800)
            z = 9600.0
            if -1500 < s < 2200:
                amplitude = 2600+450*math.sin(x/3300+y/5100)+250*math.cos(y/2600-x/4500)
                z += amplitude*math.sin(math.pi*(s+1500)/3700)**2
            values.append(round(32768+z/2))
    return values


def select_height(selected_stage):
    require(hashlib.sha256((BASE/"target-height-plan.json").read_bytes()).hexdigest() == ORIGINAL_HEIGHT_SHA256,
            "Preserved original height plan changed")
    original = read(BASE/"target-height-plan.json")
    if selected_stage == "baseline":
        return original, {"stage": selected_stage, "file": "target-height-plan.json", "recipe_verified": "historical_original_plan"}
    file = BASE/"target-height-plan-rounded-ridge.json"
    revised = read(file)
    require(set(revised) == set(original) | {"profile_revision"} and revised["profile_revision"] == PROFILE,
            "Rounded plan may add only the exact profile_revision field")
    require(all(revised[k] == value for k, value in original.items() if k != "target_heights"),
            "Rounded plan changed non-height layout metadata")
    expected = rounded_grid(original)
    require(len(expected) == 179488 and revised["target_heights"] == expected,
            "Rounded target differs from the independent exact sine-squared recipe")
    return revised, {"stage": selected_stage, "file": file.name, "sha256": hashlib.sha256(file.read_bytes()).hexdigest(),
                     "recipe_verified": True, "profile_revision": PROFILE, "samples": len(expected)}


def boss_group():
    require(hashlib.sha256(BOSS_PLAN.read_bytes()).hexdigest() == BOSS_SHA256, "Immutable Boss Plan B changed")
    plan = read(BOSS_PLAN)
    require(plan["schema"] == "Rogue10m.NaturalBossThresholdPlan.20261118.v1" and plan["revision"] == "B"
            and plan["map"] == TARGET and plan["actor_label"] == "NW18_BossThreshold"
            and plan["boss_ground"]["all_properties_preserved"] is True and plan["instance_count"] == 5,
            "Exact additive-only Boss Plan B required")
    expected = {
        "BossThreshold_Post_W": ([14240,62550,10100],[120,300,1000],"M_Natural_WarmStone"),
        "BossThreshold_Post_E": ([15760,62550,10100],[120,300,1000],"M_Natural_WarmStone"),
        "BossThreshold_Lintel": ([15000,62550,10680],[1640,300,160],"M_Natural_WarmStone"),
        "BossThreshold_Banner_W": ([14240,62394,10350],[100,10,380],"M_Natural_Banner_Blue"),
        "BossThreshold_Banner_E": ([15760,62394,10350],[100,10,380],"M_Natural_Banner_Blue"),
    }
    require(len(plan["instances"]) == 5 and {r["label"] for r in plan["instances"]} == set(expected), "Boss part identity differs")
    for row in plan["instances"]:
        center, dimensions, material = expected[row["label"]]
        require(row["location_cm"] == center and row["dimensions_cm"] == dimensions
                and row["rotation_pitch_yaw_roll"] == [0,0,0]
                and row["scale_for_verified_100cm_mesh"] == [v/100 for v in dimensions]
                and row["mesh"] == "/Engine/BasicShapes/Cube" and row["material"] == OWN+"/"+material
                and row["collision_role"] == "visual_only" and row["hidden_in_game"] is False, "Boss part recipe differs")
    return {"id": "BossThreshold", "actor_label": plan["actor_label"], "folder": plan["folder"],
            "tags": ["Rogue10m.NaturalWorldSetDressing20261117", "Rogue10m.NaturalWorldSetDressing20261117.BossThreshold"],
            "instances": plan["instances"]}, plan


def validate_floor(u, actors, roads, check):
    """Read the exact three shallow circular earth projectors and their graph."""
    require(hashlib.sha256(FLOOR_PLAN.read_bytes()).hexdigest() == FLOOR_SHA256, "Immutable village-earth plan changed")
    plan = read(FLOOR_PLAN)
    require(plan["schema"] == "Rogue10m.VillageEarthFloor.20261118.v1" and plan["map"] == TARGET,
            "Village earth plan schema/map differs")
    protected = dict(plan["protected_material_sha256"])
    protected[plan["source_material"]] = plan["source_sha256"]
    check("village_earth_source6_disk_hashes", all(hashlib.sha256(
        (ROOT/"Content"/(name[6:]+".uasset")).read_bytes()).hexdigest() == digest for name,digest in protected.items()))
    material = u.load_asset(OWN+"/M_VillageEarth")
    require(isinstance(material, u.Material), "Owned village Material unavailable")
    lib = u.MaterialEditingLibrary
    nodes = list(lib.get_material_expressions(material))
    counts = Counter(n.get_class().get_name() for n in nodes)
    wanted = Counter({"MaterialExpressionWorldPosition":1,"MaterialExpressionComponentMask":1,
        "MaterialExpressionScalarParameter":3,"MaterialExpressionMultiply":4,"MaterialExpressionTextureSample":1,
        "MaterialExpressionVectorParameter":1,"MaterialExpressionTextureCoordinate":1,
        "MaterialExpressionConstant2Vector":1,"MaterialExpressionDistance":1,
        "MaterialExpressionSmoothStep":1,"MaterialExpressionOneMinus":1})
    check("village_earth_exact16_node_classes", len(nodes) == 16 and counts == wanted)
    require(counts == wanted, "Cannot inspect an unrecognized village material graph")
    scalars = {str(n.get_editor_property("parameter_name")):n for n in nodes if isinstance(n,u.MaterialExpressionScalarParameter)}
    tint = next(n for n in nodes if isinstance(n,u.MaterialExpressionVectorParameter))
    color = tint.get_editor_property("default_value")
    texture = next(n for n in nodes if isinstance(n,u.MaterialExpressionTextureSample))
    check("village_earth_palette_and_source", set(scalars) == {"WorldUVScale","RoadOpacity","RoadRoughness"}
        and all(abs(scalars[k].get_editor_property("default_value")-v) < 1e-6 for k,v in
                {"WorldUVScale":.002,"RoadOpacity":.97,"RoadRoughness":.9}.items())
        and str(tint.get_editor_property("parameter_name")) == "RoadTint"
        and max(abs(a-b) for a,b in zip((color.r,color.g,color.b,color.a),(.70,.54,.34,1))) < 1e-6
        and texture.get_editor_property("texture").get_path_name() == "/Game/StarterContent/Textures/T_Ground_Gravel_D.T_Ground_Gravel_D"
        and material.get_editor_property("material_domain") == u.MaterialDomain.MD_DEFERRED_DECAL
        and material.get_editor_property("blend_mode") == u.BlendMode.BLEND_TRANSLUCENT)
    def inputs(node):
        return [v for v in lib.get_inputs_for_material_expression(material,node) if v is not None]
    def exact_pins(node, expected):
        names = list(map(str,lib.get_material_expression_input_names(node)))
        sources = list(lib.get_inputs_for_material_expression(material,node))
        return len(names)==len(sources) and all(source == expected.get(name) for name,source in zip(names,sources))
    coord = next(n for n in nodes if isinstance(n,u.MaterialExpressionTextureCoordinate))
    center = next(n for n in nodes if isinstance(n,u.MaterialExpressionConstant2Vector))
    distance = next(n for n in nodes if isinstance(n,u.MaterialExpressionDistance))
    smooth = next(n for n in nodes if isinstance(n,u.MaterialExpressionSmoothStep))
    inverse = next(n for n in nodes if isinstance(n,u.MaterialExpressionOneMinus))
    opacity = lib.get_material_property_input_node(material,u.MaterialProperty.MP_OPACITY)
    radius = inputs(smooth)
    circle_ok = (len(radius)==1 and isinstance(radius[0],u.MaterialExpressionMultiply)
        and inputs(distance)==[coord,center] and inputs(radius[0])==[distance]
        and abs(radius[0].get_editor_property("const_b")-2)<1e-6
        and abs(center.get_editor_property("r")-.5)<1e-6 and abs(center.get_editor_property("g")-.5)<1e-6
        and abs(smooth.get_editor_property("const_min")-.975)<1e-6 and abs(smooth.get_editor_property("const_max")-1)<1e-6
        and inputs(inverse)==[smooth] and isinstance(opacity,u.MaterialExpressionMultiply)
        and exact_pins(distance,{"A":coord,"B":center})
        and exact_pins(radius[0],{"A":distance}) and exact_pins(smooth,{"Value":radius[0]})
        and exact_pins(opacity,{"A":scalars["RoadOpacity"],"B":inverse})
        and coord.get_editor_property("coordinate_index")==0
        and abs(coord.get_editor_property("u_tiling")-1)<1e-6
        and abs(coord.get_editor_property("v_tiling")-1)<1e-6)
    check("village_earth_circle_opacity_graph", circle_ok)
    world = next(n for n in nodes if isinstance(n,u.MaterialExpressionWorldPosition))
    xy = next(n for n in nodes if isinstance(n,u.MaterialExpressionComponentMask))
    base = lib.get_material_property_input_node(material,u.MaterialProperty.MP_BASE_COLOR)
    uv = inputs(texture)[0] if len(inputs(texture))==1 else None
    first_pin = lambda node: str(list(lib.get_material_expression_input_names(node))[0])
    check("village_earth_original_WorldXY_gravel_wiring", isinstance(base,u.MaterialExpressionMultiply)
        and isinstance(uv,u.MaterialExpressionMultiply) and exact_pins(base,{"A":texture,"B":tint})
        and exact_pins(uv,{"A":xy,"B":scalars["WorldUVScale"]}) and exact_pins(texture,{"UVs":uv})
        and exact_pins(xy,{first_pin(xy):world}) and not inputs(world)
        and [bool(xy.get_editor_property(k)) for k in ("r","g","b","a")]==[True,True,False,False]
        and lib.get_material_property_input_node(material,u.MaterialProperty.MP_ROUGHNESS)==scalars["RoadRoughness"])
    # Enumerate only native material-property slots known to have a valid input.
    active=[]
    for name in dir(u.MaterialProperty):
        prop=getattr(u.MaterialProperty,name)
        if name.startswith("MP_") and isinstance(prop,u.MaterialProperty) and prop.value in set(range(34))-{3,4,13,14}:
            if lib.get_material_property_input_node(material,prop) is not None: active.append(prop.value)
    check("village_earth_only_three_material_outputs", set(active)=={p.value for p in
        (u.MaterialProperty.MP_BASE_COLOR,u.MaterialProperty.MP_ROUGHNESS,u.MaterialProperty.MP_OPACITY)})
    expected = {"H":([1000,42550,9600],3750),"M":([29000,42550,9600],3750),"T":([22000,55550,9600],3650)}
    maximum = max(a.get_component_by_class(u.DecalComponent).get_editor_property("sort_order") for a in roads)
    require(len(roads)==29 and maximum<100, "Floor sort must be strictly above every road and at most100")
    sort_order = maximum+1
    rows=[]
    for hub,(center_cm,radius_cm) in expected.items():
        planned=[r for r in plan["decals"] if r["village"]==hub]
        require(len(planned)==1 and planned[0]["label"]=="NW18_VillageEarth_"+hub
                and planned[0]["center_cm"]==center_cm and planned[0]["half_extents_cm"]==[16,radius_cm,radius_cm],
                "Village-earth actor recipe differs")
        selected = [a for a in actors if a.get_actor_label()=="NW18_VillageEarth_"+hub]
        valid = len(selected)==1
        if valid:
            actor=selected[0];component=actor.get_component_by_class(u.DecalComponent);rotation=actor.get_actor_rotation()
            observed_q=rotation.quaternion();expected_q=u.Rotator(pitch=-90,yaw=0,roll=0).quaternion()
            qdot=sum(getattr(observed_q,k)*getattr(expected_q,k) for k in "xyzw")
            valid = (actor.get_class().get_path_name()=="/Script/Engine.DecalActor" and component is not None
                and max(abs(getattr(actor.get_actor_location(),axis)-v) for axis,v in zip("xyz",center_cm))<.02
                and max(abs(getattr(actor.get_actor_scale3d(),axis)-1) for axis in "xyz")<.0001
                and math.isfinite(qdot) and abs(abs(qdot)-1)<1e-5
                and not actor.get_editor_property("hidden") and component.get_decal_material()==material
                and not actor.get_editor_property("is_spatially_loaded")
                and str(actor.get_folder_path())=="NaturalWorld20261117/VillageIdentity"
                and set(map(str,actor.tags))=={"Rogue10m.WorldLayout20261112","Rogue10m.NaturalBoundary20261117",
                    "Rogue10m.VillageEarthFloor20261118","Rogue10m.VillageEarthFloor20261118."+hub}
                and max(abs(getattr(component.get_editor_property("decal_size"),axis)-v) for axis,v in zip("xyz",[16,radius_cm,radius_cm]))<.02
                and component.get_editor_property("fade_screen_size")==0 and component.get_editor_property("sort_order")==sort_order
                and all(component.get_editor_property(k)==0 for k in
                    ("fade_start_delay","fade_duration","fade_in_duration","fade_in_start_delay"))
                and component.get_editor_property("visible") and not component.get_editor_property("hidden_in_game"))
            primitives=list(actor.get_components_by_class(u.PrimitiveComponent))
            valid &= len(primitives)==2 and {(c.get_name(),c.get_class().get_path_name()) for c in primitives}=={
                ("ArrowComponent0","/Script/Engine.ArrowComponent"),("Sprite","/Script/Engine.BillboardComponent")}
            valid &= all(c.get_editor_property("bIsEditorOnly")
                and str(c.get_collision_profile_name())=="NoCollision"
                and c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION for c in primitives)
        rows.append({"hub":hub,"radius_cm":radius_cm,"passed":bool(valid)})
    check("village_earth_three_exact_shallow_visual_decals",all(r["passed"] for r in rows))
    return {"material":material.get_path_name(),"class_counts":dict(counts),"sort_order":sort_order,"decals":rows}
