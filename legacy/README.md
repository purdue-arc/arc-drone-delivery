# Legacy code

Retired code, kept so the team can read how earlier versions worked.

**Nothing in this folder is built, launched or maintained.** The
`COLCON_IGNORE` file here keeps colcon from building any of it. Do not copy
code out of here into the flight stack without reading why it was retired
below. Several of these pieces are unsafe to run next to the current mission.

## `landing/arc_landing/`

The original landing state machine and landing-target bridge, built on
**MAVROS**.

- **Retired:** 2026-08-31, when the team moved PX4 ↔ ROS 2 communication to
  uXRCE-DDS end-to-end. See `arc_landing/DEPRECATED.md`.
- **Replaced by:** `vision_landing`'s `mission_controller` (mission logic) and
  `zed_apriltag_streaming` (tag detection).
- **Why it can't just be built again:** it depends on `mavros_msgs`, which is
  no longer installed in the `arc-drone:jazzy` Docker image.

## `landing/vision_landing_prototypes/`

Early nodes from the `vision_landing` package. They were never part of the
build, and each one conflicts with a node the mission relies on.

| File | Replaced by | Why it's unsafe |
|---|---|---|
| `apriltag_detector.cpp` | `zed_apriltag_streaming`'s `zed_apriltag_node` | A second publisher of `/landing_target_pose`. Its hardcoded camera intrinsics (fx=fy=525, cy=240) are wrong for the sim camera (fx=fy=205.5, cy=180) by 2.56x, which corrupts every range estimate. |
| `landing_controller.cpp` | `mission_controller` | Publishes camera-frame positions straight to `/fmu/in/trajectory_setpoint`: the wrong frame, and it fights `mission_controller` for control of the aircraft. |
| `pose_estimator.cpp` | `zed_apriltag_node` | A second publisher of `/landing_target_pose`, fed by a `/tag_detections` topic that nothing publishes. |

These files used to live in
`navigation-stack/DD_Nav_WS/dd_gazebo_ws/src/vision_landing/src/`.

## `path_planning_original/`

The first version of the team's A*, D* and Dijkstra grid planners, from the
`DD_Nav_WS` repository. It used to live in `navigation-stack/Path_Planning/`.

- **Replaced by:** `experimental/path_planning/`, a refactor of the same
  algorithms with one header per module.
- **Why keep it:** it has two tests the refactor never ported:
  `tests/benchmark.cpp` (timing comparison of the planners) and
  `tests/a_star_tests.cpp`. They include `header.hh`, so they only build
  against this older layout.
- **Not here:** the Purdue map source data that sat next to this code
  (`worlds/`). It is simulation input, not planner code, and stays in
  `navigation-stack/Path_Planning/worlds/`.
