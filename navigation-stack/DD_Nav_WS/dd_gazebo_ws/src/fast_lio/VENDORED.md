# fast_lio — vendored

Upstream: <https://github.com/hku-mars/FAST_LIO>
Branch:   `ROS2`
Commit:   `a4743b095409588842a5b30ddfa27e29d2f99164` (2025-01-15)

Bundled submodule, pinned by the commit above:

- `include/ikd-Tree` — <https://github.com/hku-mars/ikd-Tree> at
  `e2e3f4e9d3b95a9e66b1ba83dc98d4a05ed8a3c4`. Checked in rather than left a
  submodule, for the same reason `livox_ros_driver2` is: a field laptop with no
  internet must still be able to build the flight stack, and a `git clone`
  without `--recursive` would otherwise produce an empty directory and a
  confusing compile error.
- `include/IKFoM_toolkit` — header-only, already a plain directory upstream.

## What it is

FAST-LIO2: a tightly-coupled iterated-Kalman lidar-inertial odometry and
mapping system. It fuses raw 3D lidar points with IMU at every scan, rather
than extracting features first, and maintains the map in an incremental k-d
tree. Here it is the first thing in the stack that **solves** for the
aircraft's pose instead of reading it from PX4.

## Why this fork, and this branch

`hku-mars/FAST_LIO` is the reference implementation from the authors of the
paper. Its `ROS2` branch is the maintained ROS 2 port; the `main` branch is ROS
1 only. Community rewrites exist and are in some cases tidier, but this is the
implementation the published results are about, and the failure modes are the
documented ones.

Pinned by commit because the ROS2 branch is not tagged.

## Native dependencies

None beyond what the image already has. In particular this package does **not**
need `ros-jazzy-pcl-ros` — see local change 3 — so adding FAST-LIO required no
change to `docker/Dockerfile.jazzy` and no image rebuild. It uses PCL
(`libpcl-dev`, already present via `pcl-conversions`) and Eigen.

## Local changes to upstream

Five, all recorded here so a future bump knows what to re-apply.

1. **C++17, not C++14** (`CMakeLists.txt`). Upstream pins C++14 in five
   separate statements, one of them `-std=c++0x`. Jazzy's `rclcpp` headers
   require C++17. The failure is loud but badly misleading: the build dies
   *inside* `rclcpp/any_subscription_callback.hpp` with `no type named
   'variant_type'`, which reads like a broken ROS installation rather than a
   wrong `-std` flag.

2. **`PythonLibs` and `matplotlibcpp` removed** (`CMakeLists.txt`,
   `laserMapping.cpp`, `include/matplotlibcpp.h` deleted). Upstream does
   `find_package(PythonLibs REQUIRED)`, includes `<Python.h>`, and links
   `${PYTHON_LIBRARIES}` — all for a plotting header no translation unit ever
   calls. `PythonLibs` was removed in CMake 3.27, and requiring it would put
   `python3-dev` in the flight image for nothing.

3. **`pcl_ros` dropped, `tf2_ros` added** (`CMakeLists.txt`, `package.xml`).
   No source file includes a `pcl_ros` header; it was only pulling in the PCL
   libraries, which `find_package(PCL ...)` does directly. Dropping it is what
   keeps `ros-jazzy-pcl-ros` out of the image. It was also, silently, the only
   thing providing `tf2_ros` — which `laserMapping.cpp` genuinely includes and
   upstream never declared — so `tf2_ros` is now named explicitly. The `PCL`
   components were widened from `common io` to `common io filters kdtree
   search`, which is what the code actually uses.

4. **Frame names are parameters and the TF broadcast can be turned off**
   (`laserMapping.cpp`). Upstream hardcodes `camera_init` and `body` and
   broadcasts that transform unconditionally. Three parameters were added —
   `publish.odom_frame_id`, `publish.body_frame_id`, `publish.tf_en`, all
   defaulting to upstream's behaviour.

   This stack allows exactly one publisher per transform link and one root.
   Left alone, FAST-LIO adds a second, disconnected root (`camera_init`) to
   `/tf` at 10 Hz. The mission configs set `tf_en: false` and rename the frames
   to `lio_odom` / `lio_body`; `lio_odom_bridge` owns the connection into the
   mission's tree and rate limits it.

5. **Deleted, not modified:** `doc/`, `Log/`, `PCD/`, `rviz_cfg/`,
   `launch/gdb_debug_example.launch` — sample data, output directories and a
   ROS 1 launch file. `src/`, `msg/`, `include/ikd-Tree/`,
   `include/IKFoM_toolkit/` are otherwise untouched.

`config/` is untouched and unused: the mission's parameters live in
`vision_landing/config/fast_lio/`, which is where the ARC-specific values and
the reasons for them are. Upstream's `config/mid360.yaml` is kept only as the
diff target for a future bump.

## What we do NOT use

`launch/mapping.launch.py` starts the estimator plus its own RViz against
upstream's config directory. The mission uses
**`vision_landing/launch/fast_lio.launch.py`**, which additionally starts the
cloud converter, the IMU bridge and `lio_odom_bridge`, remaps the bare global
topic names (`/Odometry`, `/path`, `/cloud_registered`) under `/fast_lio/`, and
selects the aircraft-vs-SITL configuration from a single argument.

## The thing most likely to bite

FAST-LIO does not report degeneracy. At transit height over open ground the
only returns are a flat ground plane, which constrains roll, pitch and height
and leaves x, y and yaw to drift on IMU alone — and the published pose looks
exactly as confident as it does over a street full of buildings. Everything
protecting the stack from that lives in `lio_odom_bridge`, not in here.
