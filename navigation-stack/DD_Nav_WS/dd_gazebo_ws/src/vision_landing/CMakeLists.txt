cmake_minimum_required(VERSION 3.8)
project(vision_landing)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(image_transport REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(vision_msgs REQUIRED)
find_package(px4_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(OpenCV REQUIRED)
find_package(apriltag REQUIRED)

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

add_executable(nav2_path_bridge src/nav2_path_bridge.cpp)
ament_target_dependencies(nav2_path_bridge rclcpp rclcpp_action nav2_msgs nav_msgs geometry_msgs)

add_executable(winch_bridge src/winch_bridge.cpp)
ament_target_dependencies(winch_bridge rclcpp px4_msgs std_msgs)

add_executable(vio_node src/vio_node.cpp)
ament_target_dependencies(vio_node rclcpp sensor_msgs px4_msgs cv_bridge OpenCV)

install(TARGETS
  mission_controller
  gimbal_bridge
  winch_bridge
  nav2_path_bridge
  costmap_to_cloud
  vio_node
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
