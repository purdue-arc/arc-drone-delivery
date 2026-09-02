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
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    vision_landing_share = get_package_share_directory('vision_landing')
    drone_nav_share = get_package_share_directory('drone_nav')
    nav2_params = os.path.join(drone_nav_share, 'config', 'nav2_params.yaml')

    delivery_lat = LaunchConfiguration('delivery_lat')
    delivery_lon = LaunchConfiguration('delivery_lon')
    use_nav2 = LaunchConfiguration('use_nav2')

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
            'rviz', default_value='false',
            description='Open RViz with the delivery view (TF, Livox cloud, '
                        'costmap, planned path, landing target).'),

        # --- localization frame -------------------------------------------
        Node(
            package='tf2_ros', executable='static_transform_publisher',
            name='map_to_odom',
            arguments=['0', '0', '0', '0', '0', '0', 'map', 'odom'],
        ),

        # --- odom + TF from PX4 -------------------------------------------
        # odom -> base_footprint, driven by /fmu/out/vehicle_odometry.
        Node(package='drone_nav', executable='odom_publisher_broadcaster',
             name='odom_publisher_broadcaster', output='screen'),
        # base_footprint -> base_link as a static identity.
        #
        # drone_nav's footprint_tf_broadcaster and stabilized_tf_broadcaster
        # are deliberately NOT started. footprint_tf_broadcaster reads a PX4
        # message whose definition no longer matches this firmware (DDS
        # rejects it: "payload size 168 > 167"), and stabilized_tf_broadcaster
        # subscribes to demo/imu, a gazebo_ros topic that does not exist here
        # for the same reason the depth camera does not. Both links would stay
        # unpublished, leaving base_link undefined and Nav2 unable to plan.
        #
        # Identity is correct for our use: planning is 2D at transit altitude,
        # so the ground-projection offset between the frames does not change
        # the costmap footprint.
        Node(
            package='tf2_ros', executable='static_transform_publisher',
            name='footprint_to_base_link',
            arguments=['0', '0', '0', '0', '0', '0',
                       'base_footprint', 'base_link'],
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
        Node(
            package='vision_landing', executable='costmap_to_cloud',
            name='costmap_to_cloud', output='screen',
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
                'record': LaunchConfiguration('record'),
                'status_topic': LaunchConfiguration('status_topic'),
                'local_position_topic': LaunchConfiguration('local_position_topic'),
                'battery_topic': LaunchConfiguration('battery_topic'),
                'land_detected_topic': LaunchConfiguration('land_detected_topic'),
            }.items(),
        ),
    ])
