"""Independent, read-only complete/cold QA of the two reviewed cave spaces.

Import is inert. The explicit function returns a report without writing files,
saving, changing properties, spawning, starting PIE, drawing or retrying. It
checks the frozen plans, retained scene/materials, real component transforms,
and finite simple-Pawn sweeps. Camera settings are checked; actual camera
traversal and rendered acceptance remain a separate required test.
"""
from __future__ import annotations

from collections import OrderedDict
import copy
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import struct
import traceback

ROOT = Path("D:/Project/Rogue10m")
BASE = ROOT / "Feature/doc/evidence/caves20261118"
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
OWN = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117"
CAVE_OWN = OWN + "/Caves20261118"
PLAN_SHA = "39b0150690e02a6edb453f91d0362603d9063dc3771dce9977d870fe184e6a74"
DRESSING_SHA = "4177a9e1b8544e16cd61217cf64fd01a5c47da8e6ef3ee84dea25228107449db"
VISIBILITY_SHA = "5509ec2f171704b4b963cb82726c9660f4e2c5b05ac883394ef63e72de644d81"
WALL_TEXTURE_SHA = "a4127f8ca450c50c114001d60c92e05da98c380c9597c22715f382c3827b3cd8"
TRIPLANAR_SHA = "0d24c669a60f9c998401afcc7f7c75e74f6fe1f72c499496fda3139e5c47353f"
EXPOSURE_SHA = "bc58e611db30ae46e604be7b3c72a3af02a94a57a6eef288faf23eb5feea8627"
EXPOSURE_RECEIPT_SHA = "89c6924c0f886c4d7b7367d37af1c9c67972973128030c9473b06bcbaf56667a"
BOUNDS_SHA = "fb2cb4bd5e9108507dee21d16af499c1ef19679ebbe029244705b2d55586fc70"
BOUNDS_RECEIPT_SHA = "f8f002c576404a05c9686ef83d16b027c939f5422b81bdd67837c55d27ced349"
CAMERA_RELIEF_SHA = "92e2464e246528952718203c7e82ed7455c20102fa7b8c5586835229f02fb92f"
COLLAR_SHA = "4f3d5dacfdee8af8e00e32c6db3597208061e2593e488f74cdd88de334a3696a"
COLLAR_PREFLIGHT_SHA = "2fd5f9dd3c8d57eb723d215ac724724b54234e8b23389971fe4e91f67a8e84ca"
ORIGINAL_LAYER = "7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955"
LANDSCAPE_GUID = "147B73D247543E06FDA1A7AF9D467458"
NEW_MATERIALS = {"M_CaveSurface", "MI_CaveGrass", "MI_CaveStoneFloor", "MI_CaveRock",
                 "M_CaveLamp", "MI_CaveLampWarm", "MI_CaveLampCool"}
STRUCT_ADDRESS = re.compile(r"(?<=\(0x)[0-9A-Fa-f]+(?=\) \{)")


def require(value, message):
    if not value:
        raise RuntimeError(message)


def module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def read(relative):
    return json.loads((BASE / relative).read_text(encoding="utf-8-sig"))


def path(value):
    return value.get_path_name() if value else None


def xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def normalized_addresses(value):
    """Only strip the process address in repr(<Struct ...>); keep its fields."""
    if isinstance(value, dict):
        return {k: normalized_addresses(v) for k, v in value.items()}
    if isinstance(value, list):
        return [normalized_addresses(v) for v in value]
    if isinstance(value, str) and value.startswith("<Struct "):
        return STRUCT_ADDRESS.sub("ADDRESS", value)
    return value


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False,
                                    separators=(",", ":"), allow_nan=False).encode()).hexdigest()


def near(a, b, tolerance=0.05):
    return len(a) == len(b) and all(math.isfinite(float(x)) and abs(float(x)-float(y)) <= tolerance for x, y in zip(a, b))


def instance_transform(u, component, index):
    value = component.get_instance_transform(index, world_space=True)
    if isinstance(value, tuple):
        require(not any(v is False for v in value), "Native instance transform reported failure")
        transforms = [v for v in value if isinstance(v, u.Transform)]
        require(len(transforms) == 1, "Ambiguous native instance transform output")
        value = transforms[0]
    require(isinstance(value, u.Transform), "Native instance transform missing")
    return value


def cell_world(site, cell):
    return [site["origin_cm"][i] + (cell[i]+0.5)*500.0 for i in (0, 1)]


def ramp_ground(site, x, y):
    ramp = site["ramp"]
    return 9600.0 - 1000.0 * (y-ramp["top_y_cm"]) / (ramp["bottom_y_cm"]-ramp["top_y_cm"])


def inside_expanded_rect(x, y, rect, pad):
    return rect[0]-pad <= x <= rect[2]+pad and rect[1]-pad <= y <= rect[3]+pad


def point_segment_distance(point, a, b):
    dx, dy = b[0]-a[0], b[1]-a[1]
    t = max(0.0, min(1.0, ((point[0]-a[0])*dx+(point[1]-a[1])*dy)/(dx*dx+dy*dy)))
    return math.hypot(point[0]-a[0]-t*dx, point[1]-a[1]-t*dy)


def build_probe_cases(plan):
    """Pure finite coverage from all unique planned route/graph edges."""
    cases, counts = [], {}
    for site in plan["sites"]:
        key = site["site_id"]
        actor_label = "CV18_" + key + "_Structure"
        edges = {}
        for a, b in site["graph_edges"]:
            p, q = [*cell_world(site, a), 8610.0], [*cell_world(site, b), 8610.0]
            edges[tuple(sorted((tuple(p), tuple(q))))] = (p, q)
        # Include the actual ramp and transition to the first floor cell.
        for route in site["runtime_routes"].values():
            for p, q in zip(route, route[1:]):
                if p[2] != 8610.0 or q[2] != 8610.0:
                    edges[tuple(sorted((tuple(p), tuple(q))))] = (list(p), list(q))
        for index, (p, q) in enumerate(edges.values()):
            dx, dy = q[0]-p[0], q[1]-p[1]
            length = math.hypot(dx, dy)
            require(length > 1, "Degenerate cave route edge")
            lateral = (-dy/length, dx/length)
            sloped = p[2] != 8610.0 or q[2] != 8610.0
            clearance = 40.0 if sloped else 10.0
            for radius in (34.0, 42.0):
                for lane in (-120.0, 0.0, 120.0):
                    start = [p[0]+lateral[0]*lane, p[1]+lateral[1]*lane, p[2]+96.0+clearance]
                    end = [q[0]+lateral[0]*lane, q[1]+lateral[1]*lane, q[2]+96.0+clearance]
                    for direction, a, b in (("forward", start, end), ("reverse", end, start)):
                        cases.append({"id": f"{key}_edge{index:03d}_r{radius:g}_lane{lane:g}_{direction}",
                            "kind": "route_clear", "site": key, "radius": radius, "start": a, "end": b,
                            "expected_blocking": False, "sloped_reference_clearance_cm": clearance})
        counts[key] = {"unique_route_edges": len(edges), "route_queries": len(edges)*12}
        for index, cell in enumerate(site["floor_cells"]):
            x, y = cell_world(site, cell)
            cases.append({"id": f"{key}_floor_support_{index:03d}", "kind": "floor_support", "site": key,
                "radius": 34.0, "start": [x, y, 8610+96+100], "end": [x, y, 8610+96-20],
                "expected_blocking": True, "actor_label": actor_label, "impact_z": 8610.0})
            cases.append({"id": f"{key}_ceiling_{index:03d}", "kind": "ceiling", "site": key,
                "radius": 34.0, "start": [x, y, 8610+200], "end": [x, y, 9200+100],
                "expected_blocking": True, "actor_label": actor_label, "impact_z": 9200.0})
        for index, t in enumerate((0.05, 0.25, 0.5, 0.75, 0.95)):
            ramp = site["ramp"]
            x, y = ramp["x_cm"], ramp["top_y_cm"]+t*(ramp["bottom_y_cm"]-ramp["top_y_cm"])
            z = ramp_ground(site, x, y)
            cases.append({"id": f"{key}_ramp_support_{index}", "kind": "ramp_support", "site": key,
                "radius": 34.0, "start": [x, y, z+96+150], "end": [x, y, z+96-30],
                "expected_blocking": True, "actor_label": actor_label})
        walls = [r for r in site["components"] if "_Wall_" in r["label"]]
        # Exercise ten spread-out physical walls per site, plus all forced
        # secret-interface walls when they are among those deterministic picks.
        selected = [walls[round(i*(len(walls)-1)/9)] for i in range(10)]
        cells = set(map(tuple, site["floor_cells"]))
        for index, wall in enumerate(selected):
            x, y, _ = wall["location_cm"]
            normal = (0.0, 1.0) if wall["dimensions_cm"][0] > wall["dimensions_cm"][1] else (1.0, 0.0)
            candidates = [(x+normal[0]*250*sign, y+normal[1]*250*sign) for sign in (-1, 1)]
            starts = [p for p in candidates if (math.floor((p[0]-site["origin_cm"][0])/500),
                      math.floor((p[1]-site["origin_cm"][1])/500)) in cells]
            require(starts, "Wall has no adjacent walkable floor")
            p = starts[0]
            cases.append({"id": f"{key}_wall_{index:02d}", "kind": "wall", "site": key, "radius": 34.0,
                "start": [p[0], p[1], 8610+96+10], "end": [2*x-p[0], 2*y-p[1], 8610+96+10],
                "expected_blocking": True, "actor_label": actor_label, "wall_label": wall["label"]})
        x0, y0, x1, y1 = site["roof_rect_cm"]
        for index, (fx, fy) in enumerate(((0.25, .25), (.75, .25), (.25, .75), (.75, .75), (.5, .5))):
            x, y = x0+(x1-x0)*fx, y0+(y1-y0)*fy
            cases.append({"id": f"{key}_surface_roof_{index}", "kind": "surface_roof", "site": key,
                "radius": 34.0, "start": [x, y, 9602+96+100], "end": [x, y, 9602+96-20],
                "expected_blocking": True, "actor_label": actor_label, "impact_z": 9602.0})
    require(len({case["id"] for case in cases}) == len(cases), "Duplicate capsule case IDs")
    require(all(1 <= math.dist(c["start"], c["end"]) <= 10000 for c in cases), "Capsule length outside native limits")
    return cases, counts


