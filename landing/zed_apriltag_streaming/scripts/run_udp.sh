#!/usr/bin/env bash
set -euo pipefail
PORT="${1:-5000}"
./build/zed_apriltag --port "$PORT" "${@:2}"
