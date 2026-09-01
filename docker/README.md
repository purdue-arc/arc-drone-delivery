# Docker setup for arc-drone-delivery

Three-container layout running ROS 2 Jazzy on Ubuntu 24.04, designed to work
on both the dev machine (amd64) and Jetson Orin Nano (arm64) running JetPack
6.2 (Ubuntu 22.04 host).

## Why Docker on the Jetson?

JetPack 6.2 ships Ubuntu 22.04 and supports ROS 2 Humble natively, but the
project targets Jazzy on Ubuntu 24.04 for dev/Jetson parity. NVIDIA JetPack 7
(Ubuntu 24.04) currently supports only Jetson Thor — Orin Nano support is on
the roadmap for late Q2 2026. Until then, Docker is the cleanest path to
Jazzy on Orin Nano.

When JetPack 7.2 ships for Orin Nano, the same vision_landing code runs
natively — Docker has been an environment isolator, not a code dependency.

## Where the ROS packages live (read this before debugging a build)

This repo is a **monorepo, not a ROS workspace**. There is no top-level `src/`.
The packages are at:

```
navigation-stack/DD_Nav_WS/dd_gazebo_ws/src/   vision_landing, drone_nav,
                                               px4_msgs, px4_ros_com, …
landing/zed_apriltag_streaming/                perception
```

`entrypoint.sh` knows this and builds with explicit `--base-paths`. Do not run
a bare `colcon build` in `/home/arc/arc_ws` — it will find nothing.

Two consequences worth knowing:

- **Build artifacts live outside the mount.** The container builds into
  `/home/arc/build_ws` (override with `ARC_WS_BUILD`). The repo root carries
  `build/` and `install/` directories from native *host* builds against a
  different ROS distro; mixing them with container builds is exactly the bug
  that made `make up-hw` crash-loop with `Package 'vision_landing' not found`
  while appearing to have a workspace. Those host trees now carry
  `COLCON_IGNORE`.
- **`pointcloud_to_grid` is skipped.** It needs `pcl_ros`, which is not in this
  image, and it sits in the same `src/` as the flight packages — without
  skipping it a plain build fails and takes everything else down with it.
  Override with `ARC_WS_SKIP`.

Rebuild after changing code with `make rebuild-ws` (which sets
`ARC_WS_REBUILD=1`). A service that needs a package it cannot find now exits
with one clear message instead of restart-looping — that is `ARC_REQUIRE_PKG`.

## Files in this directory

| File | Purpose |
|---|---|
| `Dockerfile.jazzy` | Single base image with ROS 2 Jazzy, Micro XRCE-DDS Agent, AprilTag, all project deps |
| `entrypoint.sh` | Container entrypoint — sources ROS, builds workspace on first run |
| `docker-compose.yml` | Base compose with two core services: xrce_agent, mission |
| `docker-compose.sitl.yml` | Overlay for PX4 SITL — Gazebo camera perception, WASD teleop profile |
| `docker-compose.hardware.yml` | Overlay for Tarot T960 — serial Pixhawk agent, ZED capture, delivery mission, Livox profile |
| `.env.example` | Template for environment variables (UID, GID, TAG_SIZE_M, etc.) |

## Architecture (uXRCE-DDS end-to-end — MAVROS dropped)

