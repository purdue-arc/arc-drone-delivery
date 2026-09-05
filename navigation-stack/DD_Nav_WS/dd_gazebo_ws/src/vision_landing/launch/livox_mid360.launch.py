# Livox Mid-360 — the obstacle source for the delivery mission.
#
# Starts the vendored livox_ros_driver2 and puts the sensor in the transform
# tree. Everything downstream (Nav2's costmaps, flight_level_filter,
# cloud_to_scan, slam_3d_node) consumes /livox/points and does not know or care
# whether the returns came from this or from gazebo_scan_bridge.
#
# WHY NOT THE DRIVER'S OWN LAUNCH FILES
#
# livox_ros_driver2 ships two per sensor and neither is usable here:
#
#   msg_MID360_launch.py   xfer_format 1 — Livox's own CustomMsg. Nav2's
#                          obstacle layer takes PointCloud2 only, and
#                          flight_level_filter reads it with a
#                          PointCloud2ConstIterator. Nothing in this stack can
#                          read a CustomMsg, and the failure is silent: the
#                          driver runs, the topic exists, the costmap stays
#                          empty, and the mission refuses to transit.
#   rviz_MID360_launch.py  correct format, but also starts RViz — on the
#                          aircraft, with no display.
#
# Both also publish on /livox/lidar with frame_id livox_frame and no transform
# for it, so the cloud cannot be placed in the world even once it is readable.
#
# THE TOPIC NAME MATTERS
#
# /livox/lidar is remapped to /livox/points, which is what gazebo_scan_bridge
# publishes in SITL. That remap is the whole reason the flight code is
# identical in simulation and on the aircraft — the alternative is a topic
# name that differs between the two, which is exactly the kind of difference
# that makes a simulation stop being evidence.
#
# THE SIMULATOR USES THIS FILE TOO
#
# With start_driver:=false nothing talks to a sensor and only the mount
# transform is published. That is what SITL runs, and it is deliberate: the
# simulated bridge stamps its cloud `livox_frame` exactly as the real driver
# does, so both worlds resolve the same transform chain
#   map -> odom -> base_footprint -> base_link -> livox_frame
# and the sim exercises the one number most likely to be wrong on the real
# aircraft. Before this, the bridge stamped `base_link`, which folded the
# 0.15 m offset silently away and left the mount transform untested by
# anything.
#
# THE MOUNT IS A PARAMETER, NOT A CONSTANT
#
# The sensor sits under the airframe ("under_drone"), 0.15 m below base_link,
# matching sim/models/typhoon_h480.sdf. That number is from the simulated
# model and has never been measured on the real aircraft. Measure it, and the
# mount rotation, during the bench bring-up and set mount_* here — do not edit
# the code. A wrong mount transform does not fail loudly; it puts obstacles in
# the wrong place, which looks like the map being "a bit off" right up until
# the aircraft flies into something.

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    default_config = os.path.join(
        get_package_share_directory('vision_landing'),
        'config', 'livox', 'MID360_config.json')

    return LaunchDescription([
        DeclareLaunchArgument(
            'start_driver', default_value='true',
            description='Start the sensor driver. FALSE publishes only the '
                        'mount transform, which is what SITL wants: there the '
                        'cloud comes from gazebo_scan_bridge, but it is '
                        'stamped with the same frame, so the transform chain '
                        'under test is identical.'),
        DeclareLaunchArgument(
            'publish_mount_tf', default_value='true',
            description='Publish base_link -> livox_frame. Set FALSE when '
                        'something else already does — the compose `lidar` '
                        'service does exactly that, because the mission '
                        'service alongside it publishes the airframe geometry.'),
        DeclareLaunchArgument(
            'user_config_path', default_value=default_config,
            description='Livox JSON: sensor IP, host IP and the five UDP '
                        'ports. The host IP must be this machine\'s address '
                        'on the lidar network — the driver BINDS to it, and a '
                        'wrong value logs "bind failed" and then keeps running '
                        'with no topics at all.'),
        DeclareLaunchArgument(
            'cloud_topic', default_value='/livox/points',
            description='Where the PointCloud2 is published. Matches what '
                        'gazebo_scan_bridge publishes in SITL; change it and '
                        'the costmap, the flight-level filter and both SLAM '
                        'nodes all go quiet.'),
        DeclareLaunchArgument(
            'frame_id', default_value='livox_frame',
            description='Frame the cloud is stamped with. Nav2 derives the '
                        'raytracing origin from it, so it must be the '
                        'SENSOR, not base_link.'),
        DeclareLaunchArgument(
            'publish_freq', default_value='10.0',
            description='Hz. 10 matches the simulated bridge. The costmap '
                        'sources set expected_update_rate 0.0, so this is a '
                        'CPU/latency trade rather than a correctness one.'),
        DeclareLaunchArgument(
            'parent_frame', default_value='base_link',
            description='What the sensor is mounted to.'),
        # Under the airframe. See the header before changing these.
        DeclareLaunchArgument('mount_x', default_value='0.0'),
        DeclareLaunchArgument('mount_y', default_value='0.0'),
        DeclareLaunchArgument('mount_z', default_value='-0.15'),
        DeclareLaunchArgument('mount_yaw', default_value='0.0'),
        DeclareLaunchArgument('mount_pitch', default_value='0.0'),
        # INVERTED. The sensor hangs below the airframe (mount_z) and looks
        # DOWN (this roll). The Mid-360's cone is -7..+52 degrees about its own
        # plane — mostly upward — so mounted the right way up under the
        # aircraft it stares at the sky: at 15 m it cannot see the ground at
        # all, and a lidar-inertial estimator has nothing to solve on. Rolled,
        # the cone becomes -52..+7 and the ground is 11.7 m away in cruise.
        #
        # This MUST match the sensor pose in typhoon_h480.sdf, and both must
        # match how the sensor is actually bolted to the aircraft. Nothing
        # checks that; a mismatch puts every obstacle in the wrong place and
        # says nothing.
        DeclareLaunchArgument('mount_roll', default_value='3.14159265'),

        Node(
            package='livox_ros_driver2',
            executable='livox_ros_driver2_node',
            name='livox_mid360',
            output='screen',
            condition=IfCondition(LaunchConfiguration('start_driver')),
            parameters=[{
                # 0 = PointCloud2. See the header: this is the one that
                # matters.
                'xfer_format': 0,
                # One topic for all sensors. There is one sensor.
                'multi_topic': 0,
                # 0 = live lidar (anything else replays a file).
                'data_src': 0,
                'publish_freq': LaunchConfiguration('publish_freq'),
                'output_data_type': 0,
                'frame_id': LaunchConfiguration('frame_id'),
                'user_config_path': LaunchConfiguration('user_config_path'),
                # Unused for the Mid-360 (broadcast-code addressing is an
                # older-generation thing) but the node reads both parameters
                # unconditionally and will not start without them.
                'lvx_file_path': '/dev/null',
                'cmdline_input_bd_code': 'livox0000000001',
            }],
            remappings=[
                ('/livox/lidar', LaunchConfiguration('cloud_topic')),
            ],
        ),

        # The sensor's place on the airframe. Without this the cloud arrives
        # stamped livox_frame with nothing connecting it to base_link, so
        # flight_level_filter's lookupTransform fails on every scan — it now
        # publishes nothing in that case, so the costmap would simply never
        # fill and the mission would refuse to transit.
        Node(
            package='tf2_ros', executable='static_transform_publisher',
            name='base_link_to_livox',
            condition=IfCondition(LaunchConfiguration('publish_mount_tf')),
            arguments=[
                '--x', LaunchConfiguration('mount_x'),
                '--y', LaunchConfiguration('mount_y'),
                '--z', LaunchConfiguration('mount_z'),
                '--yaw', LaunchConfiguration('mount_yaw'),
                '--pitch', LaunchConfiguration('mount_pitch'),
                '--roll', LaunchConfiguration('mount_roll'),
                '--frame-id', LaunchConfiguration('parent_frame'),
                '--child-frame-id', LaunchConfiguration('frame_id'),
            ],
        ),
    ])
