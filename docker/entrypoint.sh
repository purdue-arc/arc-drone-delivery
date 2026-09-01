#!/bin/bash
# docker/entrypoint.sh
# ----------------------------------------------------------------------------
# Container entrypoint: source ROS, build the workspace if needed, exec CMD.
#
# WHY THIS IS NOT A PLAIN `colcon build`
#
# This repo is a monorepo, not a ROS workspace. There is no top-level `src/`:
# the ROS packages live at
#   navigation-stack/DD_Nav_WS/dd_gazebo_ws/src/   (vision_landing, drone_nav,
#                                                   px4_msgs, px4_ros_com, …)
#   landing/zed_apriltag_streaming/                (perception)
#
# The previous version of this script looked for ${WS}/src, never found it,
# skipped the build, and then sourced whatever install/ happened to be sitting
# at the repo root — which was a months-old build made on the HOST against a
# different ROS distro, containing the deprecated arc_landing MAVROS stack and
# no vision_landing at all. `make up-hw` therefore crash-looped on
#   Package 'vision_landing' not found
# while appearing to have a workspace. Hence: explicit --base-paths, and a
# build directory the CONTAINER owns rather than one shared with the host.
# ----------------------------------------------------------------------------

set -e

# shellcheck disable=SC1091
source "/opt/ros/${ROS_DISTRO}/setup.bash"

REPO="/home/arc/arc_ws"

# Where the ROS packages actually are.
BASE_PATHS=(
    "${REPO}/navigation-stack/DD_Nav_WS/dd_gazebo_ws/src"
    "${REPO}/landing/zed_apriltag_streaming"
)

# Build output lives OUTSIDE the bind mount by default. The host tree carries
# build/ and install/ directories from native host builds against a different
# distro; mixing them with container builds is how the stale-overlay bug above
# happened. Set ARC_WS_BUILD to a path inside the mount if you deliberately
# want the artifacts to persist on the host.
BUILD_WS="${ARC_WS_BUILD:-/home/arc/build_ws}"

# pointcloud_to_grid needs pcl_ros, which is not in this image. It sits in the
# same src/ as the flight packages, so without skipping it a plain colcon build
# fails and takes every other package down with it.
SKIP_PACKAGES="${ARC_WS_SKIP:-pointcloud_to_grid}"

build_ws() {
    echo "[entrypoint] Building ROS workspace into ${BUILD_WS}"
    mkdir -p "${BUILD_WS}"
    cd "${BUILD_WS}"
    colcon build --symlink-install \
        --base-paths "${BASE_PATHS[@]}" \
        --packages-skip ${SKIP_PACKAGES} \
        --cmake-args -DCMAKE_BUILD_TYPE=Release \
        --event-handlers console_cohesion+
}

if [ "${ARC_WS_REBUILD:-0}" = "1" ]; then
    build_ws
elif [ ! -f "${BUILD_WS}/install/setup.bash" ]; then
    echo "[entrypoint] No workspace overlay found — running first-time build"
    build_ws
fi

if [ -f "${BUILD_WS}/install/setup.bash" ]; then
    # shellcheck disable=SC1091
    source "${BUILD_WS}/install/setup.bash"
else
    echo "[entrypoint] WARNING: no workspace overlay at ${BUILD_WS}/install." >&2
    echo "[entrypoint] ROS packages from this repo will NOT be found." >&2
fi

# Fail fast and loudly rather than let a service start against a broken
# overlay. A mission container that cannot find its own launch file should say
# so once, not restart-loop with the same message every few seconds.
if [ -n "${ARC_REQUIRE_PKG:-}" ]; then
    if ! ros2 pkg prefix "${ARC_REQUIRE_PKG}" >/dev/null 2>&1; then
        echo "[entrypoint] FATAL: required package '${ARC_REQUIRE_PKG}' not found." >&2
        echo "[entrypoint] The workspace build failed — scroll up for the error," >&2
        echo "[entrypoint] or rebuild with: ARC_WS_REBUILD=1 make rebuild-ws" >&2
        exit 1
    fi
fi

cd "${REPO}"

if [ $# -eq 0 ]; then
    exec bash
fi

exec "$@"
