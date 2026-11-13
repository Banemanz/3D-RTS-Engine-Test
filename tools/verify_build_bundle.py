#!/usr/bin/env python3
"""Check packaged RTS builds and embed their exact source/run identity.

Runs before uploading any build artifact so missing executables, resources or
incorrect project layouts fail CI rather than producing empty downloads.
"""
from __future__ import annotations

import json
import pathlib
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
        "docs/SC2_FEATURE_MATRIX.md",
        "licenses/PICOJSON_LICENSE",
        "run-game.bat" if windows else "run-game.sh",
        "run-editor.bat" if windows else "run-editor.sh",
    ]

    if windows:
        mandatory.append("symbols/RTSCameraTerrain.pdb")
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

    info = {
        "project": "3D-RTS-Engine-Test",
        "variant": "Release",
        "platform": platform,
        "commit": sha,
        "github_actions_run_id": str(run_id),
        "bundle_format_version": 1,
    }
    (root / "build-info.json").write_text(
        json.dumps(info, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Verified {len(mandatory)} files in {root}; commit {sha[:12]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