def material_live_state(u, obj, reader):
    lib, base, parents, seen = u.MaterialEditingLibrary, obj, [], set()
    while isinstance(base, u.MaterialInstance):
        require(path(base) not in seen, "New material parent cycle")
        seen.add(path(base))
        base = base.get_editor_property("parent")
        require(base is not None, "Missing new material parent")
        parents.append(path(base))
    require(isinstance(base, u.Material), "Concrete material required")
    result = {"path": path(obj), "parent_chain": parents, "base_material": path(base),
        "used_with_instanced_static_meshes": bool(base.get_editor_property("used_with_instanced_static_meshes")),
        "graph": reader._fingerprint(u, lib, base)}
    if isinstance(obj, u.MaterialInstance):
        result["scalar_values"] = {str(k): lib.get_material_instance_scalar_parameter_value(obj, k) for k in lib.get_scalar_parameter_names(base)}
        result["vector_values"] = {str(k): [float(getattr(lib.get_material_instance_vector_parameter_value(obj, k), a)) for a in "rgba"]
                                   for k in lib.get_vector_parameter_names(base)}
        result["texture_values"] = {str(k): path(lib.get_material_instance_texture_parameter_value(obj, k)) for k in lib.get_texture_parameter_names(base)}
    return result


def refined_material_expectations(u, original, receipt, spec):
    """Derive the only permitted material delta, rather than accept any graph."""
    before={p:{k:v for k,v in value.items() if k!="unsaved"} for p,value in original.items()}
    require(receipt["material_states_before"]==before,"Visibility material-before evidence differs from original authoring")
    surface_key=CAVE_OWN+"/M_CaveSurface.M_CaveSurface"
    old=before[surface_key]["graph"];new=receipt["surface_graph_after"]
    require(receipt["surface_graph_before"]==old and len(old["nodes"])==13 and len(new["nodes"])==18,
            "Exact13 to18-node surface revision required")
    require(all(new["nodes"].get(name)==value for name,value in old["nodes"].items()),"An original surface node changed")
    names=receipt["surface_slope_nodes"]
    classes={"pixel_normal_ws":"MaterialExpressionPixelNormalWS","mask_z":"MaterialExpressionComponentMask",
             "smooth":"MaterialExpressionSmoothStep","color":"MaterialExpressionConstant3Vector",
             "lerp":"MaterialExpressionLinearInterpolate"}
    require(set(names)==set(classes) and len(set(names.values()))==5
            and set(new["nodes"])-set(old["nodes"])==set(names.values()),"Exact five declared slope nodes required")
    require(all(new["nodes"][names[k]]["class"]==v for k,v in classes.items()),"Slope node classes differ")
    base_key=str(u.MaterialProperty.MP_BASE_COLOR.value)
    require({k:v for k,v in new.items() if k not in {"nodes","outputs"}}==
            {k:v for k,v in old.items() if k not in {"nodes","outputs"}}
            and all(new["outputs"][k]==v for k,v in old["outputs"].items() if k!=base_key)
            and new["outputs"][base_key]["node"]==names["lerp"],"Only the BaseColor output may change")
    mask=new["nodes"][names["mask_z"]]
    require(mask["properties"]=={"r":False,"g":False,"b":True,"a":False}
            and [r["node"] for r in mask["inputs"] if r["node"]]==[names["pixel_normal_ws"]],"World-normal B-only mask differs")
    smooth=new["nodes"][names["smooth"]]
    require(abs(smooth["properties"]["const_min"]-.5)<1e-6 and abs(smooth["properties"]["const_max"]-.9)<1e-6
            and smooth["properties"]["const_value"]==0.0,"Slope smoothstep constants differ")
    require({r["pin"]:r["node"] for r in smooth["inputs"]}=={"Min":None,"Max":None,"Value":names["mask_z"]},
            "Slope smoothstep wiring differs")
    lerp_inputs={r["pin"]:r for r in new["nodes"][names["lerp"]]["inputs"]}
    require({k:v["node"] for k,v in lerp_inputs.items()}=={"A":names["color"],"B":old["outputs"][base_key]["node"],"Alpha":names["smooth"]}
            and lerp_inputs["B"]["output"]==old["outputs"][base_key]["output"],"Side/grass Lerp wiring differs")
    explicit=receipt["surface_explicit_node_properties"]
    require(near(explicit["color_constant_rgba"],spec["side_color_linear_rgba"],1e-6)
            and near([explicit[k] for k in ("lerp_const_a","lerp_const_b","lerp_const_alpha")],[0,1,.5],1e-6),
            "Historical explicit visibility constants differ")
    # A parent's changed graph is visible through its two instances; every
    # parameter, parent, path, flag and other material graph must stay exact.
    expected=copy.deepcopy(before)
    for value in expected.values():
        if value["base_material"]==surface_key:value["graph"]=new
    require(receipt["expected_material_states_after"]==expected,"Visibility receipt changes a material beyond the approved surface graph")
    return expected


def wall_binding_history(material_roles, visibility):
    """Check v1's semantic binding proof; its visual rejection stays separate."""
    require(hashlib.sha256((BASE/"wall-texture-spec.json").read_bytes()).hexdigest()==WALL_TEXTURE_SHA,
            "Frozen first wall-binding specification changed")
    spec,receipt=read("wall-texture-spec.json"),read("author-live/wall-texture.json")
    require(spec["schema"]=="Rogue10m.CaveWallTextureSpec.20261118.v1" and spec["map"]==TARGET
            and spec["structural_plan_sha256"]==PLAN_SHA
            and hashlib.sha256((BASE/"author-live/visibility.json").read_bytes()).hexdigest()==spec["visibility_receipt_sha256"]
            and receipt.get("success") is True and receipt["spec_sha256"]==WALL_TEXTURE_SHA
            and receipt["visibility_receipt_sha256"]==spec["visibility_receipt_sha256"],
            "Successful exact wall-binding history required")
    require(receipt["material_states_unchanged"]==visibility["expected_material_states_after"],
            "First wall-binding stage changed a material state")
    structures=read("author-live/structures.json")
    require(structures.get("success") is True and structures["plan_sha256"]==PLAN_SHA,"Successful original structure proof required")
    by_site={s["site"]:s for s in structures["sites"]}
    require(len(spec["binding_changes"])==len(receipt["binding_changes"])==2,"Exactly two first-stage bindings required")
    result={}
    for desired in spec["binding_changes"]:
        site=by_site[desired["site_id"]]
        bins=[c for c in site["components"] if c["role"]=="stone_wall"]
        require(len(bins)==1,"One original stone-wall bin per site required")
        c=bins[0]
        require(desired["actor_path"]==site["actor_path"] and desired["component_path"]==c["path"]
                and desired["package"]==site["package"] and desired["instance_count"]==len(c["instances"])
                and desired["old_material"]==material_roles["stone_wall"]==c["material"].split(".")[0]
                and desired["new_material"]==CAVE_OWN+"/MI_CaveRock" and desired["slot"]==0,
                "First binding stage differs from original role/bin/count")
        observed=[r for r in receipt["binding_changes"] if r["site_id"]==desired["site_id"]]
        require(len(observed)==1 and {k:observed[0][k] for k in desired}==desired
                and observed[0]["observed_material"].split(".")[0]==desired["new_material"],
                "First binding receipt does not match its frozen specification")
        result[desired["site_id"]]=desired
    require(set(result)=={"Q","Z"} and result["Q"]["instance_count"]==84 and result["Z"]["instance_count"]==86,
            "Exact84/86 wall-plus-ceiling history required")
    return result


