#!/usr/bin/env python3
"""Procedurally produce a sculpted example map from the editable JSON project.

Pure standard-library Python. This intentionally illustrates how developers may
generate large terrain and actor datasets externally, without using a proprietary
editor API or recompiling the game.
"""
import json
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
source = ROOT / "projects" / "default.rtsproject.json"
output = ROOT / "projects" / "terrain_showcase.rtsproject.json"

def generate():
    project = json.loads(source.read_text(encoding="utf-8"))
    project["name"] = "Terrain Workshop Showcase"
    project["description"] = "Sculpted rolling terrain, material painting, pathing masks and placed actor example"
    terrain = project["terrain"]
    cells = []
    for z in range(terrain["depth"]):
        for x in range(terrain["width"]):
            wx = terrain["min_x"] + x * terrain["spacing"]
            wz = terrain["min_z"] + z * terrain["spacing"]
            hill = math.exp(-((wx-16)**2 + (wz-21)**2)/(2*13**2))*7.4
            small = math.exp(-((wx+16)**2 + (wz+23)**2)/(2*8**2))*2
            valley = math.exp(-((wx+1)**2 + (wz-30)**2)/(2*8**2))*-1.8
            height = round(hill+small+valley,2)
            if abs(height)<0.06: height=0
            rock = math.hypot(wx-16,wz-21)
            sand = math.hypot(wx+18,wz+24)
            dirt = math.hypot(wx+12,wz-18)
            material = 2 if rock<13 else 3 if sand<12 else 1 if dirt<15 else 4 if hill>0.3 else 0
            blocked = rock<7 and wz>20 and wx>14
            if height or material or blocked:
                entry = {"x":x,"z":z}
                if height:entry["height"]=height
                if material:entry["material"]=material
                if blocked:entry["blocked"]=True
                cells.append(entry)
    terrain["cells"] = cells
    project["placements"].append({
        "id":"sculpture_1","kind":"Doodad","catalog":"zealot_actor",
        "name":"Ridge Monument","owner":-1,"position":[-7,0,23],
        "rotation":[0,36,0],"scale":[1.8,2.6,1.8],
        "amount":0,"overrides":{"editor_note":"User-authored presentation actor instance"}
    })
    project["placements"].append({
        "id":"waypoint_1","kind":"Point","catalog":"",
        "name":"Canyon Observation Point","owner":-1,"position":[13,0,18],
        "rotation":[0,0,0],"scale":[1,1,1],
        "amount":0,"overrides":{"target":"optional scripted trigger marker"}
    })
    output.write_text(json.dumps(project,indent=2)+"\n",encoding="utf-8")
    print(f"Created {output} ({len(cells)} sparse terrain cells)")

if __name__ == "__main__":
    generate()
