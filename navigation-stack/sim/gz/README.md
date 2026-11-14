# Gazebo Harmonic SITL

The delivery mission's simulation on **Gazebo Harmonic**, alongside the Gazebo
Classic setup in `../models/` and `../worlds/`. Gazebo Classic reached end of
life in January 2025 and has no ARM packages for Ubuntu 22.04. Harmonic runs
on both x86 and ARM.

**Status: new, and not yet trusted.** Gazebo Classic is still the reference
simulator. Treat this one as a candidate until the mission has passed the same
checks on it (the readiness gate is ten consecutive clean runs). Until then,
anything flown here should also be flown on Classic before it counts.

## What is here

| Path | What it is |
|---|---|
| `run_sitl.sh` | Starts Gazebo on one of these worlds, then PX4 attached to the aircraft in it |
| `models/arc_x500/` | The aircraft: PX4's x500 quadrotor + gimbal camera + Mid-360 stand-in |
| `models/arc_gimbal/` | PX4's CGO3 gimbal with a 640x360 camera (PX4 ships 1280x720) |
| `worlds/apriltag_landing.sdf` | Search and land: ground, the tag, the aircraft |
| `worlds/purdue_campus.sdf` | The delivery world: the same 26 OSM buildings as the Classic world |

The lidar reaches ROS through `gz_lidar_bridge`
(`DD_Nav_WS/dd_gazebo_ws/src/gz_lidar_bridge`), which runs in the Docker
container as the `gz_lidar` service. Nothing needs ROS on the host.

## Setup, once

1. Install Gazebo Harmonic and PX4 at the commit recorded in
   [`../../PX4-AUTOPILOT.md`](../../PX4-AUTOPILOT.md). Run PX4's
   `Tools/setup/ubuntu.sh`, which installs Harmonic on Ubuntu 22.04.
2. Make sure the GStreamer development packages are installed **before**
   building PX4. Without them PX4 silently skips its camera-streaming plugin
   (`libGstCameraSystem.so`) and the landing camera has no video:
   ```bash
   sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
     gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-libav
   ```
   If you installed them after a build, delete
   `PX4-Autopilot/build/px4_sitl_default/CMakeCache.txt` and rebuild.
3. Build PX4 once:
   ```bash
   cd navigation-stack/PX4-Autopilot && make px4_sitl
   ```
4. Build the container image: `make build` from the repo root.

## Fly it

```bash
# Terminal 1: PX4 + Gazebo (add HEADLESS=1 to skip the 3D window)
navigation-stack/sim/gz/run_sitl.sh apriltag_landing    # or purdue_campus

# Terminal 2: the flight stack, plus the lidar bridge
make up-sitl-gz
make logs SVC=mission
make start
```

`make down` stops everything, including `gz_lidar`.

`run_sitl.sh` sets `NAV_DLL_ACT 0` and `COM_RC_IN_MODE 4` once PX4 has booted,
so SITL arms without a ground station or an RC transmitter. Those are SITL
values only; the aircraft's are in `config/px4/`.

## How it differs from the Classic setup

| | Gazebo Classic | Gazebo Harmonic |
|---|---|---|
| Aircraft | Typhoon H480 (hexacopter) | x500 (quadrotor), PX4 airframe 4019 |
| Lidar bridge | `gazebo_scan_bridge`, on the host, needs ROS 2 Humble | `gz_lidar_bridge`, in the container |
| Lidar topic in Gazebo | depends on the world and model name | fixed: `/arc/lidar/scan` |
| Camera | Classic gst plugin, UDP 5600 | PX4's `GstCameraSystem`, UDP 5600 (same H.264 RTP) |
| Gimbal | Classic MAVLink gimbal plugin (`MNT_MODE_OUT 1`) | PX4 drives the joints itself (`MNT_MODE_OUT 2`) |
| How it starts | `make px4_sitl gazebo-classic_typhoon_h480` | `run_sitl.sh` (worlds and models stay in this repo) |

Neither aircraft is the real one. The Tarot T960 is a larger, heavier
hexacopter. Both are stand-ins for exercising mission logic. Neither is good
for flight-dynamics tuning.

## Things that will bite you

**Do not add `<plugin>` elements to a world.** A world with none gets its
system plugins from PX4's `server.config` (physics, sensors, GPS, the camera
streamer). Declaring even one replaces that whole list, and the camera stream
and GPS disappear with it.

**Do not give the camera a `<topic>`.** `GstCameraSystem` finds the camera by
its default topic name and streams the first one it finds.

**`gz_lidar` and Gazebo must agree on `GZ_IP` and `GZ_PARTITION`.** Both use
`127.0.0.1` and `arc_sitl` (`run_sitl.sh` and the compose service set them).
A mismatch fails silently: the bridge starts, logs "Bridging ...", and
`/livox/points` never carries a message. The default partition contains the
username, which differs between the host and the container, so do not unset
it. To run `gz topic` from another terminal, `export GZ_PARTITION=arc_sitl`
there first.

**`px4-gimbal test` is not a fair gimbal test.** It sends zero angular rates,
and PX4's Gazebo gimbal treats a zero rate as "hold where you are", so the
gimbal never moves. `gimbal_bridge` sends NaN rates, and the gimbal tracks the
angle. Verified at v1.16.2: commanded -90 deg, measured -89.2 deg.

**If the camera stays level, check gimbal control.** PX4 denies pitch
commands from anyone not in primary control; `gimbal_bridge` claims it before
each command. `px4-gimbal status` from `rootfs/` should show
`Primary control: 1/1`, and PX4 logs "Configured primary gimbal control ...
to 1/1" when the mission starts.

**The lidar mount is in four files now.** `scripts/check_lidar_mount.py`
compares all of them.

## Verified so far (2026-10-03, ARM VM, no GPU, software rendering)

- PX4 v1.16.2 boots against both worlds, passes preflight and reports
  "Ready for takeoff!" with no manual parameter changes. It takes off in
  `apriltag_landing`. On `purdue_campus` all 26 buildings load and GPS puts
  the aircraft on the pad (40.428920, -86.927991).
- `/livox/points` arrives in ROS in `livox_frame` with wall-clock stamps:
  ~1,770 points per scan on the pad and ~7,200 at 10 m (the downward mount
  working). It runs at about 6.7 Hz rather than 10, because this machine runs
  the simulation slower than real time.
- The camera streams 640x360 H.264 on UDP 5600. The gimbal pitches to -89 deg
  on command, and the AprilTag is visible directly below the aircraft from
  10 m.

- At PX4 `09cb22f799`, in Docker: one full search-and-land mission on
  `apriltag_landing` (`make up-sitl-gz`, `make start`). The tag was found on
  the first scan from 5 m, and the aircraft landed 9 cm from the pad centre.

**Not yet verified:** the mission on `purdue_campus`; the lidar seeing the campus buildings (they are `<polyline>` extrusions, which
load, but no flight has gone near one); the delivery leg; Nav2 routing around
the buildings; and the ten-run gate.
