# vision_landing/config

| File | Used by |
|---|---|
| `nav.rviz` | `delivery.launch.py rviz:=true` — costmap, planned transit route, flown track |
| `perception.rviz` | `delivery.launch.py rviz:=true` — Livox cloud, TF, landing target |
| `delivery.rviz` | Standalone combined view; not launched automatically |

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
