cmake_minimum_required(VERSION 3.8)
project(vision_landing)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(image_transport REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2 REQUIRED)
find_package(vision_msgs REQUIRED)
find_package(px4_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(OpenCV REQUIRED)
find_package(apriltag REQUIRED)
# For livox_pc2_to_custom's output message. This is the vendored driver package
# in this same workspace, not an apt package.
find_package(livox_ros_driver2 REQUIRED)

# `include`, not `include/vision_landing`: headers are included as
# <vision_landing/foo.hpp> so the package name stays in the include path and
# cannot collide with a same-named header from another package.
include_directories(include)

# Perception lives in ONE place: zed_apriltag_streaming's zed_apriltag_node,
# which decodes an H.264 RTP stream and publishes /landing_target_pose. It
# serves SITL (gazebo gst camera plugin on UDP 5600) and hardware (ZED 2i on
# the Jetson) identically.
#
# src/apriltag_detector.cpp is therefore NOT built: it would be a second
# publisher of /landing_target_pose, and its hardcoded intrinsics
# (fx=fy=525, cy=240) are wrong for the 640x360 hfov=2.0 sim camera, whose
# true values are fx=fy=205.5, cy=180 — a 2.56x focal error that corrupts
# every range estimate.
#
# src/payload_bridge.cpp is NOT built either. It is superseded by
# winch_bridge: this aircraft does not drop from a hook on a servo, it lowers
# on a winch, and winch_bridge's release_actuator_index already drives that
# same hook servo. Building both would put two nodes on one PX4 actuator set
# (both default to index 1) fighting over its position, and nothing publishes
# /arc/payload/command — mission_controller speaks /arc/winch/command.
# Kept on disk as the reference for the simple servo-release airframe.
#
# src/landing_controller.cpp and src/pose_estimator.cpp are NOT built.
# Both are superseded prototypes and are unsafe to run alongside the mission
# controller: landing_controller publishes camera-frame positions straight to
# /fmu/in/trajectory_setpoint (wrong frame, and it fights mission_controller
# for the vehicle), and pose_estimator is a second publisher of
# /landing_target_pose fed by a /tag_detections topic nothing produces.
# Kept on disk for reference only — see git history.

add_executable(mission_controller src/mission_controller.cpp)
ament_target_dependencies(mission_controller rclcpp geometry_msgs px4_msgs std_msgs nav_msgs)

add_executable(gimbal_bridge src/gimbal_bridge.cpp)
ament_target_dependencies(gimbal_bridge rclcpp px4_msgs std_msgs)

add_executable(costmap_to_cloud src/costmap_to_cloud.cpp)
ament_target_dependencies(costmap_to_cloud rclcpp nav_msgs sensor_msgs)

# Flattens the 3D lidar cloud into the 2D LaserScan that SLAM consumes. Serves
# SITL and the aircraft identically: both publish /livox/points.
add_executable(cloud_to_scan src/cloud_to_scan.cpp)
ament_target_dependencies(cloud_to_scan rclcpp sensor_msgs)

# Cuts the lidar cloud to the aircraft's own flight level before it reaches
# Nav2's 2D costmap, so "obstacle" means "at my height" rather than "somewhere
# between 2 and 40 m off the ground".
add_executable(flight_level_filter src/flight_level_filter.cpp)
ament_target_dependencies(flight_level_filter rclcpp sensor_msgs tf2_ros tf2)

add_executable(nav2_path_bridge src/nav2_path_bridge.cpp)
ament_target_dependencies(nav2_path_bridge rclcpp rclcpp_action nav2_msgs nav_msgs geometry_msgs)

add_executable(winch_bridge src/winch_bridge.cpp)
ament_target_dependencies(winch_bridge rclcpp px4_msgs std_msgs)

add_executable(vio_node src/vio_node.cpp)
ament_target_dependencies(vio_node rclcpp sensor_msgs px4_msgs cv_bridge OpenCV)

# --- FAST-LIO2 support ------------------------------------------------------
# The three nodes that let FAST-LIO2 run against this stack's topics without
# any of them sitting in the flight-critical obstacle path. See each file's
# header, and DD_Nav_WS/dd_gazebo_ws/src/fast_lio/VENDORED.md.

# Branches /livox/points into the CustomMsg FAST-LIO reads. Deliberately a
# branch and not a replacement: the costmap's cloud never passes through it.
add_executable(livox_pc2_to_custom src/livox_pc2_to_custom.cpp)
ament_target_dependencies(livox_pc2_to_custom rclcpp sensor_msgs livox_ros_driver2)

# PX4's IMU as a sensor_msgs/Imu, FRD->FLU. This is the SITL IMU source; on the
# aircraft FAST-LIO should use the Mid-360's own /livox/imu instead.
add_executable(px4_imu_bridge src/px4_imu_bridge.cpp)
ament_target_dependencies(px4_imu_bridge rclcpp sensor_msgs px4_msgs)

# FAST-LIO's solution -> a slew-limited map->odom correction, or, by default,
# just a measurement of how large that correction would have been.
add_executable(lio_odom_bridge src/lio_odom_bridge.cpp)
ament_target_dependencies(lio_odom_bridge rclcpp nav_msgs sensor_msgs geometry_msgs std_msgs tf2 tf2_ros)

install(TARGETS
  mission_controller
  gimbal_bridge
  winch_bridge
  nav2_path_bridge
  costmap_to_cloud
  cloud_to_scan
  flight_level_filter
  vio_node
  livox_pc2_to_custom
  px4_imu_bridge
  lio_odom_bridge
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include
)

# Tests for the two coordinate transforms that place the aircraft over the pad
# and over the customer. Run with: colcon test --packages-select vision_landing
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_mission_math test/test_mission_math.cpp)
  target_include_directories(test_mission_math PRIVATE include)
endif()

ament_package()