```
┌──────────────────────── Host OS (Jetson or workstation) ────────────────────────┐
│                                                                                   │
│  ┌─────────────────┐  ┌─────────────────────┐  ┌─────────────────────────────┐   │
│  │  arc_xrce_agent │  │  arc_zed_apriltag   │  │  arc_mission                │   │
│  │  ─────────────  │  │  (hardware overlay) │  │  ───────────                │   │
│  │  MicroXRCEAgent │  │  zed_apriltag_node  │  │  vision_landing             │   │
│  │  PX4 ↔ ROS 2    │  │  → /landing_target_ │  │  mission_controller         │   │
│  │  (/fmu/* topics)│  │    pose, /detections│  │  → /fmu/in/*, /gimbal_tilt_ │   │
│  └────────┬────────┘  └────────┬────────────┘  │    cmd (custom gimbal)      │   │
│           │                    │               └──────────────┬──────────────┘   │
│           └────────────────────┼──────────────────────────────┘                   │
│                                │                                                  │
│                     ROS 2 DDS (Fast DDS over host network + shared memory)        │
│                                │                                                  │
│  ┌─────────────────────────────┴───────────────────────────────────────────────┐  │
│  │  PX4 SITL + Gazebo (on host, NOT in container)                              │  │
│  │  OR — Pixhawk running uxrce_dds_client over /dev/ttyTHS1 (hardware mode)    │  │
│  └─────────────────────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────────────────────────┘
```

All three containers share the same Docker image and the same host
network/IPC namespace. They communicate via ROS 2 DDS — exactly as they
would if running as separate processes natively. Restart any one of them
without affecting the others.

## Quick start (SITL)

```bash
# From repo root, NOT from inside docker/

# 1. Set UID/GID for bind-mount file ownership
cp docker/.env.example docker/.env
# Edit docker/.env if your UID isn't 1000

# 2. Build the image (~10 minutes first time, cached afterward)
make build

# 3. Start PX4 SITL on host (separate terminal)
cd PX4-Autopilot
make px4_sitl gz_typhoon_h480

# 4. Start all containers
make up-sitl

# 5. Watch the mission controller
make logs SVC=mission

# 6. Start the mission (the controller idles until told to go)
docker compose -f docker/docker-compose.yml -f docker/docker-compose.sitl.yml \
  exec mission bash -ic \
  'ros2 topic pub --once /arc/mission/start std_msgs/msg/Bool "{data: true}"'
```

The drone will arm, take off to 5 m, rotate through descending search
levels until it sees the AprilTag, then land on it. Publish
`{data: false}` on the same topic to abort into PX4 AUTO.LAND.

## Quick start (hardware, Jetson on Tarot T960)

```bash
# On the Jetson, from repo root

cp docker/.env.example docker/.env
# Important: edit docker/.env if your Jetson user isn't UID 1000

# Verify Pixhawk wiring
ls -la /dev/ttyTHS1

# Build (slower on Jetson — ~20 min first time)
make build

# Bring up. The mission service runs delivery.launch.py on this overlay:
# Nav2 planning, the winch bridge, transit and return.
make up-hw

# The ZED capture and the lidar driver are opt-in profiles:
docker compose -f docker/docker-compose.yml -f docker/docker-compose.hardware.yml \
  --profile zed --profile lidar up -d

# Confirm the PX4 topic names this build expects actually exist
make check-px4-topics MODE=hw

# Watch the mission FSM's own telemetry (not just console logs)
make mission-state MODE=hw

# Verify PX4 telemetry is flowing over uXRCE-DDS
docker compose -f docker/docker-compose.yml -f docker/docker-compose.hardware.yml \
  exec mission bash -ic 'ros2 topic echo --once /fmu/out/vehicle_status'
```

PX4 side: the Pixhawk must run the uXRCE-DDS client on the wired UART,
e.g. `uxrce_dds_client start -t serial -d /dev/ttyS2 -b 921600` (set the
`UXRCE_DDS_CFG` parameter to the matching TELEM port so it starts on boot).

## Working with running containers

```bash
make ps                          # list running services
make logs SVC=xrce_agent         # tail one service
make logs SVC=all                # tail everything
make shell SVC=mission           # bash inside the mission container
make restart SVC=mission         # restart just the mission controller
make rebuild-ws                  # rebuild colcon workspace in-place
```

## When to restart what

