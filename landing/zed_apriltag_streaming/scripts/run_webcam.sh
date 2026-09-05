#!/usr/bin/env bash
set -euo pipefail
IDX="${1:-0}"
./build/zed_apriltag --device "$IDX" "${@:2}"