def triplanar_expectations(u, historical_materials, visibility, first_bindings):
    """Validate the exact25-node world-projected program and two final bindings."""
    require(hashlib.sha256((BASE/"triplanar-spec.json").read_bytes()).hexdigest()==TRIPLANAR_SHA,
            "Frozen triplanar specification changed")
    spec,receipt=read("triplanar-spec.json"),read("author-live/triplanar.json")
    require(spec["schema"]=="Rogue10m.CaveTriplanarStoneSpec.20261118.v1" and spec["map"]==TARGET
            and spec["prior_wall_candidate_visual_status"]=="rejected_cube_uv_atlas_mismatch"
            and hashlib.sha256((BASE/"author-live/visibility.json").read_bytes()).hexdigest()==spec["visibility_receipt_sha256"]
            and hashlib.sha256((BASE/"author-live/wall-texture.json").read_bytes()).hexdigest()==spec["wall_v1_receipt_sha256"]
            and receipt.get("success") is True and receipt["spec_sha256"]==TRIPLANAR_SHA
            and receipt["visibility_receipt_sha256"]==spec["visibility_receipt_sha256"]
            and receipt["wall_v1_receipt_sha256"]==spec["wall_v1_receipt_sha256"],"Exact successful triplanar provenance required")
    require(receipt["material_states_before"]==historical_materials and receipt["surface_graph_before"]==visibility["surface_graph_after"],
            "Triplanar stage does not start at the preserved18-node state")
    before,after=receipt["surface_graph_before"],receipt["surface_graph_after"]
    names=receipt["triplanar_nodes"];descriptions={n["key"]:n for n in spec["nodes"]}
    require(len(before["nodes"])==18 and len(after["nodes"])==43 and len(names)==25
            and set(names)==set(descriptions) and len(set(names.values()))==25
            and set(after["nodes"])-set(before["nodes"])==set(names.values()),"Exact18 to43-node triplanar delta required")
    require(after["outputs"]==before["outputs"] and {k:v for k,v in after.items() if k not in {"nodes","outputs"}}==
            {k:v for k,v in before.items() if k not in {"nodes","outputs"}},"Triplanar revision changed a material output/domain")
    original13=visibility["surface_graph_before"]["nodes"]
    require(all(after["nodes"].get(k)==v for k,v in original13.items()),"An original13 surface node changed")
    require(spec["existing_lerp"]==visibility["surface_slope_nodes"]["lerp"]
            and spec["existing_aliases"]["existing_pixel_normal_ws"]==visibility["surface_slope_nodes"]["pixel_normal_ws"]
            and spec["existing_aliases"]["existing_side_color"]==visibility["surface_slope_nodes"]["color"],"Visibility aliases differ")
    lerp_name=spec["existing_lerp"]
    for name,value in before["nodes"].items():
        actual=copy.deepcopy(after["nodes"][name])
        if name==lerp_name:
            new_a=[r for r in actual["inputs"] if r["pin"]=="A"]
            require(len(new_a)==1 and new_a[0]["node"]==names["tinted_stone"],"Only Lerp.A may point at triplanar stone")
            actual["inputs"]=[next(r for r in value["inputs"] if r["pin"]=="A") if r["pin"]=="A" else r for r in actual["inputs"]]
        require(actual==value,"Existing visibility controls changed beyond Lerp.A")
    surface_key=CAVE_OWN+"/M_CaveSurface.M_CaveSurface";surface=u.load_asset(surface_key)
    lib=u.MaterialEditingLibrary
    expressions={n.get_name():n for n in lib.get_material_expressions(surface)}
    worlds=[name for name,value in before["nodes"].items() if value["class"]=="MaterialExpressionWorldPosition"]
    require(len(worlds)==1,"One retained WorldPosition alias required")
    aliases={"existing_world_position":worlds[0],"existing_pixel_normal_ws":spec["existing_aliases"]["existing_pixel_normal_ws"],
             "existing_side_color":spec["existing_aliases"]["existing_side_color"]}
    all_names={**aliases,**names}
    records={r["key"]:r for r in receipt["triplanar_node_records"]}
    require(set(records)==set(descriptions),"Exact25 explicit triplanar node records required")
    for key,description in descriptions.items():
        node=after["nodes"][names[key]];obj=expressions[names[key]]
        require(node["class"]==description["class"]==obj.get_class().get_name()
                and records[key]["name"]==names[key] and records[key]["class"]==description["class"]
                and records[key]["inputs"]==node["inputs"] and records[key]["reader_properties"]==node["properties"],
                "Triplanar node class/input record differs: "+key)
        for prop,wanted in description["properties"].items():
            actual=obj.get_editor_property(prop)
            if prop=="texture":
                require(path(actual)==wanted==spec["fixed_stone_texture"],"Fixed triplanar texture differs")
            elif prop=="sampler_type":require(actual==u.MaterialSamplerType.SAMPLERTYPE_COLOR,"Triplanar color sampler type differs")
            elif prop=="sampler_source":require(actual==u.SamplerSourceMode.SSM_FROM_TEXTURE_ASSET,"Triplanar sampler source differs")
            elif isinstance(wanted,bool):require(actual is wanted,"Triplanar mask differs: "+key+"."+prop)
            else:require(abs(float(actual)-wanted)<=1e-6,"Triplanar constant differs: "+key+"."+prop)
        links=[r for r in spec["links"] if r["destination"]==key]
        connected=[r for r in node["inputs"] if r["node"]]
        require(len(connected)==len(links),"Unexpected triplanar input connection: "+key)
        for link in links:
            source=all_names[link["source"]]
            inputs=[r for r in node["inputs"] if r["pin"]==link["pin"]] if link["pin"] else node["inputs"][:1]
            outputs=list(map(str,lib.get_material_expression_output_names(expressions[source])))
            require(outputs,"Triplanar source has no native output")
            wanted_output=link["output"] or outputs[0]
            require(len(inputs)==1 and inputs[0]["node"]==source and inputs[0]["output"]==wanted_output,
                    "Triplanar projection/weight/texture wiring differs: "+key+"."+link["pin"])
    color=expressions[aliases["existing_side_color"]].get_editor_property("constant")
    require(near([getattr(color,k) for k in "rgba"],spec["side_color_after"],1e-6)
            and near(receipt["side_color_before"],spec["side_color_before"],1e-6)
            and near(receipt["side_color_after"],spec["side_color_after"],1e-6)
            and spec["world_tile_cm"]==240.0,"Actual world-stone tint or240cm tile differs")
    texture=u.load_asset(spec["fixed_stone_texture"])
    require(texture.get_editor_property("srgb") is True and texture.get_editor_property("address_x")==u.TextureAddress.TA_WRAP
            and texture.get_editor_property("address_y")==u.TextureAddress.TA_WRAP,"Existing stone source texture settings differ")
    expected=copy.deepcopy(historical_materials)
    for state in expected.values():
        if state["base_material"]==surface_key:state["graph"]=after
    require(receipt["expected_material_states_after"]==expected,"Triplanar receipt changes parameters/parentage or another graph")
    require(len(spec["binding_changes"])==len(receipt["binding_changes"])==2,"Exactly two final wall bindings required")
    final={}
    for binding in spec["binding_changes"]:
        old=first_bindings[binding["site_id"]]
        require(all(binding[k]==old[k] for k in ("site_id","actor_path","component_path","package","instance_count","slot","wall_instances","stone_ceiling_instances"))
                and binding["old_material"]==old["new_material"]==CAVE_OWN+"/MI_CaveRock"
                and binding["new_material"]==CAVE_OWN+"/MI_CaveStoneFloor","Final wall binding differs from recorded two-bin history")
        observed=[r for r in receipt["binding_changes"] if r["site_id"]==binding["site_id"]]
        require(len(observed)==1 and all(observed[0][k]==v for k,v in binding.items() if k!="observed_material")
                and observed[0]["observed_material"].split(".")[0]==binding["new_material"],"Final binding receipt differs")
        final[binding["site_id"]]=binding
    return expected,final


