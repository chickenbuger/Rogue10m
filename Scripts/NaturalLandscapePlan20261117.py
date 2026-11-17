"""Pure target heightfield: a flat playable basin and an irregular outer ridge."""
import json
import math
from pathlib import Path

TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
ANCHOR = (15000.0, 32550.0)
PLATEAU_Z = 9600.0
POLYGON_LOCAL_M = [(-455,-65),(-455,270),(-430,350),(-250,428),(-70,438),
    (75,438),(260,425),(455,380),(485,320),(485,30),(370,-65),(120,-75),(-200,-75)]
POLYGON = [(ANCHOR[0]+x*100,ANCHOR[1]+y*100) for x,y in POLYGON_LOCAL_M]
BOUNDS = (315,630,882,945)
ORIGIN = (-90000.0,-90000.0)
SPACING = 180000.0/1008.0


def smooth(t):
    t=max(0.0,min(1.0,t))
    return t*t*(3-2*t)


def signed_distance(x,y):
    inside=False
    nearest=float("inf")
    for a,b in zip(POLYGON,POLYGON[1:]+POLYGON[:1]):
        dx,dy=b[0]-a[0],b[1]-a[1]
        t=max(0.0,min(1.0,((x-a[0])*dx+(y-a[1])*dy)/(dx*dx+dy*dy)))
        nearest=min(nearest,math.hypot(x-a[0]-t*dx,y-a[1]-t*dy))
        if (a[1]>y)!=(b[1]>y) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:
            inside=not inside
    return -nearest if inside else nearest


def height_at(x,y):
    d=signed_distance(x,y)
    # Irregularity affects only the perimeter; all combat/settlement pads are
    # on the exact same plane. No extra incline is added to the central route.
    jitter=150*math.sin(x/1900+y/2700)+100*math.sin(y/1500-x/3800)
    s=d+jitter
    h=2600+450*math.sin(x/3300+y/5100)+250*math.cos(y/2600-x/4500)
    inner=smooth((s+1500)/1300)
    outer=1-smooth((s-650)/1550)
    return PLATEAU_Z+h*inner*outer


def encode(z):
    return round(32768+z/2)


def build_height_plan():
    heights=[encode(height_at(ORIGIN[0]+ix*SPACING,ORIGIN[1]+iy*SPACING))
             for iy in range(BOUNDS[1],BOUNDS[3]+1) for ix in range(BOUNDS[0],BOUNDS[2]+1)]
    return {"schema":"Rogue10m.NaturalBoundaryHeightPlan.20261117.v1","map":TARGET,
            "landscape_guid":"147B73D247543E06FDA1A7AF9D467458",
            "source_layer_guid":"9C4488BD4196BCC81051B5A001047577",
            "new_layer_name":"NaturalBoundary20261117","vertex_bounds":list(BOUNDS),
            "width":568,"height":316,"plateau_z_cm":PLATEAU_Z,"target_heights":heights,
            "polygon_local_m":POLYGON_LOCAL_M,"polygon_world_cm":POLYGON,
            "coordinate_contract":"Unchanged XY anchor and 45-component lattice; Z flattening is explicitly new user scope."}


if __name__=="__main__":
    root=Path(__file__).resolve().parents[1]
    out=root/"Feature/doc/evidence/landscape-natural-boundary20261117/target-height-plan.json"
    out.parent.mkdir(parents=True,exist_ok=True)
    data=build_height_plan()
    with out.open("x",encoding="utf-8") as stream:json.dump(data,stream,separators=(",",":"))
    print(json.dumps({"samples":len(data["target_heights"]),"min_height_cm":2*(min(data["target_heights"])-32768),
                      "max_height_cm":2*(max(data["target_heights"])-32768),"output":str(out)}))
