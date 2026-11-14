#!/usr/bin/env bash
# Start PX4 SITL against one of this repo's Gazebo Harmonic worlds.
#
#   navigation-stack/sim/gz/run_sitl.sh [world]      # apriltag_landing (default)
#                                                    # or purdue_campus
#   HEADLESS=1 navigation-stack/sim/gz/run_sitl.sh purdue_campus
#   GZ_RENDER_ENGINE=ogre navigation-stack/sim/gz/run_sitl.sh   # see below
#
# Then, in another terminal: make up-sitl-gz
#
# WHY A SCRIPT INSTEAD OF `make px4_sitl gz_<model>`
#
# PX4's make targets can only load worlds and models from inside
# navigation-stack/PX4-Autopilot/, which is gitignored, so anything put there
# is lost on the next clone. This starts Gazebo itself on a world from
# sim/gz/worlds/, with sim/gz/models/ on the resource path, and then starts
# PX4 in standalone mode attached to the aircraft that world already contains.
# Nothing inside PX4-Autopilot/ is modified.
#
# PX4 must be built once first:
#   cd navigation-stack/PX4-Autopilot && make px4_sitl

set -euo pipefail

WORLD="${1:-apriltag_landing}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NAV="$(cd "${HERE}/../.." && pwd)"
PX4_DIR="${PX4_DIR:-${NAV}/PX4-Autopilot}"
ROOTFS="${PX4_DIR}/build/px4_sitl_default/rootfs"
PX4_BIN="${PX4_DIR}/build/px4_sitl_default/bin/px4"
WORLD_FILE="${HERE}/worlds/${WORLD}.sdf"

if [ ! -f "${WORLD_FILE}" ]; then
    echo "No world '${WORLD}'. Available:" >&2
    ls "${HERE}/worlds" | sed -n 's/\.sdf$//p' | sed 's/^/  /' >&2
    exit 1
fi
if [ ! -x "${PX4_BIN}" ] || [ ! -f "${ROOTFS}/gz_env.sh" ]; then
    echo "PX4 SITL is not built. Build it once with:" >&2
    echo "  cd ${PX4_DIR} && make px4_sitl" >&2
    exit 1
fi

# PX4's own Gazebo environment: its plugin directory (GstCameraSystem,
# OpticalFlowSystem) and server.config, which supplies every world's system
# plugins. Our worlds declare none on purpose, so this is what they get.
# (It expands variables that may be unset, hence the set +u around it.)
set +u
# shellcheck disable=SC1091
source "${ROOTFS}/gz_env.sh"
set -u

# Ours first, so arc_* models win; then the AprilTag textures; PX4's models
# (x500, gimbal meshes) were added by gz_env.sh.
export GZ_SIM_RESOURCE_PATH="${HERE}/models:${NAV}/gazebo_apriltag/models:${GZ_SIM_RESOURCE_PATH}"

# Keep gz-transport discovery on loopback, as PX4's own make targets do. The
# lidar bridge in the container (host network) sets the same value.
export GZ_IP=127.0.0.1

# gz-transport only connects peers in the same partition, and the default
# partition is "<hostname>:<username>". The container user is `arc`, not the
# host user, so the lidar bridge never saw this simulator: no error, just no
# /livox/points. A fixed name on both sides (the compose service sets the
# same) avoids that. To use `gz topic` from another terminal, export it there
# too.
export GZ_PARTITION="${GZ_PARTITION:-arc_sitl}"

# Render engine for the camera, the lidar and the GUI: ogre2 (default) or
# ogre (Ogre 1.9). ogre2 needs OpenGL 4.3 features (ARB_texture_buffer_range,
# ARB_copy_image), so on a UTM VM, whose GPU path stops at OpenGL 4.1, it
# only runs in software (LIBGL_ALWAYS_SOFTWARE=1). ogre runs on that GPU.
# ogre's lidar reports each row one vertical step too low; gz_lidar_bridge
# corrects that when the compose side is given the same GZ_RENDER_ENGINE.
GZ_RENDER_ENGINE="${GZ_RENDER_ENGINE:-ogre2}"
case "${GZ_RENDER_ENGINE}" in
    ogre2) ;;
    ogre)
        # The engine for sensors comes from server.config's Sensors system,
        # not the command line, so point Gazebo at a copy that names ogre.
        SERVER_CONFIG="$(mktemp --suffix=.server.config)"
        sed 's|<render_engine>ogre2</render_engine>|<render_engine>ogre</render_engine>|' \
            "${GZ_SIM_SERVER_CONFIG_PATH}" > "${SERVER_CONFIG}"
        export GZ_SIM_SERVER_CONFIG_PATH="${SERVER_CONFIG}"
        ;;
    *)
        echo "GZ_RENDER_ENGINE must be ogre2 or ogre, not '${GZ_RENDER_ENGINE}'." >&2
        exit 1
        ;;
