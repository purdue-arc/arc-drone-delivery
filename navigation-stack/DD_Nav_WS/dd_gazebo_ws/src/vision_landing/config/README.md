# vision_landing/config

| File | Used by |
|---|---|
| `nav.rviz` | `delivery.launch.py rviz:=true` — costmap, planned transit route, flown track |
| `perception.rviz` | `delivery.launch.py rviz:=true` — Livox cloud, TF, landing target |
| `delivery.rviz` | Standalone combined view; not launched automatically |
| `livox/` | The Mid-360 driver's JSON — sensor and host IP, UDP ports |
| `fast_lio/` | FAST-LIO2 and `lio_odom_bridge`, one pair of files per world |

## `fast_lio/`

Four files, used in pairs, selected by a single launch argument:

| Pair | `lio_config:=` | IMU |
|---|---|---|
| `mid360_sitl.yaml` + `lio_bridge_sitl.yaml` | `mid360_sitl.yaml` (default) | PX4's, via `px4_imu_bridge` |
| `mid360_aircraft.yaml` + `lio_bridge_aircraft.yaml` | `mid360_aircraft.yaml` | the Mid-360's own, on `/livox/imu` |

The bridge file is derived from the estimator file in `fast_lio.launch.py`
rather than being a second argument. The two have to describe the same IMU —
one says where the lidar is relative to it, the other where `base_link` is —
and as independent knobs they are two chances to produce a stack that starts
cleanly and is quietly wrong, since a mismatched offset biases every
correction rather than failing.

The `mid360_*.yaml` files are derived from upstream's `config/mid360.yaml`.
Every value that differs from upstream is marked `ARC` with the reason. On a
version bump, diff against upstream rather than replacing these.

## Removed: `ekf.yaml`

It configured a `robot_localization` `ekf_filter_node` fusing `/imu/data` with
`/landing_target_pose`. Nothing launched that node, and `/imu/data` is not
published anywhere in this stack — PX4's own EKF2 is the localisation source
and reaches ROS as `/fmu/out/vehicle_odometry`.

Fusing the AprilTag pose into a second state estimator would also have put a
competing position estimate next to the one PX4 flies on. If a visual pose is
ever fed back to the flight controller it goes to
`/fmu/in/vehicle_visual_odometry` (see `src/vio_node.cpp`, which is unvalidated
and not launched), not into a parallel ROS-side filter.
