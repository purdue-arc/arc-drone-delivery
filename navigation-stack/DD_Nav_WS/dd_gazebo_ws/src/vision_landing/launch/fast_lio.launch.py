# FAST-LIO2 — lidar-inertial odometry and 3D mapping, wired to this stack.
#
# Starts four things:
#
#   livox_pc2_to_custom  /livox/points -> /livox/lidar_custom, the per-point
#                        timed format FAST-LIO reads. A BRANCH off the cloud,
#                        not a link in it: the costmap's obstacle source is
#                        untouched, so nothing here can stop the aircraft
#                        seeing a building.
#   px4_imu_bridge       PX4's IMU as sensor_msgs/Imu (SITL only by default).
#   fastlio_mapping      the estimator.
#   lio_odom_bridge      its solution -> a rate-limited map->odom correction,
#                        or, by default, only a measurement of one.
#
# HOW THIS RELATES TO THE OTHER TWO SLAM NODES
#
# drone_slam's slam_node and slam_3d_node are not estimators. They read the
# aircraft's pose from PX4 and rasterize the lidar into a picture — useful for
# confirming the sensor is mounted and aimed correctly, useless for finding out
# whether the pose is right, because they assume it. FAST-LIO solves for the
# pose from the cloud and the IMU, so it is the first thing here that can
# disagree with PX4. That disagreement is the point, and /arc/lio/correction_norm_m
# is where you read it.
#
# TWO STAGES, AND THE ORDER MATTERS
#
#   lio:=true                     observer. Full estimator, no transform
#                                 touched, correction measured and logged.
#   lio:=true lio_tf:=true        engaged. lio_odom_bridge owns map->odom.
#
# Fly the first for a while before the second. The correction's size over a
# full delivery is the evidence for whether engaging it is an improvement or a
# way to slide a costmap around under a flying aircraft, and there is currently
# no such evidence for this airframe.
#
# START IT ON THE GROUND. FAST-LIO estimates the gravity vector from the first
# ~0.1 s of IMU and needs the aircraft stationary for it. Started mid-flight it
# initializes against accelerating measurements and the whole solution is tilted.

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import (LaunchConfiguration, PathJoinSubstitution,
                                  PythonExpression)
from launch_ros.actions import Node
# Without value_type, a LaunchConfiguration reaches the node as the STRING
# "false", which rclcpp then rejects against a bool parameter — the node dies
# at startup rather than defaulting.
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_file = LaunchConfiguration('lio_config')

    # ONE knob picks the world, and everything that has to agree with it is
    # derived rather than declared separately.
    #
    # Three things must describe the same IMU: which topic FAST-LIO reads,
    # whether px4_imu_bridge is running to feed it, and where base_link sits
    # relative to it. As independent arguments those are three chances to
    # produce a stack that starts cleanly, publishes confidently, and is wrong
    # — a mismatched extrinsic biases every correction by its own error and
    # reports nothing. So `lio_config` selects the other two.
    is_aircraft = ["'aircraft' in '", config_file, "'"]
    bridge_config = PythonExpression(
        ["'lio_bridge_aircraft.yaml' if ", *is_aircraft,
         " else 'lio_bridge_sitl.yaml'"])
    # The aircraft reads the Mid-360's own /livox/imu, so PX4's would be a
    # topic nothing subscribes to.
    run_imu_bridge = PythonExpression(
        ["'false' if ", *is_aircraft, " else 'true'"])

    return LaunchDescription([
        DeclareLaunchArgument(
            'lio_config', default_value='mid360_sitl.yaml',
            description='FAST-LIO parameter file in vision_landing/config/'
                        'fast_lio, and the only thing that needs setting to '
                        'switch worlds. mid360_sitl.yaml uses PX4\'s IMU '
                        'because the simulator has no other; '
                        'mid360_aircraft.yaml uses the Mid-360\'s own, which '
                        'is the one to fly. The matching lio_odom_bridge '
                        'config and the IMU bridge are selected from this. '
                        'The two files list every way they differ.'),
        DeclareLaunchArgument(
            'lio_rviz', default_value='false',
            description='Open the FAST-LIO window (config/lio.rviz): the '
                        'accumulated LIO map, the live scan, FAST-LIO\'s '
                        'track and PX4\'s TF axes, all drawn in `map` so the '
                        'two estimates can be compared by eye. '
                        'delivery.launch.py sets this from its own `rviz`.'),
        DeclareLaunchArgument(
            'lio_tf', default_value='false',
            description='Let FAST-LIO correct map->odom. FALSE (default) runs '
                        'the estimator as a pure observer: it publishes '
                        '/arc/lio/correction_norm_m saying how far it and PX4 '
                        'disagree, and moves no frame. TRUE hands '
                        'lio_odom_bridge the map->odom link — whoever sets it '
                        'MUST also stop the static identity publisher, which '
                        'delivery.launch.py does automatically.'),

        Node(
            package='vision_landing', executable='livox_pc2_to_custom',
            name='livox_pc2_to_custom', output='screen',
            parameters=[{'use_sim_time': False}],
        ),

        Node(
            package='vision_landing', executable='px4_imu_bridge',
            name='px4_imu_bridge', output='screen',
            condition=IfCondition(run_imu_bridge),
            parameters=[{'use_sim_time': False}],
        ),

        Node(
            package='fast_lio', executable='fastlio_mapping',
            name='fast_lio', output='screen',
            parameters=[
                PathJoinSubstitution([
                    FindPackageShare('vision_landing'),
                    'config', 'fast_lio', config_file]),
                {'use_sim_time': False},
            ],
            # Upstream publishes on bare global names — /Odometry, /path,
            # /cloud_registered. /path in particular is close enough to Nav2's
            # planned-path topics to be confusing in a bag, and none of them
            # say where they came from. Namespaced here rather than patched
            # into the vendored source, so a version bump has less to re-apply.
            remappings=[
                ('/Odometry', '/fast_lio/odometry'),
                ('/path', '/fast_lio/path'),
                ('/cloud_registered', '/fast_lio/cloud_registered'),
                ('/cloud_registered_body', '/fast_lio/cloud_registered_body'),
                ('/cloud_effected', '/fast_lio/cloud_effected'),
                ('/Laser_map', '/fast_lio/laser_map'),
            ],
        ),

        # Its own window, like the 3D SLAM view and for the same reason: the
        # nav view is a top-down 2D picture for judging routes, and a 3D map
        # shares nothing useful with it. This one is drawn in `map` so
        # FAST-LIO's track and PX4's TF axes are directly comparable — the two
        # separating is the measurement.
        Node(
            package='rviz2', executable='rviz2', name='rviz_lio',
            output='screen',
            condition=IfCondition(LaunchConfiguration('lio_rviz')),
            arguments=['-d', PathJoinSubstitution([
                FindPackageShare('vision_landing'), 'config', 'lio.rviz'])],
            parameters=[{'use_sim_time': False}],
        ),

        Node(
            package='vision_landing', executable='lio_odom_bridge',
            name='lio_odom_bridge', output='screen',
            parameters=[
                PathJoinSubstitution([
                    FindPackageShare('vision_landing'),
                    'config', 'fast_lio', bridge_config]),
                {
                    'use_sim_time': False,
                    'publish_tf': ParameterValue(
                        LaunchConfiguration('lio_tf'), value_type=bool),
                },
            ],
        ),
    ])
