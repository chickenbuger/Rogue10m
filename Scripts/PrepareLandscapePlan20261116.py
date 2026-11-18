"""Prepare the map design supplement; never launches Unreal or edits assets."""
from __future__ import annotations

import hashlib
import importlib.util
import json
import math
from pathlib import Path
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "Docs/References/WorldMap"
EVIDENCE = ROOT / "Feature/doc/evidence/landscape-production-20261116"


def main():
    layout_path = REFERENCE / "world-layout-20261112.json"
    acquisition_path = REFERENCE / "acquisition-catalog-20261112.json"
    layout = json.loads(layout_path.read_text(encoding="utf-8-sig"))
    acquisition = json.loads(acquisition_path.read_text(encoding="utf-8-sig"))
    source = ROOT / "Scripts/WorldLayoutTerrain20261112.py"
    spec = importlib.util.spec_from_file_location("terrain_plan_baseline", source)
    terrain = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(terrain)
    bounds = {"minX": -487.5, "maxX": 525.0, "minY": -100.5, "maxY": 462.0}
    pads = {p["id"]: p for p in terrain._PADS}
    nodes = []
    for n in layout["nodes"]:
        p = n["positionM"]
        pad = pads.get(n["id"])
        wx, wy = terrain.local_to_world(p["x"], p["y"])
        nodes.append({"id": n["id"], "label": n["label"], "kind": n["kind"],
                      "localM": p, "worldCm": {"x": wx, "y": wy},
                      "baselinePadElevationM": pad["z"] / 100 if pad else None,
                      "baselinePadRadiusM": pad["radius_cm"] / 100 if pad else None})
    margins = {}
    for pad in pads.values():
        x, y = terrain.world_to_local(pad["x"], pad["y"])
        r = pad["radius_cm"] / 100
        margins[pad["id"]] = min(x-r-bounds["minX"], bounds["maxX"]-x-r,
                                      y-r-bounds["minY"], bounds["maxY"]-y-r)
    graph = {n["id"]: set() for n in nodes}
    by_id = {n["id"]: n for n in nodes}
    road_margins = []
    for e in layout["edges"]:
        graph[e["from"]].add(e["to"])
        graph[e["to"]].add(e["from"])
        for end in (e["from"], e["to"]):
            p = by_id[end]["localM"]
            road_margins.append(min(p["x"]-bounds["minX"], bounds["maxX"]-p["x"],
                                    p["y"]-bounds["minY"], bounds["maxY"]-p["y"])-19)
    reached, pending = set(), ["S"]
    while pending:
        n = pending.pop()
        if n not in reached:
            reached.add(n)
            pending.extend(graph[n]-reached)
    direct = [by_id[n]["localM"] for n in ("S", "J100", "J180", "J240", "B")]
    direct_m = sum(math.hypot(a["x"]-b["x"], a["y"]-b["y"]) for a,b in zip(direct,direct[1:]))
    checks = {"nodes21": len(nodes)==21, "roads29": len(layout["edges"])==29,
              "majorHubs3": sum(n["kind"]=="major_hub" for n in nodes)==3,
              "outposts5": sum(n["kind"]=="outpost" for n in nodes)==5,
              "allNodesConnected": len(reached)==len(nodes),
              "allRoadsBidirectionalAndUngated": all(e["bidirectional"] and not e["requiresQuest"] and not e["requiresSkill"] for e in layout["edges"]),
              "directPlanar300M": abs(direct_m-300)<1e-9,
              "allPadsWithinBounds": min(margins.values())>0,
              "allRoadShouldersWithinBounds": min(road_margins)>0,
              "books23": acquisition["counts"]["totalBooks"]==23}
    document = {"schemaVersion": 1, "documentId": "rogue10m-landscape-production-20261116",
        "status": "design_ready", "runtimeImplementationComplete": False,
        "implementationResult": "Feature/doc/2026-11-16_landscape-production-result.md",
        "sourceCatalogs": [{"path": str(p.relative_to(ROOT)).replace("\\", "/"),
                            "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in (layout_path, acquisition_path, source)],
        "sourceMapPreserved": "/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A",
        "implementationCandidate": "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A",
        "method": "native_Landscape_Manage_Delete_outside_components_after_live_baseline",
        "coordinateSystem": {"units": "m", "north": "+Y", "east": "+X", "origin": "S", "anchorWorldCm": [15000,32550,0]},
        "envelope": {"localBoundsM": bounds, "worldBoundsCm": [-33750,67500,22500,78750],
            "widthM": 1012.5, "depthM": 562.5, "componentQuads": 63, "subsectionsPerComponent": 1,
            "gridSpacingCm": 1250/7, "originalComponents": 256, "retainedComponents": 45,
            "outsideComponents": 211, "retainedComponentIndexX": [5,13], "retainedComponentIndexY": [10,14],
            "retainedInclusiveVertexRangeX": [315,882], "retainedInclusiveVertexRangeY": [630,945],
            "equivalentUniqueVertexGrid": [568,316], "landscapeOriginalLocationCm": [-90000,-90000,0], "zScale": 256},
        "road": {"walkWidthM": 8, "totalGradedWidthM": 14, "featherBeyondGradedHalfWidthM": 12,
                 "directPlanarLengthM": direct_m, "directionalSlopeTargetDegrees": 12,
                 "fullTriangleSlopeTargetDegrees": 12, "knownHistoricalFullTriangleMaximumDegrees": 15.095,
                 "knownHistoricalFullTriangleMaximumResolved": False},
        "finalArena": {"localCenterM": [0,354.5], "worldCenterCm": [15000,68000,9600],
                       "radiusM": 55, "northBoundaryMarginM": 52.5, "bossActorPlacementImplemented": False},
        "nodes": nodes, "edges": layout["edges"],
        "validationBoundary": "This generated catalog verifies design geometry and historical procedural inputs only; it does not report live execution status. See implementationResult for native crop, cold reload, collision failures and runtime verification."}
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    result = {"passed": all(checks.values()), "checks": checks, "padBoundaryMarginM": margins,
              "minimumRoadShoulderBoundaryMarginM": min(road_margins), "directPlanarLengthM": direct_m,
              "liveEditorInspectionPerformedByThisTool": False,
              "landscapeMutationPerformedByThisTool": False, "scope": document["validationBoundary"]}
    (EVIDENCE / "plan-geometry-check.json").write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    if result["passed"]:
        (REFERENCE / "landscape-production-20261116.json").write_text(json.dumps(document,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(result,ensure_ascii=False))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