| Symptom | Restart |
|---|---|
| No `/fmu/out/*` topics | `make restart SVC=xrce_agent` (and check the PX4-side uxrce_dds_client) |
| Tag detected but mission not transitioning | `make restart SVC=mission` |
| No `/detections` / `/landing_target_pose` topic | `make restart SVC=zed_apriltag` |
| Changed code in vision_landing or zed_apriltag_streaming | `make rebuild-ws && make restart SVC=mission` |
| `zed_apriltag_node` exits with "No usable camera intrinsics" | `CALIB_FILE` is unset or wrong. This is deliberate — see below |
| Mission holds at launch: "waiting for the first Nav2 plan" | The lidar is not publishing `/livox/points`. Start `--profile lidar` |
| Changed Dockerfile or dependencies | `make build && make down && make up-sitl` |

## Troubleshooting

**`Permission denied` on bind-mounted files:** UID mismatch. Check
`docker/.env` — your USER_UID and USER_GID must match the host user
that owns the repo. `id -u` and `id -g` on the host give the right values.

**Containers can't see each other's topics:** Almost always `network_mode`
or `ipc` not being `host`. Confirm with `docker inspect arc_mission |
grep NetworkMode`. Should print `host`. Also check `ROS_DOMAIN_ID` — PX4's
uxrce_dds_client publishes on DDS domain 0, so the whole stack defaults to 0.

**`/dev/ttyTHS1: Permission denied`:** The container user needs to be
in the `dialout` group. The Dockerfile adds the `arc` user to dialout,
but if you changed the username, update the `usermod -aG` line.

**Agent connects but no `/fmu/out/*` topics:** The PX4-side
`uxrce_dds_client` isn't talking to the agent — for SITL it connects to
`udp4 -p 8888` automatically; on hardware check the UART wiring, baudrate,
and `UXRCE_DDS_CFG`.

**`zed_apriltag_node` refuses to start:** it needs camera intrinsics. Without
them it can detect tags but can never publish `/landing_target_pose`, so the
mission would search until it failsafe-lands — a silent failure that looks
exactly like a missing tag. It now fails loudly on the pad instead. Set
`CALIB_FILE` in `docker/.env` to the calibration's path *inside the container*.

Note the variable name: it is `CALIB_FILE`. `docker/.env` used to define
`ZED_CALIB_FILE`, which no compose file read, so the calibration silently
resolved to empty on every hardware run.

**Mission never leaves preflight, "no PX4 telemetry yet":** the message now
names the topics it is waiting on. Compare them with `ros2 topic list | grep
fmu/out` (or `make check-px4-topics`). If the aircraft's PX4 publishes
versioned names (`vehicle_status_v2`), override all four `PX4_*_TOPIC`
variables in `docker/.env` — a partial override is indistinguishable from a
dead DDS link.

**colcon build fails inside container with permissions error:** Leftover
`build/`, `install/`, `log/` directories from a previous root-owned build.
Clean them: `sudo rm -rf build install log && make rebuild-ws`.

**Slow rebuild every time:** Use `--symlink-install` (already set in
entrypoint) and bind-mount `~/.ccache` if you want ccache speedups.

## Future: Isaac ROS variant

When you switch to GPU-accelerated AprilTag detection on Jetson, add a
`docker-compose.jetson.yml` overlay that adds `runtime: nvidia` to the
zed_apriltag service and swaps the base image to NVIDIA's
`nvcr.io/nvidia/isaac/isaac_ros_dev:jazzy` container. The downstream
mission container needs no changes as long as the replacement publishes
the same `/landing_target_pose` camera-frame pose.

## Migration path to native Jazzy on Jetson

When JetPack 7.2 ships for Orin Nano (expected Q2 2026):
1. `make down` to stop containers
2. Backup `arc_ws/install` and `arc_ws/build`
3. Reflash Jetson with JetPack 7.2
4. `sudo apt install ros-jazzy-ros-base ros-jazzy-apriltag ...` (same
   packages as the Dockerfile) and build the Micro XRCE-DDS Agent from source
5. `cd ~/arc_ws && colcon build`
6. Same `ros2 run vision_landing mission_controller` + `zed_apriltag_node`

The containers are an environmental crutch, not a code dependency.