def current_global_pp_state(u, actors):
    result={}
    for actor in actors:
        if not isinstance(actor,u.PostProcessVolume):continue
        settings=actor.get_editor_property("settings");text=settings.export_text()
        result[path(actor)]={"label":actor.get_actor_label(),"settings_export_text":text,
            "settings_sha256":hashlib.sha256(text.encode()).hexdigest(),
            "priority":float(actor.get_editor_property("priority")),"unbound":bool(actor.get_editor_property("unbound")),
            "enabled":bool(actor.get_editor_property("enabled")),"blend_radius_cm":float(actor.get_editor_property("blend_radius")),
            "blend_weight":float(actor.get_editor_property("blend_weight")),
            "min":float(settings.get_editor_property("auto_exposure_min_brightness")),
            "max":float(settings.get_editor_property("auto_exposure_max_brightness")),
            "override_min":bool(settings.get_editor_property("override_auto_exposure_min_brightness")),
            "override_max":bool(settings.get_editor_property("override_auto_exposure_max_brightness"))}
    return result


def camera_relief_expectations(dressing):
    """Apply only the two frozen world-X matrix offsets to an in-memory plan."""
    require(hashlib.sha256((BASE/"camera-relief-spec.json").read_bytes()).hexdigest()==CAMERA_RELIEF_SHA,
            "Frozen two-rock camera relief specification changed")
    spec=read("camera-relief-spec.json");receipt=read("author-live/camera-relief.json")
    require(spec["schema"]=="Rogue10m.CaveCameraReliefSpec.20261118.v1" and spec["map"]==TARGET
            and receipt["schema"]=="Rogue10m.CaveCameraRelief.20261118.v1"
            and receipt.get("success") is True and receipt["spec_sha256"]==CAMERA_RELIEF_SHA
            and receipt["input_sha256"]==spec["input_sha256"],"Successful exact two-rock repair receipt required")
    for relative,expected in spec["input_sha256"].items():
        require(hashlib.sha256((BASE/relative).read_bytes()).hexdigest()==expected,
                "Camera relief predecessor changed: "+relative)
    require(spec["native_composite_blake3"]==receipt["composite_blake3"]
            =="67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc"
            and spec["original_layer_blake3"]==receipt["original_layer_blake3"]==ORIGINAL_LAYER
            and spec["new_actors_components_instances_assets"]==0
            and spec["all_2016_capsule_cases_unchanged"] is True and spec["all_276_runtime_waypoints_unchanged"] is True
            and spec["camera_minimum_distance_cm_unchanged"]==140.0
            and spec["camera_sustained_max_pull_in_cm_unchanged"]==180.0,
            "Camera relief changed terrain, content counts, routes or camera acceptance thresholds")
    require(receipt["changed_instance_count"]==2 and receipt["actor_count"]==1408
            and receipt["checked_instance_matrix_predecessors"]=={"structure":286,"dressing":426}
            and receipt["original1404_preserved"] is True and receipt["all_other_instances_and_components_preserved"] is True
            and receipt["actor_snapshot_sha256_before"]==receipt["normalized_actor_snapshot_sha256_after"]
            and receipt["thresholds_and_routes_unchanged"] is True and receipt["actual_camera_pass_claimed"] is False
            and not receipt["assets_saved"] and not receipt["save_called"]
            and not receipt["created_actors"] and not receipt["created_components"] and not receipt["created_assets"]
            and receipt["dirty_before"]==spec["expected_dirty_before"]==[]
            and receipt["dirty_after"]==spec["expected_dirty_after"] and len(receipt["dirty_after"])==1,
            "Repair scope/preservation/save/runtime claim differs")
    previous_bounds=read("author-live/exposure-bounds.json")
    require(normalized_addresses(receipt["effects_before"])==normalized_addresses(receipt["effects_after"])
            ==normalized_addresses(previous_bounds["effects_after"]),"Camera repair changed global/camera/light/local exposure state")
    def author_digest(value):
        return hashlib.sha256((json.dumps(value,sort_keys=True,ensure_ascii=False,separators=(",",":"),allow_nan=False)+"\n").encode()).hexdigest()
    require(receipt["material_states_sha256"]==author_digest(read("author-live/triplanar.json")["expected_material_states_after"])
            and receipt["source40_material_states_sha256"]==author_digest(read("baseline-live/baseline-materials.json")["materials"]),
            "Camera repair material preservation proof differs")
    original=read("author-live/dressing.json")
    require(original["success"] is True and len(spec["instance_changes"])==len(receipt["instance_changes"])==2,
            "Exactly two repaired dressing matrices required")
    result=copy.deepcopy(dressing);overlays={};labels=set()
    for change in spec["instance_changes"]:
        require(change["site_id"]=="Q" and change["index"] in (120,121)
                and change["label"]=="Q_Rock_Q_Wall_V_03_"+("060" if change["index"]==120 else "061")
                and change["world_translation_delta_cm"]==[50.0,0.0,0.0],"Only the diagnosed two Q wall-skin instances may move +50cmX")
        authored_site=next(s for s in original["sites"] if s["site_id"]=="Q")
        authored_bin=next(c for c in authored_site["components"] if c["path"]==change["component_path"])
        old=authored_bin["instances"][change["index"]]
        before,after=change["before_world_transform"],change["after_world_transform"]
        actual=next(c for c in receipt["instance_changes"] if c["index"]==change["index"])
        require(change["actor_path"]==authored_site["actor_path"] and change["package"]==authored_site["package"]
                and old["index"]==change["index"] and old["label"]==change["label"] and old["world_transform"]==before
                and before["q"]==after["q"] and before["s"]==after["s"]
                and after["p"]==[before["p"][0]+50.0,*before["p"][1:]]
                and all(actual[k]==v for k,v in change.items())
                and all(near(actual["observed_world_transform_before"][k],before[k],.01 if k=="p" else 1e-6)
                        and near(actual["observed_world_transform_after"][k],after[k],.01 if k=="p" else 1e-6) for k in ("p","q","s")),
                "The exact original matrix, identity and X-only repair readback are required")
        plan_site=next(s for s in result["sites"] if s["site_id"]=="Q")
        row=next(r for r in plan_site["instances"] if r["label"]==change["label"])
        require(row["mesh"]=="boulder_medium" and row["material_role"]=="rock"
                and change["label"] not in labels,"Repair targets a different or duplicate dressing role")
        row["bounds_center_cm"][0]+=50.0
        key=(change["component_path"],change["index"])
        require(key not in overlays,"Duplicate repair matrix key")
        overlays[key]=change;labels.add(change["label"])
    require(len(overlays)==len(labels)==2,"Exact two unique repair transforms required")
    return result,overlays


