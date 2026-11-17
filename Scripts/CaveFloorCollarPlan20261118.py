"""Pure150cm cave-floor collar revision; no Unreal/process/asset operations.

Preserves the original cave/geometry plans and v1 target. Only the pit fade is
changed: floor through150cm outside the pit, then smoothstep to the unchanged
300cm outer envelope. Ramp formulas, geometry, routes and probe criteria stay
unchanged. The native payload requires a fresh clean full-grid live snapshot.
"""
from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/"Feature/doc/evidence/caves20261118"
PLAN_SHA="39b0150690e02a6edb453f91d0362603d9063dc3771dce9977d870fe184e6a74"
V1_FILE_SHA="96abbc526da80ba3637341261a57892ade0078b8fc08df55fd3bbe3a14b3a898"
V1_GRID_SHA="ef9d5119c138e680001442fac76499e2e0ff40e9f51e428a33bd834aa3b4ffc1"
V1_NATIVE="b9f28e82d7661e83555d5f72942ea999ca6aea9554969753a889ea50f87e1970"
ORIGINAL_LAYER="7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955"
V1_APPLY_SHA="c4d5b5024ff1389c2bc8879730f546f639111445c9bd3304163176a7f0b0c93f"
COLLAR=150.0
OUTER=300.0


def require(ok,message):
    if not ok:raise ValueError(message)


def sha(raw):return hashlib.sha256(raw).hexdigest()


def serialized(value):
    return (json.dumps(value,sort_keys=True,ensure_ascii=False,separators=(",",":"),allow_nan=False)+"\n").encode()


def read(name,pin=None):
    raw=(BASE/name).read_bytes()
    if pin:require(sha(raw)==pin,"Immutable input changed: "+name)
    return json.loads(raw)


def base_module():
    spec=importlib.util.spec_from_file_location("original_cave_layout_for_collar",ROOT/"Scripts/CaveLayoutPlan20261118.py")
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m


def collar_fade(distance):
    if distance<=COLLAR:return 1.0
    if distance>=OUTER:return 0.0
    t=(OUTER-distance)/(OUTER-COLLAR)
    return t*t*(3.0-2.0*t)


def quad(values,x,y,m):
    gx=(x-m.LANDSCAPE_ORIGIN[0])/m.SPACING-m.BOUNDS[0]
    gy=(y-m.LANDSCAPE_ORIGIN[1])/m.SPACING-m.BOUNDS[1]
    ix,iy=math.floor(gx),math.floor(gy)
    require(0<=ix<567 and 0<=iy<315,"Interpolation quad outside retained grid")
    ids=[iy*568+ix,iy*568+ix+1,(iy+1)*568+ix,(iy+1)*568+ix+1]
    return {"indices":ids,"heights_uint16":[values[i] for i in ids],"heights_cm":[2*(values[i]-32768) for i in ids]}


