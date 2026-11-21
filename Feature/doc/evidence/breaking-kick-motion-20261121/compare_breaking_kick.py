"""Compare the real game stock and Blender kick without comparing editor-only pose proxies."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import re


def summarize(run, output_rows):
    trace = run / "breaking-kick-poses.csv"
    with trace.open(encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.DictReader(handle))
    contacts = {int(row["frame"]) for row in rows if float(row["damage"]) > 0.01}
    assert len(contacts) == 1, contacts
    contact = contacts.pop()
    damage_rows = [row for row in rows if int(row["frame"]) == contact]
    damage = damage_rows[0]
    log = (run / "engine.log").read_text(encoding="utf-8-sig", errors="replace")
    elapsed = float(re.search(r"WEAPON3P DAMAGE .*?pulse=1 elapsed=([0-9.]+) ", log)[1])
    source = re.search(r"BREAKING_KICK_CONTACT .*?clip=([^\r\n]+)", log)[1]
    sample_offsets = {-10, -7, -4, -2, 0, 2, 4, 8, 12, 18, 24, 35, 56}

    def location(row):
        return [float(row[axis]) for axis in "xyz"]

    summary = {
        "label": run.name, "first_damage_frame": contact, "first_damage_montage_s": float(damage["montage_s"]),
        "first_damage_elapsed_s": elapsed, "first_damage_source": source,
        "first_damage_amount": float(damage["damage"]), "signed_toe_to_target_gap_cm": float(damage_rows[0]["toe_capsule_gap_cm"]),
        "actor_delta_during_attack_cm": max(math.dist(tuple(float(row["actor_"+a]) for a in "xyz"),
                      tuple(float(damage_rows[0]["actor_"+a]) for a in "xyz")) for row in rows
                      if contact - 10 <= int(row["frame"]) <= contact + 56),
        "pose_csv_sha256": hashlib.sha256(trace.read_bytes()).hexdigest(),
        "engine_log_sha256": hashlib.sha256((run / "engine.log").read_bytes()).hexdigest(),
    }
    key_rows = [row for row in rows if int(row["frame"]) - contact in sample_offsets]
    output_rows.extend([{"run": run.name, "frame_from_damage": int(row["frame"]) - contact, **row} for row in key_rows])
    for body in ("manny", "display"):
        measured = {}
        for bone in ("head", "hand_l", "hand_r", "foot_l", "ball_l", "foot_r", "ball_r"):
            samples = {int(row["frame"]): location(row) for row in rows
                       if row["mesh"] == body and row["bone"] == bone and contact - 10 <= int(row["frame"]) <= contact + 56}
            start, strike, end = samples[contact - 10], samples[contact], samples[contact + 56]
            trajectory = list(samples.values())
            measured[bone] = {"start_cm": start, "contact_cm": strike, "recovery_cm": end,
                              "start_to_contact_cm": math.dist(start, strike),
                              "first_contact_window_max_cm": max(math.dist(start, samples[frame]) for frame in range(contact - 10, contact + 13)),
                              "peak_step_cm": max(math.dist(a,b) for a,b in zip(trajectory, trajectory[1:]))}
        summary[body] = measured
    summary["contact_result"] = re.findall(r"BREAKING_KICK_CONTACT[^\r\n]+", log)[0]
    assert "RESULT=BREAKING_KICK_PASSED completed=1 failures=0" in log
    return summary


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("worktree", type=Path)
    parser.add_argument("stock")
    parser.add_argument("candidate")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    runs = args.worktree / "tmp/MeleeVfxQARuns"
    rows = []
    stock, candidate = [summarize(runs / name, rows) for name in (args.stock, args.candidate)]
    assert stock["first_damage_frame"] == candidate["first_damage_frame"]
    # Separate fresh profile trials can resolve different damage rolls.
    for name in ("first_damage_montage_s", "first_damage_elapsed_s"):
        assert math.isclose(stock[name], candidate[name], abs_tol=1.e-6), (name, stock[name], candidate[name])
    assert candidate["actor_delta_during_attack_cm"] == stock["actor_delta_during_attack_cm"] == 0.0
    if args.out:
        with (args.out / "comparison.json").open("x", encoding="utf-8") as handle:
            json.dump({"input": "Real Q and inventoried Knuckle, programmatic 60Hz in StartHub", "stock": stock, "candidate": candidate}, handle, indent=2)
        with (args.out / "pose-comparison.csv").open("x", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)
    for run in (stock, candidate):
        print(run["label"], run["contact_result"], "timing/amount", run["first_damage_elapsed_s"], run["first_damage_amount"])
        for bone in ("ball_l", "ball_r", "hand_r"):
            print(bone, run["display"][bone])


if __name__ == "__main__":
    main()