def bounded_exposure_history(visibility):
    """Bind the rejected EV0 experiment and the actual query-shape repair."""
    def pinned(name, expected):
        require(hashlib.sha256((BASE/name).read_bytes()).hexdigest()==expected,
                "Frozen exposure artifact changed: "+name)
        return read(name)

    exposure_spec=pinned("exposure-spec.json",EXPOSURE_SHA)
    exposure=pinned("author-live/exposure.json",EXPOSURE_RECEIPT_SHA)
    bounds_spec=pinned("exposure-bounds-spec.json",BOUNDS_SHA)
    bounds=pinned("author-live/exposure-bounds.json",BOUNDS_RECEIPT_SHA)
    triplanar=read("author-live/triplanar.json")
    tri_sha=hashlib.sha256((BASE/"author-live/triplanar.json").read_bytes()).hexdigest()
    vis_sha=hashlib.sha256((BASE/"author-live/visibility.json").read_bytes()).hexdigest()
    require(exposure_spec["schema"]=="Rogue10m.CaveExposureSpec.20261118.v1"
            and bounds_spec["schema"]=="Rogue10m.CaveExposureBoundsSpec.20261118.v1"
            and exposure["schema"]=="Rogue10m.CaveExposureTune.20261118.v1"
            and bounds["schema"]=="Rogue10m.CaveExposureBoundsRepair.20261118.v1"
            and exposure.get("success") is True and bounds.get("success") is True
            and exposure["spec_sha256"]==EXPOSURE_SHA and bounds["spec_sha256"]==BOUNDS_SHA
            and bounds["exposure_receipt_sha256"]==bounds_spec["exposure_receipt_sha256"]==EXPOSURE_RECEIPT_SHA,
            "Successful exact exposure experiment and bounded-shape repair receipts required")
    for recipe in (exposure_spec,bounds_spec):
        require(recipe["map"]==TARGET and recipe["visibility_receipt_sha256"]==vis_sha
                and recipe["triplanar_receipt_sha256"]==tri_sha
                and recipe["native_composite_blake3"]=="67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc"
                and recipe["original_layer_blake3"]==ORIGINAL_LAYER
                and recipe["new_actors"]==recipe["new_assets"]==recipe["new_components"]==0,
                "Exposure repair provenance or unchanged content contract differs")
    require(bounds_spec["ev0_candidate_visually_accepted"] is False
            and bounds_spec["supersedes_no_collision_fallback_hypothesis"] is True,
            "The rejected EV0 candidate and invalid NoCollision hypothesis must remain recorded")
    for stage in (exposure,bounds):
        require(stage["actor_count"]==1408 and not stage["assets_saved"] and not stage["save_called"]
                and not stage["created_actors"] and not stage["created_assets"] and not stage["created_components"]
                and stage["material_states_unchanged"]==triplanar["expected_material_states_after"],
                "Exposure stages altered actors/assets/materials or saved unexpectedly")
        normalized=copy.deepcopy(stage["effects_after"])
        require(set(normalized["local_pp"])==set(stage["effects_before"]["local_pp"]) and len(normalized["local_pp"])==2,
                "Exactly two existing local post-process components required")
        for pp_path in normalized["local_pp"]:
            normalized["local_pp"][pp_path]["settings"]=stage["effects_before"]["local_pp"][pp_path]["settings"]
        require(normalized_addresses(normalized)==normalized_addresses(stage["effects_before"]),
                "Global post-process/camera/lights/bounds/non-exposure properties changed")
    require(normalized_addresses(bounds["effects_before"])==normalized_addresses(exposure["effects_after"]),
            "Exposure repair did not start from the recorded EV0 experiment")
    require(len(exposure["sites"])==len(bounds["sites"])==len(bounds_spec["sites"])==2,
            "Exactly two frozen exposure repair sites required")
    sites={}
    for site in bounds_spec["sites"]:
        sid=site["site_id"]
        require(sid in ("Q","Z") and sid not in sites,"Duplicate or unknown repaired exposure site")
        prior=next(s for s in exposure["sites"] if s["site_id"]==sid)
        old=next(s for s in visibility["postprocess_sites"] if s["site_id"]==sid)
        actual=next(s for s in bounds["sites"] if s["site_id"]==sid)
        require(all(actual[k]==v for k,v in site.items()) and site["before_ev100"]==0.0 and site["after_ev100"]==3.0
                and site["before_collision"]=="NO_COLLISION" and site["after_collision"]=="QUERY_ONLY"
                and site["all_collision_responses"]=="IGNORE"
                and prior["before_ev100"]==3.0 and prior["after_ev100"]==0.0 and prior["only_min_max_changed"] is True
                and prior["before_settings_export_text"]==old["settings_export_text"]==site["after_settings_export_text"]
                and prior["after_settings_export_text"]==site["before_settings_export_text"]
                and actual["actual_settings_export_text_after"]==site["after_settings_export_text"],
                "The two local full settings must follow exact EV3 to EV0 to original EV3")
        for key in ("actor_path","box_path","post_process_path"):
            require(site[key]==prior[key]==old[key],"Exposure repair changed a component identity")
        require(actual["all_channels_ignore"] is True and actual["new_overrides"]==0
                and not actual["overlap_events"] and not actual["navigation"] and not actual["tick_enabled"]
                and abs(actual["after_center_query"]["distance_cm"])<=.01
                and abs(actual["after_outside_query"]["distance_cm"]-500.0)<=1.0,
                "Successful public bounded physics-distance proof required")
        require(bounds["effects_after"]["local_pp"][site["post_process_path"]]["settings"]==old["settings_export_text"],
                "Final exposure settings are not the complete original EV3 settings")
        sites[sid]=site
    return sites


def validate_local_visibility(u, actors, labels, receipt, spec, inspector):
    repaired=bounded_exposure_history(receipt)
    require(receipt.get("success") is True and receipt.get("spec_sha256")==VISIBILITY_SHA
            and receipt.get("actor_count")==1408 and not receipt.get("actors_created"),"Successful exact visibility refinement required")
    require(receipt["global_postprocess_before"]==receipt["global_postprocess_after"],"Global post-process changed during refinement")
    actual_global=current_global_pp_state(u,actors)
    require(actual_global==receipt["global_postprocess_before"],"Current full global post-process differs")
    exposure=[s for s in actual_global.values() if s["label"]==spec["global_exposure_actor_label"]]
    require(len(exposure)==1 and exposure[0]["min"]==exposure[0]["max"]==11.5 and exposure[0]["override_min"]
            and exposure[0]["override_max"] and exposure[0]["unbound"] and exposure[0]["enabled"],"Original globalEV11.5 exposure contract differs")
    cdo=u.get_default_object(u.Rogue10mCharacter.static_class())
    camera={"native_defaults":inspector.camera_defaults(u),"camera_post_process":{}}
    for c in cdo.get_components_by_class(u.CameraComponent):
        camera["camera_post_process"][path(c)]={"settings_export_text":c.get_editor_property("post_process_settings").export_text(),
                                              "blend_weight":float(c.get_editor_property("post_process_blend_weight"))}
    require(normalized_addresses(camera)==normalized_addresses(receipt["camera_cdo_postprocess_before"])
            ==normalized_addresses(receipt["camera_cdo_postprocess_after"]),"Camera CDO post-process/defaults changed")
    expected_paths=set(receipt["created_component_paths"])
    require(len(expected_paths)==4 and len(receipt["postprocess_sites"])==2,"Exact four added local-exposure components required")
    rows=[];observed_paths=set()
    for site in spec["sites"]:
        actor=labels[site["actor_label"]]
        boxes=list(actor.get_components_by_class(u.BoxComponent));pps=list(actor.get_components_by_class(u.PostProcessComponent))
        require(len(boxes)==len(pps)==1,"One Box and PostProcessComponent per dressing actor required")
        box,pp=boxes[0],pps[0]
        require(box.get_name()==site["box_name"] and pp.get_name()==site["post_process_name"]
                and pp.get_attach_parent()==box and box.get_attach_parent()==actor.get_editor_property("root_component"),
                "Bounded post-process attachment differs")
        repair=repaired[site["site_id"]]
        channel_names=["ECC_WORLD_STATIC","ECC_WORLD_DYNAMIC","ECC_PAWN","ECC_VISIBILITY","ECC_CAMERA",
                       "ECC_PHYSICS_BODY","ECC_VEHICLE","ECC_DESTRUCTIBLE"]
        channel_names += ["ECC_ENGINE_TRACE_CHANNEL"+str(i) for i in range(1,7)]
        channel_names += ["ECC_GAME_TRACE_CHANNEL"+str(i) for i in range(1,51)]
        channels=[getattr(u.CollisionChannel,n) for n in channel_names if getattr(u.CollisionChannel,n,None) is not None]
        require(box.get_collision_enabled()==u.CollisionEnabled.QUERY_ONLY and str(box.get_collision_profile_name())=="Custom"
                and all(box.get_collision_response_to_channel(c)==u.CollisionResponseType.ECR_IGNORE for c in channels)
                and not box.get_editor_property("visible")
                and box.get_editor_property("hidden_in_game") and not box.get_editor_property("generate_overlap_events")
                and not box.get_editor_property("can_ever_affect_navigation") and not box.is_component_tick_enabled()
                and not pp.is_component_tick_enabled(),"Exposure bounds require a hidden query shape ignoring all channels, no overlaps/navigation/tick")
        origin,extent,radius=u.SystemLibrary.get_component_bounds(box)
        require(near(xyz(origin),site["box_center_cm"],.01) and near(xyz(extent),site["box_extent_cm"],.01)
                and abs(xyz(origin)[2]-xyz(extent)[2]-8400)<.01 and abs(xyz(origin)[2]+xyz(extent)[2]-9200)<.01,
                "Actual exposure Box world bounds differ")
        settings=pp.get_editor_property("settings");text=settings.export_text()
        overrides=sorted(name for name,value in re.findall(r"(bOverride_\w+)=(True|False)",text) if value=="True")
        require(overrides==site["override_names"] and settings.get_editor_property("override_auto_exposure_min_brightness")
                and settings.get_editor_property("override_auto_exposure_max_brightness")
                and settings.get_editor_property("auto_exposure_min_brightness")==settings.get_editor_property("auto_exposure_max_brightness")==3.0,
                "Exactly the two local exposure overrides atEV3 are required")
        require(not pp.get_editor_property("unbound") and pp.get_editor_property("enabled") and pp.get_editor_property("visible")
                and not pp.get_editor_property("hidden_in_game") and pp.get_editor_property("priority")==100.0
                and pp.get_editor_property("blend_radius")==200.0 and pp.get_editor_property("blend_weight")==1.0,
                "Local exposure blend/enable/bounded settings differ")
        expected=[s for s in receipt["postprocess_sites"] if s["site_id"]==site["site_id"]]
        require(len(expected)==1 and expected[0]["actor_path"]==path(actor) and expected[0]["box_path"]==path(box)
                and expected[0]["post_process_path"]==path(pp) and expected[0]["parent_box_path"]==path(box)
                and expected[0]["settings_export_text"]==text,"Current local exposure differs from successful authoring receipt")
        require(xyz(origin)[2]+xyz(extent)[2]+pp.get_editor_property("blend_radius")<site["outside_camera_test_cm"][2],
                "Local exposure would affect the declared exterior camera height")
        probes=[]
        for point,wanted in ((repair["box_center_cm"],0.0),(repair["outside_query_cm"],500.0)):
            result=box.get_closest_point_on_collision(u.Vector(*point))
            require(isinstance(result,tuple) and len(result)==2 and isinstance(result[0],(int,float))
                    and isinstance(result[1],u.Vector) and abs(float(result[0])-wanted)<=.01,
                    "Current public inside/outside collision-distance proof failed")
            probes.append({"query_cm":point,"distance_cm":float(result[0]),"closest_point_cm":xyz(result[1])})
        observed_paths.update((path(box),path(pp)))
        rows.append({"site":site["site_id"],"box":path(box),"post_process":path(pp),"world_center_cm":xyz(origin),
            "world_extent_cm":xyz(extent),"min_max_ev100":3.0,"highest_blend_z_cm":9400.0,"outside_camera_z_cm":9880.0,
            "collision":"QueryOnly","all_channels_ignore":True,"public_shape_queries":probes})
    require(observed_paths==expected_paths,"Local exposure component inventory differs")
    require(u.SystemLibrary.get_console_variable_int_value("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange")==1,
            "Required EV100 project exposure range differs")
    return rows


