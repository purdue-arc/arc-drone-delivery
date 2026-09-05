cmake_minimum_required(VERSION 3.16)
project(zed_apriltag_streaming VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(apriltag_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)

add_compile_definitions(ZAS_ENABLE_POSE=1)

# apriltag C library — prefer system install (ros-jazzy-apriltag), fall back to FetchContent
find_package(apriltag QUIET)
if(apriltag_FOUND)
  message(STATUS "Using system apriltag")
  set(APRILTAG_LINK_LIBS apriltag::apriltag)
else()
  message(STATUS "apriltag not found via find_package — fetching from source")
  set(APRILTAG_SOURCE_DIR "" CACHE PATH "Path to a local apriltag checkout (optional)")
  if(APRILTAG_SOURCE_DIR AND EXISTS "${APRILTAG_SOURCE_DIR}/CMakeLists.txt")
    add_subdirectory("${APRILTAG_SOURCE_DIR}" apriltag_build)
  else()
    # Pinned, not master: this is a fallback that only fires when
    # ros-${ROS_DISTRO}-apriltag is missing, and an unpinned upstream would
    # mean the aircraft's detector could change between two builds of the same
    # commit. It is still a network dependency — install the apt package (as
    # docker/Dockerfile.jazzy does) so this path is never taken in the field.
    message(WARNING
      "Building apriltag from source. Prefer the ros-${ROS_DISTRO}-apriltag "
      "package: this path needs network access at build time.")
    include(FetchContent)
    FetchContent_Declare(
      apriltag
      GIT_REPOSITORY https://github.com/AprilRobotics/apriltag.git
      GIT_TAG        v3.4.5
    )
    FetchContent_MakeAvailable(apriltag)
  endif()
  set(APRILTAG_LINK_LIBS apriltag)
endif()

add_executable(zed_apriltag_node
  src/main.cpp
  src/zed_gst_pipeline.cpp
  src/apriltag_detector.cpp
  src/calibration.cpp
)

target_include_directories(zed_apriltag_node PRIVATE ${OpenCV_INCLUDE_DIRS})

target_link_libraries(zed_apriltag_node
  ${APRILTAG_LINK_LIBS}
  ${OpenCV_LIBS}
)

ament_target_dependencies(zed_apriltag_node
  rclcpp
  sensor_msgs
  geometry_msgs
  apriltag_msgs
  cv_bridge
)

install(TARGETS zed_apriltag_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
