#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")"
exec ./RTSCameraTerrain --project projects/production_showcase.rtsproject.json "$@"
