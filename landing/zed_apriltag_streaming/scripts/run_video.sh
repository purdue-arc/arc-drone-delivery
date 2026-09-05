#!/usr/bin/env bash
set -euo pipefail
VID="${1:-}"
if [[ -z "$VID" ]]; then
  echo "Usage: $0 /path/to/video.mp4 [extra args...]"
  exit 2
fi
./build/zed_apriltag --video "$VID" "${@:2}"
