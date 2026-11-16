#!/usr/bin/env python3
"""Check packaged RTS builds and embed their exact source/run identity.

Runs before uploading any build artifact so missing executables, resources or
incorrect project layouts fail CI rather than producing empty downloads.
"""
from __future__ import annotations

import json
import pathlib
import subprocess
import sys


def main() -> int:
    if len(sys.argv) != 5:
        print("usage: verify_build_bundle.py DIR PLATFORM SHA RUN_ID", file=sys.stderr)
        return 2

    root, platform, sha, run_id = sys.argv[1:]
    root = pathlib.Path(root).resolve()
    windows = platform == "windows-x64"
    if platform not in {"windows-x64", "linux-x86_64"}:
        raise ValueError("Unknown build platform")
    ext = ".exe" if windows else ""
    mandatory = [
        f"RTSCameraTerrain{ext}",
        f"RTSProjectTool{ext}",
        "README.md",
        "START_HERE.txt",
        "LICENSE",
        "projects/default.rtsproject.json",
        "projects/terrain_showcase.rtsproject.json",
        "projects/effects_showcase.rtsproject.json",
        "projects/ai_defense_showcase.rtsproject.json",
        "projects/ability_workshop_showcase.rtsproject.json",
        "projects/behavior_workshop_showcase.rtsproject.json",
        "projects/ground_impact_showcase.rtsproject.json",
        "projects/projectile_forge_showcase.rtsproject.json",
        "projects/area_shapes_showcase.rtsproject.json",
        "projects/weapon_forge_showcase.rtsproject.json",
        "projects/actor_events_showcase.rtsproject.json",
        "projects/requirements_workshop_showcase.rtsproject.json",
        "projects/technology_forge_showcase.rtsproject.json",
        "projects/ai_research_showcase.rtsproject.json",
        "projects/interface_studio_showcase.rtsproject.json",
        "projects/ui_frame_studio_showcase.rtsproject.json",
        "projects/ui_trigger_showcase.rtsproject.json",
        "docs/AUTHORING.md",
        "docs/ACTOR_EVENT_AUTHORING.md",
        "docs/REQUIREMENT_AUTHORING.md",
        "docs/TECHNOLOGY_RESEARCH.md",
        "docs/TECH_SHOW_USE.md",
        "docs/TECH_TREE_MAP.md",
        "docs/AI_RESEARCH.md",
        "docs/CREATOR_WINDOWS_ACCESSIBILITY.md",
        "docs/INTERFACE_EDITOR.md",
        "docs/UI_FRAME_HIERARCHY.md",
        "docs/UI_TRIGGER_EVENTS.md",
        "docs/BUILD_ARTIFACTS.md",
        "docs/CREATOR_ARCHITECTURE.md",
        "docs/TRIGGER_EDITOR.md",
        "docs/RTS_ENGINE_AUDIT.md",
        "docs/REPLAY_COMMANDS.md",
        "docs/SC2_FEATURE_MATRIX.md",
        "licenses/PICOJSON_LICENSE",
        "licenses/IMGUI_LICENSE",
        "run-game.bat" if windows else "run-game.sh",
        "run-editor.bat" if windows else "run-editor.sh",
    ]

    if windows:
        mandatory.append("symbols/RTSCameraTerrain.pdb")
    # A runtime deliverable must not accidentally include a third-party SDK
    # just because GLFW was pulled in through CMake FetchContent.
    for unwanted in ("include", "lib"):
        if (root / unwanted).exists():
            raise RuntimeError(f"Development files leaked into runtime build: {unwanted}")

    for relative in mandatory:
        path = root / relative
        if not path.is_file() or path.stat().st_size == 0:
            raise RuntimeError(f"Missing or empty installed file: {relative}")

    expected_magic = b"MZ" if windows else b"\x7fELF"
    for exe in (f"RTSCameraTerrain{ext}", f"RTSProjectTool{ext}"):
        with (root / exe).open("rb") as handle:
            header = handle.read(4)
        if not header.startswith(expected_magic):
            raise RuntimeError(f"Not a native executable: {exe}: {header!r}")

    for name in ("default", "terrain_showcase", "effects_showcase"):
        p = root / "projects" / f"{name}.rtsproject.json"
        project = json.loads(p.read_text(encoding="utf-8"))
        if project.get("schema_version") != 1 or not project.get("catalog"):
            raise RuntimeError(f"Invalid bundled starter project: {p}")

    # Enforce parity against the binary, not just a JSON file that the
    # packaging script can write while accidentally shipping an older EXE.
    native = root / f"RTSCameraTerrain{ext}"
    feature_text = subprocess.check_output(
        [str(native), "--features"], text=True, timeout=20
    )
    features = dict(
        line.strip().split("=", 1)
        for line in feature_text.splitlines()
        if "=" in line
    )
    if features.get("RTS_BUILD_COMMIT") != sha:
        raise RuntimeError(
            f"Native executable commit mismatch: expected {sha}, "
            f"got {features.get('RTS_BUILD_COMMIT')!r}"
        )
    required = (
        "RTS_FEATURE_FOG_OF_WAR",
        "RTS_FEATURE_GAMEPLAY_HUD",
        "RTS_FEATURE_NATIVE_PAUSE_MENU",
        "RTS_FEATURE_CREATOR_GUI",
        "RTS_FEATURE_TERRAIN_EDITOR",
        "RTS_FEATURE_SCENE_EDITOR",
        "RTS_FEATURE_DATA_EDITOR",
        "RTS_FEATURE_TRIGGER_EDITOR",
        "RTS_FEATURE_VISUAL_ECA_EDITOR",
        "RTS_FEATURE_TRIGGER_TRACE_MONITOR",
        "RTS_FEATURE_DYNAMIC_NAV_REPLAN",
        "RTS_FEATURE_STABLE_SIM_ORDER",
        "RTS_FEATURE_COMMAND_STREAM",
        "RTS_FEATURE_REPLAY_VERIFY",
        "RTS_FEATURE_AI_COMMAND_PLANNER",
        "RTS_FEATURE_SCRIPT_GLOBALS",
        "RTS_FEATURE_SCRIPT_EXPRESSIONS",
        "RTS_FEATURE_SCRIPT_CONTROL_FLOW",
        "RTS_FEATURE_RECURSIVE_ECA_UI",
        "RTS_FEATURE_TIMED_EFFECTS",
        "RTS_FEATURE_UNIT_ABILITIES",
        "RTS_FEATURE_ABILITY_EDITOR",
        "RTS_FEATURE_STATUS_BEHAVIORS",
        "RTS_FEATURE_VISUAL_EFFECT_FLOW",
        "RTS_FEATURE_GROUND_AREA_ABILITIES",
        "RTS_FEATURE_DELAYED_AREA_IMPACTS",
        "RTS_FEATURE_AUTHORED_PROJECTILES",
        "RTS_FEATURE_PROJECTILE_INTERCEPTION",
        "RTS_FEATURE_AREA_SHAPES",
        "RTS_FEATURE_AUTHORED_WEAPONS",
        "RTS_FEATURE_ARMOR_PENETRATION",
        "RTS_FEATURE_WEAPON_PROJECTILES",
        "RTS_FEATURE_ACTOR_EVENT_CUES",
        "RTS_FEATURE_REQUIREMENT_GRAPHS",
        "RTS_FEATURE_UPGRADE_RESEARCH",
        "RTS_FEATURE_TECH_SHOW_USE",
        "RTS_FEATURE_TECH_DEPENDENCY_MAP",
        "RTS_FEATURE_AI_RESEARCH",
        "RTS_FEATURE_CREATOR_MODULE_WINDOWS",
        "RTS_FEATURE_CREATOR_DPI_SCALING",
        "RTS_FEATURE_GAMEPLAY_INTERFACE_EDITOR",
        "RTS_FEATURE_UI_FRAME_HIERARCHY",
        "RTS_FEATURE_UI_TRIGGER_EVENTS",
    )
    missing = [flag for flag in required if features.get(flag) != "1"]
    if missing:
        raise RuntimeError(f"Native executable is missing expected features: {missing}")

    info = {
        "project": "3D-RTS-Engine-Test",
        "variant": "Release",
        "platform": platform,
        "commit": sha,
        "github_actions_run_id": str(run_id),
        "bundle_format_version": 2,
        "native_executable_features": {key: features[key] for key in required},
    }
    (root / "build-info.json").write_text(
        json.dumps(info, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Verified {len(mandatory)} files in {root}; commit {sha[:12]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
