cmake_minimum_required(VERSION 3.8)
project(gazebo_scan_bridge)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

# Gazebo Classic (gazebo11) transport + msgs, via pkg-config. This package is
# built on the HOST against ROS 2 Humble, because that is where the gazebo11
# development headers are. It is deliberately NOT part of the Jazzy container
# workspace — see COLCON_IGNORE.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GAZEBO REQUIRED gazebo)

add_executable(gazebo_scan_bridge src/gazebo_scan_bridge.cpp)
target_include_directories(gazebo_scan_bridge PRIVATE ${GAZEBO_INCLUDE_DIRS})
target_link_directories(gazebo_scan_bridge PRIVATE ${GAZEBO_LIBRARY_DIRS})
target_link_libraries(gazebo_scan_bridge ${GAZEBO_LIBRARIES})
ament_target_dependencies(gazebo_scan_bridge rclcpp sensor_msgs)

install(TARGETS gazebo_scan_bridge DESTINATION lib/${PROJECT_NAME})

ament_package()