esac

GZ_PID=""
cleanup() {
    [ -n "${GZ_PID}" ] && kill "${GZ_PID}" 2>/dev/null || true
    pkill -f "gz sim -g" 2>/dev/null || true
    [ -n "${SERVER_CONFIG:-}" ] && rm -f "${SERVER_CONFIG}" || true
}
trap cleanup EXIT INT TERM

if gz topic -l 2>/dev/null | grep -q '^/world/.*/clock'; then
    echo "A Gazebo world is already running. Stop it first (pkill -f 'gz sim')." >&2
    exit 1
fi

echo "==> Gazebo: ${WORLD_FILE} (render engine ${GZ_RENDER_ENGINE})"
gz sim --verbose="${GZ_VERBOSE:-1}" -r -s "${WORLD_FILE}" &
GZ_PID=$!

if [ -z "${HEADLESS:-}" ]; then
    gz sim -g --render-engine "${GZ_RENDER_ENGINE}" >/dev/null 2>&1 &
fi

# PX4's standalone wait gives up after 30 s; the campus world can take longer
# than that to load on a slow machine, so wait here instead.
echo "==> Waiting for world '${WORLD}' to load"
for _ in $(seq 1 180); do
    if gz service -i --service "/world/${WORLD}/scene/info" 2>/dev/null \
            | grep -q "Service providers"; then
        break
    fi
    if ! kill -0 "${GZ_PID}" 2>/dev/null; then
        echo "Gazebo exited while loading the world. Rerun with GZ_VERBOSE=4." >&2
        exit 1
    fi
    sleep 1
done

# SITL has no ground station and no RC, and PX4 refuses to arm without one
# ("No connection to the ground control station"). NAV_DLL_ACT 0 and
# COM_RC_IN_MODE 4 lift that, for SITL only; the aircraft's values are in
# config/px4/.
#
# They are set once PX4 has finished booting rather than through PX4_PARAM_*
# environment variables. Those are applied BEFORE the airframe runs, and an
# override equal to the firmware default (NAV_DLL_ACT is 0) does not count as
# a change, so the x500 airframe's later `param set-default NAV_DLL_ACT 2`
# silently wins.
PX4_CLIENT="${PX4_DIR}/build/px4_sitl_default/bin"
(
    for _ in $(seq 1 120); do
        # The logger starts at the end of rcS, after every airframe default.
        if (cd "${ROOTFS}" && "${PX4_CLIENT}/px4-logger" status) >/dev/null 2>&1; then
            cd "${ROOTFS}"
            "${PX4_CLIENT}/px4-param" set NAV_DLL_ACT 0 >/dev/null
            "${PX4_CLIENT}/px4-param" set COM_RC_IN_MODE 4 >/dev/null
            echo "INFO  [run_sitl] SITL arming params set (NAV_DLL_ACT 0, COM_RC_IN_MODE 4)"
            exit 0
        fi
        sleep 1
    done
    echo "WARN  [run_sitl] could not set SITL arming params; PX4 may refuse to arm" >&2
) &

echo "==> PX4 (airframe 4019, attached to arc_x500_0)"
cd "${ROOTFS}"
# Without a terminal on stdin (backgrounded, run from a script or CI), PX4's
# pxh> shell reads EOF and redraws its prompt in a tight loop, which filled a
# 30 GB log in 40 minutes. -d runs PX4 with no shell; use bin/px4-<module>
# from rootfs/ to talk to it instead.
PX4_ARGS=()
if [ ! -t 0 ]; then
    PX4_ARGS+=(-d)
fi
# 4019 = gz_x500_gimbal: the x500's tuning plus gimbal parameters.
PX4_GZ_STANDALONE=1 \
PX4_SYS_AUTOSTART=4019 \
PX4_GZ_MODEL_NAME=arc_x500_0 \
PX4_GZ_WORLD="${WORLD}" \
    "${PX4_BIN}" "${PX4_ARGS[@]}"
