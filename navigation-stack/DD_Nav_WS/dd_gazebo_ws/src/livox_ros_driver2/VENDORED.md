# livox_ros_driver2 — vendored

Upstream: <https://github.com/Livox-SDK/livox_ros_driver2>
Commit:   `4a1def929e5b59c7a8122d19fce6efba581ce9f7` (2026-07-31), version 1.2.7

Vendored rather than cloned at build time for the same reason `apriltag` is
pinned to a tag: a field laptop with no internet must still be able to build
the flight stack. It is small (~1.2 MB) and it is a flight dependency — the
costmap has no obstacle source without it.

## Why 1.2.7

`1.2.6` is the release that added **Ubuntu 24.04 / ROS 2 Jazzy** support,
which is what `arc-drone:jazzy` runs. `1.2.7` is the current head and adds
Avia2 support plus a fix for "no point cloud published when all config items
are omitted". Pinned by commit because 1.2.7 is not tagged yet.

## Native dependency

Requires **Livox-SDK2** (`liblivox_lidar_sdk_shared.so` in `/usr/local/lib`),
built from source in `docker/Dockerfile.jazzy` and pinned to commit
`08f523c930b2f0ba1e98a6afaa8d7476bf479908` (2026-07-31) of
<https://github.com/Livox-SDK/Livox-SDK2>. The driver and the SDK are released
together — both of these commits are from the same day — so bump them together
or not at all.

## Local changes to upstream

Three, all recorded here so a future bump knows what to re-apply:

1. **`package.xml` is committed.** Upstream ships `package_ROS1.xml` and
   `package_ROS2.xml` and expects `build.sh` to copy the right one into place.
   A plain `colcon build` over `src/` sees a directory with no manifest and
   skips the package silently. `package_ROS2.xml` is kept alongside so the
   provenance is visible; `package_ROS1.xml` and `build.sh` are deleted, since
   this repo is ROS 2 only and leaving a build script that runs
   `rm -rf ../../build/ ../../install/` inside a shared workspace is a trap.
2. **`CMakeLists.txt` defaults `DISTRO_ROS` from `$ENV{ROS_DISTRO}`** when it
   is not passed. See the marked block; the failure it prevents is a link
   error, not a clear message.
3. Nothing else. `src/`, `msg/`, `config/` and the launch directories are
   untouched.

## What we do NOT use

`launch_ROS2/msg_MID360_launch.py` publishes `xfer_format: 1` — Livox's own
`CustomMsg`, which neither Nav2's costmap nor `flight_level_filter` can read.
`rviz_MID360_launch.py` publishes PointCloud2 but also starts RViz.

The mission uses **`vision_landing/launch/livox_mid360.launch.py`** instead,
which sets PointCloud2, remaps `/livox/lidar` → `/livox/points` to match what
the simulator bridge publishes, and puts the sensor in the transform tree.
