#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")"
exec ./RTSCameraTerrain --project projects/default.rtsproject.json "$@"
