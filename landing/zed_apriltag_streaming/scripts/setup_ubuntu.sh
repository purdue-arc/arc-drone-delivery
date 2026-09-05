#!/usr/bin/env bash
set -euo pipefail

# Ubuntu 22.04+ laptop setup
sudo apt update
sudo apt install -y \
  build-essential cmake git pkg-config \
  libopencv-dev libeigen3-dev \
  gstreamer1.0-tools gstreamer1.0-libav \
  gstreamer1.0-plugins-{base,good,bad,ugly}

echo "Done."
echo "Next:"
echo "  ./scripts/build.sh"
echo "  ./build/zed_apriltag --device 0"
