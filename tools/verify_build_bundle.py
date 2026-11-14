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
        "docs/AUTHORING.md",
        "docs/BUILD_ARTIFACTS.md",
        "docs/CREATOR_ARCHITECTURE.md",
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

    for name in ("default", "terrain_showcase"):
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
