# DEPRECATED — MAVROS stack dropped (2026-08-31)

The team decided to use **uXRCE-DDS end-to-end** for PX4 ↔ ROS 2. This
package (landing FSM + landing target bridge over MAVROS) is superseded by:

- **Mission logic**: `navigation-stack/DD_Nav_WS/dd_gazebo_ws/src/vision_landing`
  (`mission_controller` — search, approach, precision landing, failsafes —
  talking to PX4 directly on `/fmu/*` topics via the Micro XRCE-DDS Agent)
- **Perception**: `landing/zed_apriltag_streaming`
  (`zed_apriltag_node` — publishes `/landing_target_pose`, the camera-frame
  tag pose the mission controller consumes)
- **Deployment**: `docker/` compose files (`xrce_agent` + `mission` +
  `zed_apriltag` services)

Nothing here is launched by the docker stack anymore. Kept for reference
only; do not extend it.