def validate_caves(require_saved_clean=False):
    report = {"schema": "Rogue10m.CaveValidation.20261118.v1", "passed": False, "checks": {},
        "diagnostics": [], "traceback": None, "assets_saved": False, "actor_or_property_mutations": False,
        "require_saved_clean": bool(require_saved_clean), "runtime_camera_certified": False,
        "physical_input": False, "full_gameplay_certified": False, "utc": datetime.now(timezone.utc).isoformat()}
    checks = report["checks"]

    def check(name, value, detail=None):
        checks[name] = bool(value)
        if not value:
            report["diagnostics"].append({"check": name, "detail": detail})

    try:
        import unreal as u
        inspector = module("cave_qa_inspector", "Scripts/Editor/InspectCaveSites20261118.py")
        reader = module("cave_qa_material_reader", "Scripts/Editor/NaturalRoadMaterial20261117.py")
        old_reader = module("cave_qa_old_actor_reader", "Scripts/Editor/AuthorNaturalBossThreshold20261118.py")
        api = module("cave_qa_component_reader", "Scripts/Editor/NaturalWorldDressing20261117.py")
        api._guard(u)
        before_dirty = api._dirty(u)
        report["dirty_packages_before"] = before_dirty
        dirty_digest = digest(before_dirty)
        require(not require_saved_clean or not before_dirty, "Saved-clean validation requires no dirty packages")
        require(hashlib.sha256((BASE/"plan.json").read_bytes()).hexdigest() == PLAN_SHA,
                "Frozen structural plan changed")
        require(hashlib.sha256((BASE/"dressing-plan.json").read_bytes()).hexdigest() == DRESSING_SHA,
                "Frozen dressing plan changed")
        plan, dressing = read("plan.json"), read("dressing-plan.json")
        v1_target, baseline_native = read("target-height-plan.json"), read("baseline-live/baseline-native.json")
        require(hashlib.sha256((BASE/"collar-spec.json").read_bytes()).hexdigest()==COLLAR_SHA
                and hashlib.sha256((BASE/"collar-preflight.json").read_bytes()).hexdigest()==COLLAR_PREFLIGHT_SHA,
                "Frozen collar recipe/preflight changed")
        collar,collar_preflight=read("collar-spec.json"),read("collar-preflight.json")
        require(collar["schema"]=="Rogue10m.CaveFloorCollarSpec.20261118.v1" and collar["map"]==TARGET
                and collar["structural_plan_sha256"]==PLAN_SHA and collar["floor_collar_cm"]==150.0
                and collar["old_outer_blend_envelope_cm"]==300.0 and collar["remaining_blend_cm"]==150.0
                and collar_preflight.get("passed") is True and collar_preflight["recipe_sha256"]==COLLAR_SHA
                and hashlib.sha256((BASE/"target-height-plan-collar.json").read_bytes()).hexdigest()==collar["target_height_plan_file_sha256"],
                "Exact150cm collar target contract differs")
        target=read("target-height-plan-collar.json")
        base_actors, base_materials = read("baseline-live/baseline-actors.json"), read("baseline-live/baseline-materials.json")
        material_receipt, applied = read("author-live/materials.json"), read("author-live/apply-height-collar.json")
        require(hashlib.sha256((BASE/"visibility-spec.json").read_bytes()).hexdigest()==VISIBILITY_SHA,
                "Frozen visibility specification changed")
        visibility_spec,visibility=read("visibility-spec.json"),read("author-live/visibility.json")
        require(visibility_spec.get("schema")=="Rogue10m.CaveVisibilitySpec.20261118.v1" and visibility_spec.get("map")==TARGET
                and visibility_spec["core_plan_sha256"]==PLAN_SHA and visibility_spec["dressing_plan_sha256"]==DRESSING_SHA
                and hashlib.sha256((BASE/"author-live/materials.json").read_bytes()).hexdigest()==visibility_spec["original_material_receipt_sha256"],
                "Visibility specification provenance differs")
        require(material_receipt.get("success") is True and applied.get("success") is True,
                "Successful current-feature material/height receipts required")
        require(applied["previous_composite_blake3"]==collar["source_v1_native_blake3"]
                =="b9f28e82d7661e83555d5f72942ea999ca6aea9554969753a889ea50f87e1970"
                and applied["new_layer_guid"]==collar["new_layer_guid"] and applied["changed_samples"]==447
                and applied.get("original_layer_preserved") is True,
                "Latest native collar receipt does not preserve its exact v1 predecessor")
        require(hashlib.sha256((BASE/"target-height-plan.json").read_bytes()).hexdigest() == plan["target_height_plan_sha256"],
                "Frozen target height artifact changed")
        require(hashlib.sha256(struct.pack("<"+"H"*len(v1_target["target_heights"]),*v1_target["target_heights"])).hexdigest()
                ==collar["source_v1_grid_uint16le_sha256"]=="ef9d5119c138e680001442fac76499e2e0ff40e9f51e428a33bd834aa3b4ffc1",
                "Preserved v1 source grid changed")
        check("exact150cm_collar_target_and_preserved_v1_predecessor",True)
        native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(True))
        require(native.get("success") and native.get("read_only") and native.get("dirty_package_names_unchanged"),
                "Native read-only inspection failed")
        check("registered45_collision45_two_layers_settled", native.get("component_count") == native.get("collision_count") == 45
              and native.get("layer_count") == 2 and native.get("landscape_reports_up_to_date") is True)
        check("original_raw_paint_layer_and_landscape_guid_preserved", native.get("original_layer_blake3") == ORIGINAL_LAYER
              == baseline_native["original_layer_blake3"] and native.get("landscape_guid") == LANDSCAPE_GUID
              and native.get("source_layer_guid") == baseline_native["source_layer_guid"])
        grid = native.get("composite_heights_uint16", [])
        old = baseline_native["composite_heights_uint16"]
        require(len(grid) == len(old) == 179488 and native["width"] == 568 and native["height"] == 316, "Full568x316 grid required")
        check("all179488_composite_samples_equal_reviewed_target", grid == target["target_heights"]
              and native["composite_blake3"] == applied["target_composite_blake3"])
        report["composite_uint16le_sha256"] = hashlib.sha256(struct.pack("<"+"H"*len(grid), *grid)).hexdigest()
        require(report["composite_uint16le_sha256"]==collar["target_uint16le_sha256"],"Actual collar grid signature differs")
        layout = json.loads((ROOT/"Docs/References/WorldMap/world-layout-20261112.json").read_text(encoding="utf-8-sig"))
        nodes = {n["id"]: (15000+100*n["positionM"]["x"], 32550+100*n["positionM"]["y"]) for n in layout["nodes"]}
        road_lines = [(nodes[e["from"]], nodes[e["to"]]) for e in layout["edges"]]
        village = [(1000,42550,3800), (29000,42550,3800), (22000,55550,3700)]
        changed = [i for i, (a, b) in enumerate(zip(old, grid)) if a != b]
        outside, roads, hubs, nonflat = [], [], [], []
        for i in changed:
            x = -90000+(315+i%568)*(10000/56)
            y = -90000+(630+i//568)*(10000/56)
            inside = False
            for site in plan["sites"]:
                r = site["ramp"]
                ramp_rect = [r["x_cm"]-r["width_cm"]/2, min(r["top_y_cm"], r["bottom_y_cm"]),
                             r["x_cm"]+r["width_cm"]/2, max(r["top_y_cm"], r["bottom_y_cm"])]
                inside |= inside_expanded_rect(x,y,site["pit_rect_cm"],300) or inside_expanded_rect(x,y,ramp_rect,300)
            if not inside: outside.append(i)
            if min(point_segment_distance((x,y),a,b) for a,b in road_lines) < 1900: roads.append(i)
            if any(math.hypot(x-cx,y-cy) <= radius for cx,cy,radius in village): hubs.append(i)
            if old[i] != 32768+4800: nonflat.append(i)
        check("only_two_declared_local_excavation_masks_changed", bool(changed) and not outside, {"changed":len(changed),"outside":outside[:20]})
        check("all29_road19m_envelopes_and_three_flat_villages_preserved", not roads and not hubs,
              {"road_violations":roads[:20],"village_violations":hubs[:20]})
        check("ridge_and_nonflat_source_samples_unchanged", not nonflat, nonflat[:20])
        check("outside_declared_cut_envelopes_equal_source", not outside, outside[:20])
        report["changed_height_samples"] = len(changed)
        actors = list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())
        actual_by_path = {path(a): a for a in actors}
        old_paths = set(base_actors["actors"])
        extras = [a for a in actors if path(a) not in old_paths]
        labels = {a.get_actor_label(): a for a in extras}
        expected_labels = {"CV18_"+s+"_"+role for s in ("Q","Z") for role in ("Structure","Dressing")}
        require(len(actors) == 1408 and len(actual_by_path) == 1408 and old_paths <= set(actual_by_path)
                and len(extras) == 4 and set(labels) == expected_labels, "Exact original1404 plus four unique cave actors required")
        check("exact1408_actors_four_declared_additions", True)
        old_actors = [actual_by_path[p] for p in sorted(old_paths)]
        observed_old = old_reader._snapshot_actors(u, old_actors, api)
        observed_components = inspector.persistent_components(u, old_actors)
        check("all_original1404_actor_snapshots_preserved", normalized_addresses(observed_old) == normalized_addresses(base_actors["actors"]))
        check("all_original_persistent_components_preserved", normalized_addresses(observed_components) == normalized_addresses(base_actors["persistent_components"]))
        check("new_cave_actors_always_loaded_visible_no_tick", all(not a.get_editor_property("is_spatially_loaded")
              and not a.get_editor_property("hidden") and not a.is_actor_tick_enabled() for a in extras))
        report["local_visibility"]=validate_local_visibility(u,actors,labels,visibility,visibility_spec,inspector)
        check("two_bounded_EV3_query_shapes_ignore_all_channels_with_public_distance_proof",len(report["local_visibility"])==2)
        check("full_global_EV11_5_and_camera_defaults_preserved",True)
        material_roles = material_receipt["material_paths_by_role"]
        historical_new=refined_material_expectations(u,material_receipt["created_material_states"],visibility,visibility_spec)
        historical_wall_bindings=wall_binding_history(material_roles,visibility)
        expected_new,final_wall_bindings=triplanar_expectations(u,historical_new,visibility,historical_wall_bindings)
        check("surface_triplanar43_nodes_original13_and_visibility_controls_preserved",True)
        check("exact_two_wall_ceiling_binding_overlays_follow_frozen_recipe_chain",True)
        dressing,relief_changes=camera_relief_expectations(dressing)
        relief_seen=set()
        structure_instances = dressing_instances = light_count = 0
        actual_rows = []
        for site in plan["sites"]:
            actor = labels["CV18_"+site["site_id"]+"_Structure"]
            bins = OrderedDict()
            for row in site["components"]: bins.setdefault(row["material_role"], []).append(row)
            components = list(actor.get_components_by_class(u.InstancedStaticMeshComponent))
            require(len(components) == len(bins) == 3, "Three structure ISM material bins per site required")
            remaining = list(components)
            for bin_index,(role, rows) in enumerate(bins.items()):
                name="CV18_"+site["site_id"]+"_ISM_"+str(bin_index).zfill(2)
                matches = [c for c in remaining if c.get_name()==name]
                require(len(matches) == 1, "One exact named structure material bin required: "+role)
                c = matches[0]; remaining.remove(c)
                expected_material=material_roles[role]
                if role=="stone_wall":
                    binding=final_wall_bindings[site["site_id"]]
                    require(path(actor)==binding["actor_path"] and path(c)==binding["component_path"]
                            and c.get_instance_count()==binding["instance_count"],"Final wall overlay targets a different bin")
                    expected_material=binding["new_material"]
                require(path(c.get_material(0)).split(".")[0]==expected_material,"Structure material binding differs: "+role)
                require(path(c.get_editor_property("static_mesh")).split(".")[0] == "/Engine/BasicShapes/Cube"
                        and c.get_instance_count() == len(rows), "Structure cube/count differs")
                require(c.get_collision_enabled() == u.CollisionEnabled.QUERY_AND_PHYSICS
                        and str(c.get_collision_profile_name()) == "BlockAll"
                        and all(c.get_collision_response_to_channel(ch) == u.CollisionResponseType.ECR_BLOCK for ch in
                                (u.CollisionChannel.ECC_PAWN,u.CollisionChannel.ECC_CAMERA,u.CollisionChannel.ECC_VISIBILITY))
                        and not c.is_component_tick_enabled() and c.get_editor_property("cast_shadow"), "Structure collision/tick/shadow differs")
                for i, row in enumerate(rows):
                    transform = instance_transform(u, c, i)
                    p, y, r = row["rotation_pitch_yaw_roll_deg"]
                    expected_q = u.Rotator(pitch=p,yaw=y,roll=r).quaternion()
                    q = [getattr(transform.rotation,a) for a in "xyzw"]
                    wanted = [getattr(expected_q,a) for a in "xyzw"]
                    require(near(xyz(transform.translation),row["location_cm"]) and near(xyz(transform.scale3d),[v/100 for v in row["dimensions_cm"]],1e-5)
                            and (near(q,wanted,1e-5) or near(q,[-v for v in wanted],1e-5)), "Structure instance transform differs: "+row["label"])
                    structure_instances += 1
                actual_rows.append({"site":site["site_id"],"kind":"structure","role":role,"path":path(c),"instances":len(rows)})
        for site in dressing["sites"]:
            actor = labels[site["actor_label"]]
            bins = OrderedDict()
            for row in site["instances"]: bins.setdefault((row["mesh"],row["material_role"]),[]).append(row)
            components = list(actor.get_components_by_class(u.InstancedStaticMeshComponent))
            require(len(components) == len(bins) == 4, "Four dressing ISM bins per site required")
            remaining = list(components)
            for (mesh_key,role), rows in bins.items():
                mesh_path = dressing["source_meshes"][mesh_key]["asset"] if "asset" in dressing["source_meshes"][mesh_key] else dressing["source_meshes"][mesh_key]["path"]
                matches = [c for c in remaining if path(c.get_editor_property("static_mesh")).split(".")[0] == mesh_path.split(".")[0]
                           and path(c.get_material(0)).split(".")[0] == material_roles[role]]
                require(len(matches) == 1, "One exact dressing mesh/material bin required")
                c = matches[0]; remaining.remove(c)
                require(c.get_instance_count() == len(rows) and c.get_collision_enabled() == u.CollisionEnabled.QUERY_ONLY
                        and c.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN) == u.CollisionResponseType.ECR_IGNORE
                        and all(c.get_collision_response_to_channel(ch) == u.CollisionResponseType.ECR_BLOCK for ch in
                            (u.CollisionChannel.ECC_CAMERA,u.CollisionChannel.ECC_VISIBILITY))
                        and not c.is_component_tick_enabled() and not c.get_editor_property("can_ever_affect_navigation"), "Dressing collision differs")
                mesh = c.get_editor_property("static_mesh"); box = mesh.get_bounding_box()
                size = [b-a for a,b in zip(xyz(box.min),xyz(box.max))]
                local_center = u.Vector(*[(a+b)/2 for a,b in zip(xyz(box.min),xyz(box.max))])
                for i,row in enumerate(rows):
                    t = instance_transform(u,c,i)
                    relief_key=(path(c),i)
                    if relief_key in relief_changes:
                        relief=relief_changes[relief_key];desired=relief["after_world_transform"]
                        require(site["site_id"]=="Q" and path(actor)==relief["actor_path"] and row["label"]==relief["label"]
                                and near(xyz(t.translation),desired["p"],.01) and near(xyz(t.scale3d),desired["s"],1e-6)
                                and near([getattr(t.rotation,a) for a in "xyzw"],desired["q"],1e-6),
                                "Current diagnosed camera-relief matrix differs")
                        relief_seen.add(relief_key)
                    fn = getattr(t,"transform_location",None)
                    center = fn(local_center) if callable(fn) else u.MathLibrary.transform_location(t,local_center)
                    require(near(xyz(center),row["bounds_center_cm"]) and near([a*b for a,b in zip(size,xyz(t.scale3d))],row["dimensions_cm"]),
                            "Dressing center/scale differs: "+row["label"])
                    p,y,r=row["rotation_pitch_yaw_roll_deg"]; wanted=u.Rotator(pitch=p,yaw=y,roll=r).quaternion()
                    q=[getattr(t.rotation,a) for a in "xyzw"]; w=[getattr(wanted,a) for a in "xyzw"]
                    require(near(q,w,1e-5) or near(q,[-v for v in w],1e-5),"Dressing rotation differs: "+row["label"])
                    dressing_instances += 1
                actual_rows.append({"site":site["site_id"],"kind":"dressing","role":role,"path":path(c),"instances":len(rows)})
            lights = list(actor.get_components_by_class(u.PointLightComponent))
            require(len(lights) == len(site["lights"]) == 6,"Six lights per site required")
            for i,expected in enumerate(site["lights"]):
                matches=[c for c in lights if c.get_name()==site["site_id"]+"_CavePointLight_"+str(i).zfill(2)]
                require(len(matches)==1,"One exact point light component required")
                c=matches[0]; color=c.get_editor_property("light_color")
                require(near(xyz(c.get_editor_property("relative_location")),expected["location_cm"])
                        and abs(c.get_editor_property("intensity")-expected["intensity_lumens"])<.01
                        and abs(c.get_editor_property("attenuation_radius")-expected["attenuation_radius_cm"])<.01
                        and [int(getattr(color,k)) for k in "rgb"]==expected["color_srgb"]
                        and c.get_editor_property("intensity_units")==u.LightUnits.LUMENS
                        and c.get_editor_property("mobility")==u.ComponentMobility.MOVABLE
                        and c.get_editor_property("affects_world") and not c.get_editor_property("cast_shadows")
                        and c.get_editor_property("visible") and not c.get_editor_property("hidden_in_game")
                        and not c.is_component_tick_enabled(),"Point light differs: "+expected["label"])
                light_count += 1
        check("all286_structure_instances_transform_and_PawnCameraVisibility_collision",structure_instances==286)
        check("all426_dressing_instances_bounds_rotation_and_camera_only_collision",dressing_instances==426==dressing["instance_count"])
        check("exact_two_Q_camera_relief_X50_matrices_and_other424_dressing_preserved",relief_seen==set(relief_changes) and len(relief_seen)==2)
        check("all12_lights_exact_visible_settings",light_count==12==dressing["light_count"])
        report["components"] = actual_rows
        source_failures=[]
        for key,expected in base_materials["materials"].items():
            obj=u.load_asset(expected["path"])
            actual=inspector.material_state(u,obj,reader)
            if actual != expected: source_failures.append(key)
        check("all40_source_material_graphs_parameters_files_unchanged",len(base_materials["materials"])==40 and not source_failures,source_failures)
        checkpoint=read("baseline-files.json")
        changed_own=[]
        for row in checkpoint["owned_backed_up_files"]:
            p=ROOT/row["path"]
            if not p.is_file() or p.stat().st_size!=row["size"] or hashlib.sha256(p.read_bytes()).hexdigest().lower()!=row["sha256"].lower():changed_own.append(row["path"])
        check("all_eight_preexisting_owned_asset_files_unchanged",len(checkpoint["owned_backed_up_files"])==8 and not changed_own,changed_own)
        require({p.split(".")[0] for p in expected_new}=={CAVE_OWN+"/"+n for n in NEW_MATERIALS},"Exact seven new material states required")
        material_failures=[]; new_states={}
        for key,expected in expected_new.items():
            obj=u.load_asset(key);actual=material_live_state(u,obj,reader);new_states[key]=actual
            if actual!=expected:material_failures.append(key)
        check("all_seven_new_material_graphs_parameters_parentage_match_authored_state",not material_failures,material_failures)
        surface=new_states[CAVE_OWN+"/M_CaveSurface.M_CaveSurface"]
        source_ground=base_materials["materials"]["/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_GroundDetail"]
        check("surface_preserves_original_ground_graph_and_exact_latest_triplanar_state",visibility["surface_graph_before"]==source_ground["graph"]
              and surface["graph"]==expected_new[CAVE_OWN+"/M_CaveSurface.M_CaveSurface"]["graph"] and surface["used_with_instanced_static_meshes"])
        lamp=new_states[CAVE_OWN+"/M_CaveLamp.M_CaveLamp"]
        classes=[v["class"] for v in lamp["graph"]["nodes"].values()]
        check("lamp_emission_graph_four_nodes",len(classes)==4 and classes.count("MaterialExpressionVectorParameter")==1
              and classes.count("MaterialExpressionScalarParameter")==2 and classes.count("MaterialExpressionMultiply")==1
              and lamp["graph"]["outputs"][str(u.MaterialProperty.MP_EMISSIVE_COLOR.value)]["node"] is not None)
        cases, coverage=build_probe_cases(plan)
        report["capsule_coverage"]=coverage; report["capsule_results"]=[]; failures=[]
        site_by_id={s["site_id"]:s for s in plan["sites"]}
        for case in cases:
            result=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.probe_pawn_capsule(
                u.Vector(*case["start"]),u.Vector(*case["end"]),case["radius"],96.0))
            native_dirty=result.pop("dirty_packages",None)
            require(isinstance(native_dirty,list) and len(native_dirty)==len(before_dirty) and set(native_dirty)==set(before_dirty)
                    and result.get("success") and result.get("read_only") and result.get("dirty_package_names_unchanged"),
                    "Native read-only sweep failed; remaining cases not run: "+case["id"])
            result["dirty_package_count"]=len(native_dirty);result["dirty_package_list_sha256"]=dirty_digest
            hit=result.get("hit") or {};passed=result.get("blocking_hit") is case["expected_blocking"]
            if case["expected_blocking"]:
                passed=passed and hit.get("actor")==path(labels[case["actor_label"]]) and not hit.get("start_penetrating",True)
                if case["kind"] in {"floor_support","ramp_support","surface_roof","ceiling"}:
                    impact=hit.get("impact_point_cm",[]); normal=hit.get("impact_normal",[0,0,0])
                    expected_z=case.get("impact_z")
                    if case["kind"]=="ramp_support" and len(impact)==3:expected_z=ramp_ground(site_by_id[case["site"]],impact[0],impact[1])
                    passed=passed and len(impact)==3 and expected_z is not None and abs(impact[2]-expected_z)<=3.0
                    passed=passed and ((normal[2]<=-.71) if case["kind"]=="ceiling" else (normal[2]>=.71))
            report["capsule_results"].append({"case":case,"query":result,"passed":bool(passed)})
            if not passed:failures.append(case["id"])
        check("all_unique_route_edges_both_directions_two_radii_three_lanes_clear",not [r for r in report["capsule_results"] if r["case"]["kind"]=="route_clear" and not r["passed"]])
        check("all112_floor_and10_ramp_supports_exact",sum(r["case"]["kind"]=="floor_support" for r in report["capsule_results"])==112
              and sum(r["case"]["kind"]=="ramp_support" for r in report["capsule_results"])==10
              and not [r for r in report["capsule_results"] if r["case"]["kind"] in {"floor_support","ramp_support"} and not r["passed"]])
        check("all112_ceilings_and10_surface_roofs_exact",not [r for r in report["capsule_results"] if r["case"]["kind"] in {"ceiling","surface_roof"} and not r["passed"]])
        check("all20_wall_crossings_hit_exact_structure",sum(r["case"]["kind"]=="wall" for r in report["capsule_results"])==20
              and not [r for r in report["capsule_results"] if r["case"]["kind"]=="wall" and not r["passed"]])
        check("all_planned_capsule_queries_completed_without_failures",len(report["capsule_results"])==len(cases) and not failures,failures)
        report["capsule_query_count"]=len(cases)
        after_dirty=api._dirty(u)
        check("dirty_package_identities_unchanged",set(after_dirty)==set(before_dirty) and len(after_dirty)==len(before_dirty))
        if require_saved_clean:check("saved_clean_after_independent_reload",not after_dirty)
        report["dirty_packages_after"]=after_dirty
        report["passed"]=all(checks.values()) and not report["diagnostics"]
        report["limitations"]=["Finite simple ECC_Pawn sweeps and explicit Camera collision settings; actual camera motion/rendering are separately required.",
            "No physical-keyboard full run, whole-project scan, native/custom/unloaded reference certification, navigation or Shipping acceptance.",
            "Only the address portion of repr(<Struct ...>) is normalized for old-actor cross-process comparison; its fields are retained."]
    except Exception:
        report["traceback"]=traceback.format_exc()
        report["diagnostics"].append({"check":"validation_exception","detail":report["traceback"]})
        if "api" in locals() and "before_dirty" in locals():
            after_dirty=api._dirty(u)
            report["dirty_packages_after"]=after_dirty
            check("dirty_package_identities_unchanged",set(after_dirty)==set(before_dirty) and len(after_dirty)==len(before_dirty))
    return report
