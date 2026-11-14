cmake_minimum_required(VERSION 3.8)
project(gz_lidar_bridge)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

# Gazebo Harmonic transport + msgs. In the arc-drone:jazzy container they
# come from Jazzy's vendor packages (pulled in by ros-jazzy-ros-gz-bridge);
# on a host with gz-harmonic installed from packages.osrfoundation.org they
# are the versioned system packages.
find_package(gz_transport_vendor QUIET)
find_package(gz_msgs_vendor QUIET)
if(gz_transport_vendor_FOUND AND gz_msgs_vendor_FOUND)
  find_package(gz-transport REQUIRED)
  find_package(gz-msgs REQUIRED)
  set(GZ_LIBS gz-transport::core gz-msgs::core)
else()
  find_package(gz-transport13 QUIET)
  find_package(gz-msgs10 QUIET)
  if(gz-transport13_FOUND AND gz-msgs10_FOUND)
    set(GZ_LIBS gz-transport13::core gz-msgs10::core)
  endif()
endif()

# The container's entrypoint builds every package under dd_gazebo_ws/src with
# `set -e`, so a hard failure here would take the whole flight stack down with
# it. Without gz-transport this package is useless but harmless: skip it.
if(NOT GZ_LIBS)
  message(WARNING "gz-transport (Harmonic) not found; gz_lidar_bridge will not be built.")
  ament_package()
  return()
endif()

add_executable(gz_lidar_bridge src/gz_lidar_bridge.cpp)
target_link_libraries(gz_lidar_bridge ${GZ_LIBS})
ament_target_dependencies(gz_lidar_bridge rclcpp sensor_msgs)

install(TARGETS gz_lidar_bridge DESTINATION lib/${PROJECT_NAME})

ament_package()
