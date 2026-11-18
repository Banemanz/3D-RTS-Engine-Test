#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")"
exec ./RTSCameraTerrain --editor --project projects/terrain_showcase.rtsproject.json "$@"
