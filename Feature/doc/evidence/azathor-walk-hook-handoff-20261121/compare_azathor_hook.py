import argparse
import csv
import json
from pathlib import Path


SPECIES = "AbyssOverlordAzathor"
CLIP = "A_HumanBoss_Hook_Raw"
STEP = "peak_bone_step_cm"
TURN = "peak_bone_angle_deg"


def samples(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        return {
            (row["source_pose"], row["phase"], int(row["frame"])): row
            for row in csv.DictReader(stream)
            if row["species"] == SPECIES
        }


def attack_result(rows, stance):
    attack = [row for (s, phase, _), row in rows.items() if s == stance and phase == "attack"]
    contact = next(row for row in attack if int(row["damage_events"]) == 1)
    recovered = next(row for (s, phase, _), row in rows.items() if s == stance and phase == "attack_exit")
    if any(row["asset"] != CLIP and float(row["position_s"]) >= 0 for row in attack):
        raise AssertionError(f"{stance} did not retain the active hook")

    def maximum(field, start, end):
        selected = max(attack[start - 1:end], key=lambda row: float(row[field]))
        return {"frame": int(selected["frame"]), "value": float(selected[field])}

    return {
        "damage_frame": int(contact["frame"]),
        "damage_montage_s": float(contact["position_s"]),
        "recovery_frame": int(recovered["frame"]),
        "first": {field: float(attack[0][field]) for field in (STEP, TURN)},
        "first_six": {field: maximum(field, 1, 6) for field in (STEP, TURN)},
        "first_twenty": {field: maximum(field, 1, 20) for field in (STEP, TURN)},
        "remaining": {field: maximum(field, 21, len(attack)) for field in (STEP, TURN)},
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("stock", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--gate", action="store_true", help="Require a bounded improvement and an unchanged standing case.")
    args = parser.parse_args()
    stock, candidate = (samples(path) for path in (args.stock, args.candidate))
    report = {
        label: {stance: attack_result(rows, stance) for stance in ("idle", "walk")}
        for label, rows in (("stock", stock), ("candidate", candidate))
    }
    idle_difference = {
        field: max(
            abs(float(stock[key][field]) - float(candidate[key][field]))
            for key in stock
            if key[0] == "idle" and key[1] in ("attack_call", "attack", "attack_exit")
        )
        for field in ("position_s", STEP, TURN)
    }
    report["idle_max_difference"] = idle_difference
    paired_walk_clock = max(
        abs(float(stock[key]["position_s"]) - float(candidate[key]["position_s"]))
        for key in stock
        if key[0] == "walk" and key[1] in ("attack_call", "attack", "attack_exit")
    )
    report["walk_position_s_max_difference"] = paired_walk_clock
    late_walk_difference = {
        field: max(
            abs(float(stock[key][field]) - float(candidate[key][field]))
            for key in stock
            if key[0] == "walk" and key[1] == "attack" and key[2] > 20
        )
        for field in (STEP, TURN)
    }
    report["walk_after_first_twenty_max_difference"] = late_walk_difference
    print(json.dumps(report, indent=2))
    if args.gate:
        old, new = (report[label]["walk"] for label in ("stock", "candidate"))
        for field in (STEP, TURN):
            if new["first"][field] > 0.65 * old["first"][field]:
                raise AssertionError(f"Walking first frame failed the continuity bound for {field}")
            if new["first_twenty"][field]["value"] > 0.80 * old["first_twenty"][field]["value"]:
                raise AssertionError(f"An early {field} spike was not sufficiently reduced")
        if new["first"][STEP] > 10.0:
            raise AssertionError("The first walking toe step exceeded 10 cm")
        if max(idle_difference.values()) > 0.001 or paired_walk_clock > 0.0001:
            raise AssertionError("The standing attack or montage clock drifted")
        # The visible cloth bones retain a small secondary response to the different entry.
        if late_walk_difference[STEP] > 0.5 or late_walk_difference[TURN] > 2.0:
            raise AssertionError("The settled walking attack exceeded the secondary motion band")
        for poses in (report["stock"], report["candidate"]):
            for stance in ("idle", "walk"):
                if (poses[stance]["damage_frame"], poses[stance]["recovery_frame"]) != (60, 143):
                    raise AssertionError(f"{stance} hit and recovery timing changed")


if __name__ == "__main__":
    main()