def generate():
    m=base_module();plan=read("plan.json",PLAN_SHA);v1=read("target-height-plan.json",V1_FILE_SHA)
    receipt=read("author-live/apply-height-confirmed.json",V1_APPLY_SHA)
    require(receipt.get("success") and receipt["target_composite_blake3"]==V1_NATIVE and
            receipt["expected_original_layer_blake3"]==ORIGINAL_LAYER,"Exact successful v1 native predecessor required")
    require(sha(m.packed_heights(v1["target_heights"]))==V1_GRID_SHA,"V1 whole179488 grid changed")
    old_cave_height=m.cave_height_at

    def revised_height(x,y,sites):
        result=m.SURFACE_Z
        for site in sites:
            weight=collar_fade(m.rect_distance(x,y,site["pit_rect_cm"]))
            result=min(result,m.SURFACE_Z-(m.SURFACE_Z-m.FLOOR_Z)*weight)
            ramp=site["ramp"];y0,y1=sorted((ramp["top_y_cm"],ramp["bottom_y_cm"]))
            rect=(ramp["x_cm"]-ramp["width_cm"]/2,y0,ramp["x_cm"]+ramp["width_cm"]/2,y1)
            ramp_weight=m.fade(m.rect_distance(x,y,rect))
            t=max(0.,min(1.,(y-ramp["top_y_cm"])/(ramp["bottom_y_cm"]-ramp["top_y_cm"])))
            recess=max(m.FLOOR_Z,m.SURFACE_Z-1000.*t-40.)
            result=min(result,m.SURFACE_Z-(m.SURFACE_Z-recess)*ramp_weight)
        return result

    # Reuse the original pure builder's full protection checks, without
    # changing its source or retaining a replacement geometry plan.
    m.cave_height_at=revised_height
    try:_,native,source_preflight=m.build_plan()
    finally:m.cave_height_at=old_cave_height
    values=native["target_heights"];old=v1["target_heights"]
    changed=[i for i,(a,b) in enumerate(zip(old,values)) if a!=b]
    require(changed and all(values[i]<=old[i] for i in changed),"Collar must only lower the pit's existing outer fade")
    for i in changed:
        x=m.LANDSCAPE_ORIGIN[0]+(m.BOUNDS[0]+i%568)*m.SPACING
        y=m.LANDSCAPE_ORIGIN[1]+(m.BOUNDS[1]+i//568)*m.SPACING
        require(any(0<m.rect_distance(x,y,s["pit_rect_cm"])<OUTER for s in plan["sites"]),
                "New change escaped the old pit300cm outer envelope")
        for site in plan["sites"]:
            ramp=site["ramp"];low,high=sorted((ramp["top_y_cm"],ramp["bottom_y_cm"]))
            require(not (abs(x-ramp["x_cm"])<=ramp["width_cm"]/2 and low<=y<=high),
                    "A ramp-footprint grid sample changed during the collar-only revision")
    faces=[]
    for site in plan["sites"]:
        cells=set(map(tuple,site["floor_cells"]))
        for row in site["components"]:
            if "_Wall_" not in row["label"]:continue
            x,y,_=row["location_cm"]
            normal=(0,1) if row["dimensions_cm"][0]>row["dimensions_cm"][1] else (1,0)
            for sign in (-1,1):
                px,py=x+normal[0]*250*sign,y+normal[1]*250*sign
                cell=(math.floor((px-site["origin_cm"][0])/500),math.floor((py-site["origin_cm"][1])/500))
                if cell not in cells:continue
                face=(x+normal[0]*40*sign,y+normal[1]*40*sign)
                sample=quad(values,*face,m)
                require(all(v==m.encoded_height(8600) for v in sample["heights_uint16"]),
                        "A planned wall midface's supporting quad is not entirely floor-height: "+row["label"])
                faces.append({"site":site["site_id"],"wall":row["label"],"face_cm":list(face),"quad":sample})
    diagnosed_point=(58950.747802734375,51900.0)
    diagnosed_before=quad(old,*diagnosed_point,m);diagnosed_after=quad(values,*diagnosed_point,m)
    require(max(diagnosed_before["heights_cm"])>8600 and diagnosed_after["heights_cm"]==[8600]*4,
            "The diagnosed Z wall bleed was not removed at the four underlying vertices")
    native.update(profile_revision="20261118-QZ-floor-collar150cm",
        coordinate_contract="Only pit fade changes: flat8600cm through150cm outside each pit, remaining150cm smoothstep inside original300cm envelope. Ramp unchanged.")
    target_bytes=serialized(native)
    recipe={"schema":"Rogue10m.CaveFloorCollarSpec.20261118.v1","map":m.MAP,
        "structural_plan_sha256":PLAN_SHA,"source_v1_target_file_sha256":V1_FILE_SHA,
        "source_v1_grid_uint16le_sha256":V1_GRID_SHA,"source_v1_native_blake3":V1_NATIVE,
        "source_v1_apply_receipt_sha256":V1_APPLY_SHA,"original_layer_blake3":ORIGINAL_LAYER,
        "new_layer_guid":receipt["new_layer_guid"],"floor_z_cm":8600.,"floor_collar_cm":COLLAR,
        "old_outer_blend_envelope_cm":OUTER,"remaining_blend_cm":OUTER-COLLAR,
        "curve":"d<=150:1;150<d<300:smoothstep((300-d)/150);d>=300:0; original ramp formula unchanged",
        "target_height_plan":"target-height-plan-collar.json","target_height_plan_file_sha256":sha(target_bytes),
        "target_uint16le_sha256":sha(m.packed_heights(values)),"target_changed_from_v1_samples":len(changed),
        "geometry_actor_material_camera_changes":0,"probe_cases_and_expectations_changed":False,
        "diagnosed_case":"Z_wall_09","diagnosis":"A grid vertex outside the pit's original smoothstep raised the triangulated heightfield ahead of the physical wall face.",
        "axis_aligned_midface_math":{"spacing_cm":m.SPACING,"wall_half_thickness_cm":40.,
            "maximum_outer_neighbor_distance_cm":m.SPACING-40.,"collar_margin_cm":COLLAR-(m.SPACING-40.)},
        "runtime_and_native_capsule_acceptance_pending":True}
    recipe_bytes=serialized(recipe)
    preflight={**source_preflight,"schema":"Rogue10m.CaveFloorCollarPreflight.20261118.v1",
        "structural_plan_sha256":PLAN_SHA,"recipe_sha256":sha(recipe_bytes),
        "source_v1_file_sha256":V1_FILE_SHA,"source_v1_uint16le_sha256":V1_GRID_SHA,
        "source_v1_native_blake3":V1_NATIVE,"target_height_plan_file_sha256":sha(target_bytes),
        "target_changed_from_v1_samples":len(changed),"all_new_changes_lower_only":True,
        "all_new_changes_inside_old_pit300cm_envelope":True,
        "all_original_pit_interior_and_ramp_heights_unchanged":all(values[i]==old[i] for i in range(len(old))
            if not any(0<m.rect_distance(m.LANDSCAPE_ORIGIN[0]+(m.BOUNDS[0]+i%568)*m.SPACING,
                m.LANDSCAPE_ORIGIN[1]+(m.BOUNDS[1]+i//568)*m.SPACING,s["pit_rect_cm"])<OUTER for s in plan["sites"])),
        "verified_wall_midface_count":len(faces),"all_wall_midface_quads_flat8600":True,
        "diagnosed_case":{"id":"Z_wall_09","query_unchanged":True,"old_hit_point_cm":[*diagnosed_point,8636.2607421875],
            "underlying_quad_before":diagnosed_before,"underlying_quad_after":diagnosed_after},
        "limitations":["Pure grid/protection proof only. Native2016 probes, actual camera traversal and cold reload must be rerun.",
                        "The138.571cm spacing bound is an axis-aligned midface statement, not a universal continuous-collision theorem."]}
    require(preflight["all_original_pit_interior_and_ramp_heights_unchanged"],"A change escaped the permitted outside-pit collar domain")
    return native,recipe,preflight


def guarded_collar_revision(live_native,*,expected_recipe_sha256,expected_preflight_sha256):
    recipe=read("collar-spec.json",expected_recipe_sha256)
    preflight=read("collar-preflight.json",expected_preflight_sha256)
    target=read("target-height-plan-collar.json",recipe["target_height_plan_file_sha256"])
    old=read("target-height-plan.json",V1_FILE_SHA)
    require(preflight.get("passed") is True and preflight["recipe_sha256"]==expected_recipe_sha256
            and preflight["target_height_plan_file_sha256"]==recipe["target_height_plan_file_sha256"],"Frozen collar preflight linkage differs")
    require(live_native.get("success") and live_native.get("read_only") and live_native.get("landscape_reports_up_to_date")
            and live_native.get("component_count")==live_native.get("collision_count")==45 and live_native.get("layer_count")==2
            and live_native.get("composite_blake3")==V1_NATIVE and live_native.get("original_layer_blake3")==ORIGINAL_LAYER
            and live_native.get("composite_heights_uint16")==old["target_heights"] and live_native.get("dirty_packages")==[],
            "Fresh clean full45/two-layer v1 native snapshot required")
    require(recipe["source_v1_native_blake3"]==V1_NATIVE and recipe["source_v1_grid_uint16le_sha256"]==V1_GRID_SHA
            and recipe["floor_collar_cm"]==150 and recipe["old_outer_blend_envelope_cm"]==300,"Collar contract changed")
    payload=copy.deepcopy(target)
    payload.update(expected_composite_blake3=live_native["composite_blake3"],
        expected_original_layer_blake3=live_native["original_layer_blake3"],expected_new_layer_guid=recipe["new_layer_guid"])
    return payload


def main():
    files=[BASE/name for name in ("target-height-plan-collar.json","collar-spec.json","collar-preflight.json")]
    require(not any(p.exists() for p in files),"Preserve existing collar planning outputs; no overwrite")
    values=generate()
    for p,value in zip(files,values):
        with p.open("xb") as stream:stream.write(serialized(value))
    target,recipe,preflight=values
    print(json.dumps({"passed":True,"target_grid_sha256":recipe["target_uint16le_sha256"],
        "target_file_sha256":sha(files[0].read_bytes()),"recipe_sha256":sha(files[1].read_bytes()),
        "preflight_sha256":sha(files[2].read_bytes()),"changed_from_v1":recipe["target_changed_from_v1_samples"],
        "changed_from_rounded":preflight["changed_samples"],"wall_midfaces_verified":preflight["verified_wall_midface_count"]},indent=2))


if __name__=="__main__":main()
