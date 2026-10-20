"""Read-only independent geometry and paired-pose audit of WalkPalm fixture CSVs.

No Unreal calls, asset edits, animation resets or selecting a sign from a current pose.
Run with --before DIR --after DIR --appearance DIR (repeat) --output FILE.
Exact same-frame errors remain in the report. Optional normalized BlendSpace phase
alignment is reported separately and never used to hide a failed exact comparison.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
from collections import defaultdict
from pathlib import Path


def read_csv(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def v(row, keys):
    return tuple(float(row[k]) for k in keys)


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def mul(a, b):
    return tuple(x * y for x, y in zip(a, b))


def scale(a, s):
    return tuple(x * s for x in a)


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def length(a):
    return math.sqrt(dot(a, a))


def norm(a):
    size = length(a)
    if not math.isfinite(size) or size < 1e-8:
        raise ValueError("Degenerate or nonfinite vector/quaternion")
    return scale(a, 1.0 / size)


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def qmul(a, b):
    av, bv = a[:3], b[:3]
    return (*add(add(scale(bv, a[3]), scale(av, b[3])), cross(av, bv)), a[3]*b[3]-dot(av, bv))


def rotate(q, p):
    q = norm(q)
    return add(p, add(scale(cross(q[:3], p), 2*q[3]), scale(cross(q[:3], cross(q[:3], p)), 2)))


def angle(a, b):
    return math.degrees(math.acos(max(-1.0, min(1.0, dot(norm(a), norm(b))))))


def qangle(a, b):
    # atan2 is stable near zero; acos(dot) exaggerates floating point roundoff.
    a, b = norm(a), norm(b)
    d = qmul(a, (-b[0], -b[1], -b[2], b[3]))
    return math.degrees(2*math.atan2(length(d[:3]), abs(d[3])))


TRS = ("tx", "ty", "tz", "qx", "qy", "qz", "qw", "sx", "sy", "sz")


def transform(row, prefix):
    return v(row, [prefix + "_" + k for k in TRS])


def compose(local, parent):
    t = add(parent[:3], rotate(parent[3:7], mul(parent[7:10], local[:3])))
    return (*t, *qmul(parent[3:7], local[3:7]), *mul(parent[7:10], local[7:10]))


def load(directory):
    directory = Path(directory).resolve()
    frames = {int(r["frame"]): r for r in read_csv(directory / "frames.csv")}
    palms = read_csv(directory / "palms.csv")
    players = read_csv(directory / "blendspaces.csv")
    signs = {(r["stage"], r["side"]): float(r["fixed_sign"]) for r in read_csv(directory / "palm-signs.csv")}
    poses = defaultdict(dict)
    with (directory / "poses.csv").open(encoding="utf-8-sig", newline="") as stream:
        for r in csv.DictReader(stream):
            poses[(int(r["frame"]), r["stage"])][r["bone"]] = (r["parent"], transform(r, "local"), transform(r, "raw_world"))
    metrics = {}
    for line in (directory / "metrics.txt").read_text(encoding="utf-8-sig").splitlines():
        if "=" in line:
            k, value = line.split("=", 1)
            metrics[k] = value
    return dict(directory=directory, frames=frames, palms=palms, players=players, signs=signs, poses=poses, metrics=metrics)


def pose_world(bones, name, cache):
    if name in cache:
        return cache[name]
    parent, local, recorded = bones[name]
    # The recorded root world transform supplies the actual component transform.
    # Every descendant is independently recomposed from its captured LOCAL TRS.
    world = compose(local, pose_world(bones, parent, cache)) if parent else recorded
    cache[name] = world
    return world


def mean(rows, key):
    return sum(float(r[key]) for r in rows) / len(rows)


def summary(run):
    failures = []
    def require(good, description):
        if not good:
            failures.append(description)
    frames, poses, signs = run["frames"], run["poses"], run["signs"]
    require(sorted(frames) == list(range(651)), "frame inventory is not exactly 0..650")
    require(signs.get(("source", "left")) == -1 and signs.get(("source", "right")) == 1, "independent Manny L-/R+ signs changed")
    maximum = dict(normal_vector=0., actor_projection=0., palm_landmark_vs_pose_cm=0., local_parent_recompose_cm=0., local_parent_recompose_degrees=0.)
    groups = defaultdict(list)
    for r in run["palms"]:
        frame, stage, side = int(r["frame"]), r["stage"], r["side"]
        require(float(r["sign"]) == signs[(stage, side)], f"current-pose sign changed: {frame}/{stage}/{side}")
        W, M, I, P = [v(r, [prefix + k for k in "xyz"]) for prefix in "wmip"]
        N = scale(norm(cross(sub(M, W), sub(I, P))), signs[(stage, side)])
        recorded = v(r, ["nx", "ny", "nz"])
        maximum["normal_vector"] = max(maximum["normal_vector"], length(sub(N, recorded)))
        Q = v(frames[frame], ["actor_q" + k for k in "xyzw"])
        F, R, U = [dot(N, rotate(Q, axis)) for axis in ((1,0,0), (0,1,0), (0,0,1))]
        actual = (F, R, U, R*(-1 if side == "left" else 1))
        stored = v(r, ["normal_forward", "normal_right", "normal_up", "normal_outward"])
        maximum["actor_projection"] = max(maximum["actor_projection"], max(abs(a-b) for a,b in zip(actual,stored)))
        bones = poses[(frame, stage)]
        cache = {}
        suffix = "l" if side == "left" else "r"
        for name, point in zip(("hand", "middle_01", "index_01", "pinky_01"), (W, M, I, P)):
            bone = name + "_" + suffix
            calculated = pose_world(bones, bone, cache)
            stored_world = bones[bone][2]
            maximum["palm_landmark_vs_pose_cm"] = max(maximum["palm_landmark_vs_pose_cm"], length(sub(point, stored_world[:3])))
            maximum["local_parent_recompose_cm"] = max(maximum["local_parent_recompose_cm"], length(sub(calculated[:3], stored_world[:3])))
            maximum["local_parent_recompose_degrees"] = max(maximum["local_parent_recompose_degrees"], qangle(calculated[3:7], stored_world[3:7]))
        if int(r["stable"]):
            groups[(r["phase"], stage, side)].append(r)
    require(maximum["normal_vector"] < 1e-6 and maximum["actor_projection"] < 1e-6, "world landmarks or actor quaternion disagree with recorded signed normals")
    require(maximum["palm_landmark_vs_pose_cm"] < 1e-5 and maximum["local_parent_recompose_cm"] < .001 and maximum["local_parent_recompose_degrees"] < .001, "independent parent/local recomposition disagrees with actual world hands")
    phases = {}
    for (phase, stage, side), rows in groups.items():
        result = {"samples": len(rows)}
        for k in ("normal_forward", "normal_right", "normal_up", "normal_outward"):
            result[k+"_mean"] = mean(rows,k)
            result[k+"_min"] = min(float(r[k]) for r in rows)
            result[k+"_max"] = max(float(r[k]) for r in rows)
        phases.setdefault(phase, {}).setdefault(stage, {})[side] = result
    movement = {}
    for phase, target in (("walk110",110), ("walk210",210), ("run600",600), ("yaw90walk210",210)):
        selected_frames = sorted({int(r["frame"]) for r in run["palms"] if r["phase"]==phase and int(r["stable"])})
        rows = [frames[f] for f in selected_frames]
        active = [p for p in run["players"] if int(p["frame"]) in selected_frames and float(p["node_weight"])>.5]
        movement[phase] = dict(samples=len(rows), speed_min=min(float(r["speed_cm_s"]) for r in rows), speed_max=max(float(r["speed_cm_s"]) for r in rows), actual_distance_cm=sum(float(r["actual_step_cm"]) for r in rows), grounded=all(int(r["grounded"]) for r in rows), active_blendspaces=sorted({r["blendspace"] for r in active}), input_y_min=min(float(r["input_y"]) for r in active), input_y_max=max(float(r["input_y"]) for r in active), input_x_abs_max=max(abs(float(r["input_x"])) for r in active), active_node_weight_min=min(float(r["node_weight"]) for r in active))
        require(movement[phase]["grounded"] and movement[phase]["actual_distance_cm"]>20 and abs(movement[phase]["speed_min"]-target)<2 and abs(movement[phase]["speed_max"]-target)<2, f"not real grounded {phase} motion")
        require(abs(movement[phase]["input_y_min"]-target)<2 and abs(movement[phase]["input_y_max"]-target)<2, f"active BS does not receive actual {phase} speed")
    restore = [f for f,r in frames.items() if int(r["view_fixture"])]
    require(restore in ([], [91,181,186,191,251,361,417]), "inspection cameras not restored on the next expected frame")
    require(all(abs(float(r["eye_z"])+3)<1e-8 and abs(float(r["control_pitch"])+20)<1e-8 for r in frames.values()), "production EyeOffset/pitch changed")
    require(max(float(r["camera_error_cm"]) for r in frames.values())<.05, "native independent head-camera oracle error exceeds .05cm")
    require(int(run["metrics"]["failures"])==0, "native fixture reported failures")
    log_path=run["directory"].parents[2]/"Feature/doc/evidence/walking-palm-reference-20261009"/(run["directory"].name+".log")
    require(log_path.is_file(),"native final log missing")
    native_log={"path":str(log_path)}
    if log_path.is_file():
        matches=re.findall(r"RESULT=WALK_PALM_(PASSED|FAILED) frames=(\d+) assertions=(\d+) failures=(\d+)",log_path.read_text(encoding="utf-8-sig",errors="replace"))
        require(bool(matches),"native final result marker missing")
        if matches:
            status,last_frame,assertions,failed=matches[-1]
            native_log.update(status=status,terminal_frame_index=int(last_frame),assertions=int(assertions),failures=int(failed),sha256=sha(log_path),count_note="metrics.txt snapshots counters before the final metrics-file write check; final log has that additional assertion. Frame index650 means651 observations, including frame0.")
            require(status=="PASSED" and int(failed)==0 and int(last_frame)==650,"native final result is not the completed passing650-frame test")
    return dict(label=run["directory"].name, path=str(run["directory"]), files={name:sha(run["directory"]/name) for name in ("frames.csv","palms.csv","poses.csv","blendspaces.csv","palm-signs.csv","metrics.txt")}, frames=len(frames), geometry_independent_errors=maximum, fixed_signs={stage+"_"+side: sign for (stage,side),sign in signs.items()}, movement=movement, phases=phases, camera=dict(eye_z=-3, control_pitch=-20, max_native_independent_error_cm=max(float(r["camera_error_cm"]) for r in frames.values()), capture_restore_frames=restore, capture_restore_automatic_cache_omissions=len(restore)), native_metrics={k:value for k,value in run["metrics"].items() if k!="NOTE"}, native_final_log=native_log, failures=failures)


def error(local_a, local_b):
    return dict(translation_cm=length(sub(local_a[:3],local_b[:3])), rotation_degrees=qangle(local_a[3:7],local_b[3:7]), scale=max(abs(a-b) for a,b in zip(local_a[7:10],local_b[7:10])))


def protected(stage, name):
    return name in ("root","pelvis","head") or name.startswith(("spine","thigh","calf","foot","ball","ik_foot","ik_root"))


def category(stage, name):
    if protected(stage,name):
        return "protected_lower_head"
    if name in ("hand_l","hand_r"):
        return "wrists"
    if name.startswith("ik_hand"):
        return "derived_hand_ik"
    if name.startswith(("middle","index","pinky","thumb")):
        return "finger_locals"
    return "other_upper_body"


def compare(before, after, reference):
    result = dict(same_frame={}, stable_gait_same_frame={}, normalized_phase_alignment={}, direction_changes={}, stable_same_frame_by_bone={}, wrist_reach_and_distal_axis={}, failures=[], review_notes=[])
    stable = {int(r["frame"]) for r in after["palms"] if int(r["stable"])}
    for mode in ("same_frame", "stable_gait_same_frame"):
        groups = defaultdict(lambda: dict(samples=0, translation_cm=0., rotation_degrees=0., scale=0.))
        for (frame,stage), bones in after["poses"].items():
            if mode=="stable_gait_same_frame" and frame not in stable:
                continue
            other = before["poses"].get((frame,stage),{})
            phase = after["frames"][frame]["phase"]
            for name, (_,local,_) in bones.items():
                if name not in other:
                    continue
                group = groups[(phase,stage,category(stage,name))]
                group["samples"]+=1
                e=error(other[name][1],local)
                for k in e:
                    group[k]=max(group[k],e[k])
        result[mode] = {"/".join(k):val for k,val in sorted(groups.items())}
    # Both local and world errors are retained. Auxiliary hand IK may move even
    # though the visible wrist stays fixed, so it must not be hidden in a roll metric.
    for (frame,stage), bones in after["poses"].items():
        if frame not in stable:
            continue
        old=before["poses"][(frame,stage)]
        for name in bones.keys() & old.keys():
            group=result["stable_same_frame_by_bone"].setdefault(stage+"/"+name,dict(category=category(stage,name),samples=0,local=dict(translation_cm=0.,rotation_degrees=0.,scale=0.),world=dict(translation_cm=0.,rotation_degrees=0.,scale=0.)))
            group["samples"]+=1
            for space,index in (("local",1),("world",2)):
                e=error(old[name][index],bones[name][index])
                for k in e:
                    group[space][k]=max(group[space][k],e[k])
    for name,group in result["stable_same_frame_by_bone"].items():
        if group["category"]=="protected_lower_head":
            if any(group[space]["translation_cm"]>.001 or group[space]["rotation_degrees"]>.001 for space in ("local","world")):
                result["failures"].append("Protected lower/head local or world pose differs: "+name)
    old_palms={(int(r["frame"]),r["stage"],r["side"]):r for r in before["palms"]}
    for r in after["palms"]:
        frame,stage,side=int(r["frame"]),r["stage"],r["side"]
        if frame not in stable:
            continue
        old=old_palms[(frame,stage,side)]
        group=result["wrist_reach_and_distal_axis"].setdefault(r["phase"]+"/"+stage+"/"+side,dict(samples=0,wrist_world_delta_cm_max=0.,distal_axis_angle_degrees_max=0.))
        group["samples"]+=1
        reach=length(sub(v(old,["wx","wy","wz"]),v(r,["wx","wy","wz"])))
        axis_angle=angle(v(old,["distal_x","distal_y","distal_z"]),v(r,["distal_x","distal_y","distal_z"]))
        group["wrist_world_delta_cm_max"]=max(group["wrist_world_delta_cm_max"],reach)
        group["distal_axis_angle_degrees_max"]=max(group["distal_axis_angle_degrees_max"],axis_angle)
    if any(g["wrist_world_delta_cm_max"]>.05 for g in result["wrist_reach_and_distal_axis"].values()):
        result["failures"].append("Visible source/body wrist world reach changes by more than .05cm")
    ik=[(name,g["world"]["translation_cm"]) for name,g in result["stable_same_frame_by_bone"].items() if g["category"]=="derived_hand_ik" and g["world"]["translation_cm"]>.05]
    if ik:
        result["review_notes"].append(dict(code="AUXILIARY_HAND_IK_WORLD_MOVEMENT",actual_world_translation_cm=dict(ik),meaning="Actual auxiliary hand IK movement is not visible wrist movement; do not claim all IK world positions are preserved. Verify consumers separately."))
        result["failures"].append("Auxiliary hand IK world positions changed by more than .05cm; restore original IK tracks")
    # Independent clock correspondence uses recorded active BS normalized time.
    # Error for an inexact nearest phase is diagnostic and does not prove zero change.
    def active(run):
        return {int(r["frame"]):r for r in run["players"] if float(r["node_weight"])>.99 and float(r["asset_length"])>0}
    old, new=active(before),active(after)
    for phase in ("walk110","walk210","run600","yaw90walk210"):
        candidates=[(f,float(r["asset_time"])/float(r["asset_length"])) for f,r in old.items() if before["frames"][f]["phase"]==phase]
        pairs=[]
        for f,r in new.items():
            if after["frames"][f]["phase"]!=phase:
                continue
            p=float(r["asset_time"])/float(r["asset_length"])
            g, gp=min(candidates,key=lambda item:(abs((p-item[1]+.5)%1-.5),abs(f-item[0])))
            pairs.append((f,g,abs((p-gp+.5)%1-.5)))
        aligned=dict(pairs=len(pairs), max_normalized_phase_error=max(p[2] for p in pairs), exact_phase_pairs=sum(p[2]<1e-7 for p in pairs), max_frame_shift=max(abs(p[0]-p[1]) for p in pairs), protected={})
        for stage in ("source","body"):
            worst=dict(translation_cm=0.,rotation_degrees=0.,scale=0.)
            for f,g,_ in pairs:
                a,b=before["poses"][(g,stage)],after["poses"][(f,stage)]
                for name in a.keys() & b.keys():
                    if protected(stage,name):
                        e=error(a[name][1],b[name][1])
                        for k in e:
                            worst[k]=max(worst[k],e[k])
            aligned["protected"][stage]=worst
        result["normalized_phase_alignment"][phase]=aligned
    for phase in after["summary"]["phases"]:
        result["direction_changes"][phase]={}
        for stage in ("source","body"):
            result["direction_changes"][phase][stage]={}
            for side in ("left","right"):
                b=before["summary"]["phases"][phase][stage][side]
                a=after["summary"]["phases"][phase][stage][side]
                target=reference[side.capitalize()]
                angular_before=angle([b["normal_"+axis+"_mean"] for axis in ("forward","right","up")],target)
                angular_after=angle([a["normal_"+axis+"_mean"] for axis in ("forward","right","up")],target)
                result["direction_changes"][phase][stage][side]={"mean_forward_before":b["normal_forward_mean"],"mean_forward_after":a["normal_forward_mean"],"mean_forward_delta":a["normal_forward_mean"]-b["normal_forward_mean"],"mean_outward_before":b["normal_outward_mean"],"mean_outward_after":a["normal_outward_mean"],"mean_outward_delta":a["normal_outward_mean"]-b["normal_outward_mean"],"reference_normal_angular_error_before_degrees":angular_before,"reference_normal_angular_error_after_degrees":angular_after,"reference_normal_angular_error_delta_degrees":angular_after-angular_before}
                if angular_after>=angular_before:
                    result["failures"].append("Mean 3D normal did not move closer to reference: "+phase+"/"+stage+"/"+side)
    camera_errors=[error(transform(before["frames"][f],"camera"),transform(after["frames"][f],"camera")) for f in stable]
    result["stable_same_frame_camera_world"]=dict(translation_cm=max(e["translation_cm"] for e in camera_errors),rotation_degrees=max(e["rotation_degrees"] for e in camera_errors))
    result["same_frame_actor_motion"]=dict(translation_cm_max=max(length(sub(transform(before["frames"][f],"actor")[:3],transform(after["frames"][f],"actor")[:3])) for f in after["frames"]),speed_difference_cm_s_max=max(abs(float(before["frames"][f]["speed_cm_s"])-float(after["frames"][f]["speed_cm_s"])) for f in after["frames"]))
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before",required=True,type=Path)
    parser.add_argument("--after",required=True,type=Path)
    parser.add_argument("--appearance",action="append",default=[],type=Path)
    parser.add_argument("--reference-policy",type=Path,default=Path(__file__).resolve().parents[1]/"Feature/doc/evidence/walking-palm-reference-20261009/reference/author-policy.json")
    parser.add_argument("--output",required=True,type=Path)
    args=parser.parse_args()
    before,after=load(args.before),load(args.after)
    before["summary"],after["summary"]=summary(before),summary(after)
    appearances=[summary(load(path)) for path in args.appearance]
    reference=json.loads(args.reference_policy.read_text(encoding="utf-8-sig"))["reference_target_actor"]
    comparison=compare(before,after,reference)
    failures=before["summary"]["failures"]+after["summary"]["failures"]+comparison["failures"]+[r["label"]+": "+f for r in appearances for f in r["failures"]]
    aux_repair=any(note["code"]=="AUXILIARY_HAND_IK_WORLD_MOVEMENT" for note in comparison["review_notes"])
    status="REQUIRES_AUX_IK_REPAIR" if aux_repair else "FAILED" if failures else "PASSED"
    report=dict(schema="rogue10m.walk-palm-runtime-review.v1", reviewer="independent offline geometry/CSV audit", status=status, status_scope="independent geometry, actual movement/camera, protected lower/head world/local, visible wrist reach, reference direction improvement and auxiliary hand IK world position", reference_policy=dict(path=str(args.reference_policy.resolve()),sha256=sha(args.reference_policy),target_actor_mean_normals=reference), before=before["summary"], after=after["summary"], comparison=comparison, appearances=appearances, failures=failures, limits=["The baseline is already inward; the requested correction is a backward wrist roll toward the FBX reference.","Highest cached BlendSpace sample has a private engine API. This audit verifies actual active node, weight, speed and input, and identifies jog selection at input(0,600) only by correspondence with the authored sample inventory.","Protected evaluated source/body bones are root/pelvis/head/spines/legs/feet/foot IK. Other upper bones, wrists/finger locals and derived hand IK are reported separately. Per-bone clip modification and idle asset immutability require the separate native frozen source-key audit.","Exact same-frame pose errors are retained even if initial idle phase differs between processes. Normalized BS phase alignment is an explicitly reported diagnostic, not a replacement for frozen asset isolation.","Capture cameras only change render view/visibility. Seven subsequent frames omit automatic first-person cache comparison during restoration; native independently composed head-camera comparison remains active.","The sixteen angular comparisons use normalized per-phase means against the FBX reference mean, not a claim that every individual animation frame or scalar forward projection changes identically."])
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(dict(status=report["status"],output=str(args.output.resolve()),failures=failures,appearances=len(appearances)),ensure_ascii=False))
    return 0 if not failures else 1


if __name__=="__main__":
    raise SystemExit(main())
