"""Delivery mission: transit with Nav2 obstacle avoidance, drop, return.

Brings up, on top of the landing pipeline:

  map->odom static TF   PX4's EKF is the localization source, so odom is
                        globally consistent and coincides with map. Nothing
                        else publishes this link (there is no AMCL and no map
                        server), and without it Nav2's global costmap — which
                        works in "map" — has no transform and never plans.
  drone_nav TF/odom     /fmu/out/vehicle_odometry -> /odometry/filtered plus
                        odom->base_footprint->base_link_stabilized->base_link
  nav2 planner_server   global costmap fed by /scan, exposing
                        ComputePathToPose. The controller_server is
                        deliberately NOT started: it would publish /cmd_vel
                        and become a second authority over the aircraft.
  nav2_path_bridge      turns /arc/transit/goal into /arc/transit/path
  landing pipeline      perception + gimbal + payload + mission controller

use_sim_time is FALSE throughout. PX4 over uXRCE-DDS does not publish /clock,
so the ROS side runs on wall time; drone_nav's nav2_params.yaml ships
use_sim_time: True, which would make every TF and scan look unusably stale.

The lidar scan comes from gazebo_scan_bridge, which runs on the HOST against
ROS 2 Humble (that is where the gazebo11 headers are) — start it separately:
  source /opt/ros/humble/setup.bash && source <hostws>/install/setup.bash
  ROS_DOMAIN_ID=0 ros2 run gazebo_scan_bridge gazebo_scan_bridge

Fly a delivery:
  ros2 launch vision_landing delivery.launch.py \\
      delivery_lat:=47.3977512 delivery_lon:=8.5456072
  ros2 topic pub --once /arc/mission/start std_msgs/msg/Bool "{data: true}"
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition, UnlessCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import (LaunchConfiguration, PathJoinSubstitution,
                                  PythonExpression)
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    vision_landing_share = get_package_share_directory('vision_landing')
    drone_nav_share = get_package_share_directory('drone_nav')
    nav2_params = os.path.join(drone_nav_share, 'config', 'nav2_params.yaml')

    delivery_lat = LaunchConfiguration('delivery_lat')
    delivery_lon = LaunchConfiguration('delivery_lon')
    use_nav2 = LaunchConfiguration('use_nav2')

    # map->odom has exactly one publisher, and which one it is depends on
    # whether FAST-LIO is both running AND engaged. Evaluated once here so the
    # static publisher below and the estimator cannot both claim the link —
    # `lio_tf:=true lio:=false` would otherwise leave it with none at all,
    # which silently stops Nav2 planning rather than failing.
    lio_owns_map_odom = PythonExpression([
        "'", LaunchConfiguration('lio'), "'.lower() == 'true' and '",
        LaunchConfiguration('lio_tf'), "'.lower() == 'true'"])

    return LaunchDescription([
        DeclareLaunchArgument('delivery_lat', default_value='nan'),
        DeclareLaunchArgument('delivery_lon', default_value='nan'),
        DeclareLaunchArgument('use_nav2', default_value='true'),
        DeclareLaunchArgument('transit_height_m', default_value='15.0'),
        DeclareLaunchArgument('winch_hover_height_m', default_value='12.0'),
        # Forwarded to landing_pipeline.launch.py, which documents them.
        DeclareLaunchArgument('max_altitude_m', default_value='40.0'),
        DeclareLaunchArgument('max_range_m', default_value='2000.0'),
        DeclareLaunchArgument('require_plan_to_transit', default_value='true'),
        DeclareLaunchArgument(
            'require_costmap_to_fly', default_value='true',
            description='Hold rather than move without a fresh costmap, and '
                        'refuse at preflight a delivery outside the mapped '
                        'area (1400 x 1400 m, so 700 m from the pad). This is '
                        'the "never fly into a detected obstacle" guarantee. '
                        'To reach further, widen the costmap in '
                        'nav2_params.yaml — turning this off flies the leg '
                        'unguarded and says nothing.'),
        DeclareLaunchArgument('transit_speed_mps', default_value='4.0'),
        DeclareLaunchArgument('transit_timeout_sec', default_value='300.0'),
        DeclareLaunchArgument('transit_timeout_margin', default_value='5.0'),
        DeclareLaunchArgument('record', default_value='false'),
        DeclareLaunchArgument('status_topic',
                              default_value='/fmu/out/vehicle_status_v2'),
        DeclareLaunchArgument('local_position_topic',
                              default_value='/fmu/out/vehicle_local_position_v1'),
        DeclareLaunchArgument('battery_topic',
                              default_value='/fmu/out/battery_status_v1'),
        DeclareLaunchArgument('land_detected_topic',
                              default_value='/fmu/out/vehicle_land_detected'),
        DeclareLaunchArgument(
            'slam', default_value='false',
            description='Build a 2D occupancy map from the lidar while flying. '
                        'OBSERVER ONLY: the SLAM node does not publish TF here '
                        '(the mission already owns map->odom->base_link), so it '
                        'cannot destabilise Nav2 or the mission. It publishes '
                        '/drone_slam/map and /drone_slam/path for RViz.'),
        DeclareLaunchArgument(
            'slam_3d', default_value='false',
            description='3D voxel mapping from the lidar, in its OWN RViz '
                        'window. Keeps the height the 2D map throws away, so '
                        'buildings appear as shapes rather than outlines. '
                        'Observer only: publishes no TF. Implies the '
                        'cloud_to_scan flattener is not needed — this consumes '
                        'the full 3D cloud directly.'),
        DeclareLaunchArgument(
            'rviz', default_value='false',
            description='Open RViz with the delivery view (TF, Livox cloud, '
                        'costmap, planned path, landing target).'),
        DeclareLaunchArgument(
            'lidar', default_value='false',
            description='Start the real Livox Mid-360 driver, so '
                        '/livox/points comes from the sensor rather than from '
                        'gazebo_scan_bridge. The mount transform is published '
                        'either way — SITL needs it too. OFF by default: '
                        'on the aircraft this normally runs as its own compose '
                        'service (`--profile lidar`) so it can be restarted '
                        'without taking the mission down. Use this for a '
                        'single-launch bench bring-up, and never alongside the '
                        'compose service — two drivers cannot share the '
                        "sensor's UDP ports."),

        DeclareLaunchArgument(
            'lio', default_value='false',
            description='Run FAST-LIO2: lidar-inertial odometry and 3D '
                        'mapping from /livox/points plus an IMU. Unlike the '
                        'two drone_slam nodes this SOLVES for the aircraft\'s '
                        'pose instead of reading it from PX4, so it is the '
                        'only thing here that can disagree with the flight '
                        'controller. On its own it still moves no frame: see '
                        'lio_tf.'),
        DeclareLaunchArgument(
            'lio_tf', default_value='false',
            description='Let FAST-LIO correct map->odom instead of that link '
                        'being a static identity. Requires lio:=true. This is '
                        'the one argument here that changes what the aircraft '
                        'flies over: the costmap is drawn in map, so engaging '
                        'this slides previously-seen obstacles relative to the '
                        'aircraft. lio_odom_bridge rate limits that slide and '
                        'refuses jumps, but the correction has not yet been '
                        'measured over a real delivery. Fly lio:=true alone '
                        'first and read /arc/lio/correction_norm_m.'),
        DeclareLaunchArgument(
            'lio_config', default_value='mid360_sitl.yaml',
            description='FAST-LIO parameters. mid360_sitl.yaml (PX4\'s IMU) '
                        'or mid360_aircraft.yaml (the Mid-360\'s own IMU, the '
                        'one to fly).'),

        # --- localization frame -------------------------------------------
        # Static identity: PX4's EKF is the localization source, so odom is
        # treated as globally consistent and coincides with map.
        #
        # Suppressed when FAST-LIO is engaged, because then lio_odom_bridge
        # publishes this link instead. Two publishers of one link invalidates
        # the transform tree and stops Nav2 planning.
        Node(
            package='tf2_ros', executable='static_transform_publisher',
            name='map_to_odom',
            condition=UnlessCondition(lio_owns_map_odom),
            arguments=['0', '0', '0', '0', '0', '0', 'map', 'odom'],
        ),

        # --- odom + TF from PX4 -------------------------------------------
        # odom -> base_footprint, driven by /fmu/out/vehicle_odometry.
        Node(package='drone_nav', executable='odom_publisher_broadcaster',
             name='odom_publisher_broadcaster', output='screen'),
        # base_footprint -> base_link is published by
        # odom_publisher_broadcaster, NOT here. It used to be a static
        # identity, which pinned base_link at z = 0 and made the whole
        # transform tree flat — see the comment in that node. Two publishers
        # of one link is also a conflict in its own right.
        #
        # drone_nav's footprint_tf_broadcaster and stabilized_tf_broadcaster
        # are still deliberately NOT started. footprint_tf_broadcaster reads a
        # PX4 message whose definition no longer matches this firmware (DDS
        # rejects it: "payload size 168 > 167"), and stabilized_tf_broadcaster
        # subscribes to demo/imu, a gazebo_ros topic that does not exist here.

        # --- the sensor, and where it sits on the airframe -------------------
        # Included ALWAYS, with the driver itself gated on `lidar`.
        #
        # The mount transform base_link -> livox_frame describes the AIRFRAME,
        # not the driver, and it is needed in both worlds: on the aircraft the
        # real driver stamps its cloud livox_frame, and in SITL
        # gazebo_scan_bridge now stamps the same frame. So the sim resolves the
        # identical chain and actually exercises the mount — which is the one
        # number most likely to be wrong on the real aircraft, and whose
        # failure mode is silent.
        #
        # The compose `lidar` service runs livox_mid360.launch.py separately
        # with publish_mount_tf:=false, so the transform has exactly one
        # publisher in every combination.
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(PathJoinSubstitution([
                FindPackageShare('vision_landing'),
                'launch', 'livox_mid360.launch.py'])),
            launch_arguments={
                'start_driver': LaunchConfiguration('lidar'),
                'publish_mount_tf': 'true',
            }.items(),
        ),

        # --- flight-level filter (feeds the costmap) -----------------------
        # Must run whenever Nav2 does: the costmap's obstacle source is this
        # node's output, so without it the costmap simply stays empty.
        Node(
            package='vision_landing', executable='flight_level_filter',
            name='flight_level_filter', output='screen',
            parameters=[{'use_sim_time': False}],
        ),

        # --- Nav2 planner (no controller_server, by design) ---------------
        Node(
            package='nav2_planner', executable='planner_server',
            name='planner_server', output='screen',
            parameters=[nav2_params, {'use_sim_time': False}],
        ),
        Node(
            package='nav2_lifecycle_manager', executable='lifecycle_manager',
            name='lifecycle_manager_navigation', output='screen',
            parameters=[{
                'use_sim_time': False,
                'autostart': True,
                'node_names': ['planner_server'],
            }],
        ),
        # RViz2's Map display cannot render the costmap here (shader link
        # failure), so republish it as a point cloud, which draws fine.
        #
        # Gated on `rviz` for the same reason it exists: it is a display aid
        # and nothing in the flight path consumes /arc/costmap_cloud. Run
        # unconditionally it walked the whole 1800 x 1800 global costmap once a
        # second on the companion computer with nobody watching.
        Node(
            package='vision_landing', executable='costmap_to_cloud',
            name='costmap_to_cloud', output='screen',
            condition=IfCondition(LaunchConfiguration('rviz')),
            parameters=[{'use_sim_time': False}],
        ),
        Node(
            package='vision_landing', executable='nav2_path_bridge',
            name='nav2_path_bridge', output='screen',
            parameters=[{'use_sim_time': False}],
        ),

        # --- visualisation -------------------------------------------------
        # Both RViz windows run HERE, in the Jazzy container, alongside the
        # publishers. A host-side Humble RViz drops every OccupancyGrid with
        # "sequence size exceeds remaining buffer" — the costmap never renders
        # even though it is published fine.
        Node(
            package='rviz2', executable='rviz2', name='rviz_nav',
            output='screen',
            condition=IfCondition(LaunchConfiguration('rviz')),
            arguments=['-d', os.path.join(vision_landing_share, 'config',
                                          'nav.rviz')],
            parameters=[{'use_sim_time': False}],
        ),
        Node(
            package='rviz2', executable='rviz2', name='rviz_perception',
            output='screen',
            condition=IfCondition(LaunchConfiguration('rviz')),
            arguments=['-d', os.path.join(vision_landing_share, 'config',
                                          'perception.rviz')],
            parameters=[{'use_sim_time': False}],
        ),

        # --- SLAM (optional, observer only) --------------------------------
        # The lidar is 3D and SLAM here is 2D, so the cloud is flattened into a
        # LaserScan first. Same node in SITL and on the aircraft: both consume
        # /livox/points.
        Node(
            package='vision_landing', executable='cloud_to_scan',
            name='cloud_to_scan', output='screen',
            condition=IfCondition(LaunchConfiguration('slam')),
            parameters=[{'use_sim_time': False}],
        ),
        # publish_tf is FALSE deliberately — see the launch argument above.
        # Turning it on here gives map->odom two publishers and base_link two
        # parents, which invalidates the transform tree and stops Nav2 planning.
        Node(
            package='drone_slam', executable='slam_node',
            name='drone_slam', output='screen',
            condition=IfCondition(LaunchConfiguration('slam')),
            parameters=[{
                'use_sim_time': False,
                'publish_tf': False,
                'position_topic': LaunchConfiguration('local_position_topic'),
            }],
        ),

        # --- 3D mapping (optional, observer only, separate window) ---------
        # Consumes the full 3D cloud, not the flattened scan: the whole point
        # is the height that flattening discards. Like the 2D node it reads
        # the aircraft's pose rather than solving for it, and publishes no TF.
        Node(
            package='drone_slam', executable='slam_3d_node',
            name='drone_slam_3d', output='screen',
            condition=IfCondition(LaunchConfiguration('slam_3d')),
            parameters=[{'use_sim_time': False}],
        ),
        # Its own window, deliberately. The nav view is a top-down 2D picture
        # for judging routes; a 3D map shares nothing useful with it and would
        # only obscure the costmap.
        Node(
            package='rviz2', executable='rviz2', name='rviz_slam_3d',
            output='screen',
            condition=IfCondition(LaunchConfiguration('slam_3d')),
            arguments=['-d', PathJoinSubstitution(
                [FindPackageShare('drone_slam'), 'rviz', 'slam_3d.rviz'])],
            parameters=[{'use_sim_time': False}],
        ),

        # --- FAST-LIO2 (optional) ------------------------------------------
        # Lidar-inertial odometry and 3D mapping. The two drone_slam nodes
        # above read the aircraft's pose from PX4 and draw with it; this one
        # solves for the pose, so it is the only node here that can tell you
        # PX4 is wrong.
        #
        # Consumes the same /livox/points as everything else, through a
        # converter that branches off it rather than sitting in it — the
        # costmap's obstacle source is unaffected whether this runs or not.
        #
        # With lio_tf:=false (default) it is an observer like the other two.
        # With lio_tf:=true it takes over map->odom from the static publisher
        # above, which is suppressed in that case.
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(PathJoinSubstitution([
                FindPackageShare('vision_landing'),
                'launch', 'fast_lio.launch.py'])),
            condition=IfCondition(LaunchConfiguration('lio')),
            launch_arguments={
                'lio_config': LaunchConfiguration('lio_config'),
                'lio_tf': LaunchConfiguration('lio_tf'),
                # One `rviz:=true` opens every window, so the LIO view arrives
                # with the others rather than needing its own flag.
                'lio_rviz': LaunchConfiguration('rviz'),
            }.items(),
        ),

        # --- winch --------------------------------------------------------
        Node(
            package='vision_landing', executable='winch_bridge',
            name='winch_bridge', output='screen',
        ),

        # --- perception + gimbal + mission --------------------------------
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(vision_landing_share, 'launch',
                             'landing_pipeline.launch.py')),
            launch_arguments={
                'delivery_lat': delivery_lat,
                'delivery_lon': delivery_lon,
                'use_nav2': use_nav2,
                'transit_height_m': LaunchConfiguration('transit_height_m'),
                'winch_hover_height_m': LaunchConfiguration('winch_hover_height_m'),
                'max_altitude_m': LaunchConfiguration('max_altitude_m'),
                'max_range_m': LaunchConfiguration('max_range_m'),
                'require_plan_to_transit':
                    LaunchConfiguration('require_plan_to_transit'),
                'require_costmap_to_fly':
                    LaunchConfiguration('require_costmap_to_fly'),
                'transit_speed_mps': LaunchConfiguration('transit_speed_mps'),
                'transit_timeout_sec': LaunchConfiguration('transit_timeout_sec'),
                'transit_timeout_margin':
                    LaunchConfiguration('transit_timeout_margin'),
                'record': LaunchConfiguration('record'),
                'status_topic': LaunchConfiguration('status_topic'),
                'local_position_topic': LaunchConfiguration('local_position_topic'),
                'battery_topic': LaunchConfiguration('battery_topic'),
                'land_detected_topic': LaunchConfiguration('land_detected_topic'),
            }.items(),
        ),
    ])
