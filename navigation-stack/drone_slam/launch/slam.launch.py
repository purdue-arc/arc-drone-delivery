"""
slam.launch.py
==============
Launches the drone_slam node and a dedicated RViz2 window.

Usage
-----
  ros2 launch drone_slam slam.launch.py
  ros2 launch drone_slam slam.launch.py use_sim_time:=true   # simulation (default)
  ros2 launch drone_slam slam.launch.py use_sim_time:=false  # real hardware
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    use_sim_time = LaunchConfiguration('use_sim_time')

    rviz_cfg = PathJoinSubstitution(
        [FindPackageShare('drone_slam'), 'rviz', 'slam_view.rviz']
    )

    slam_node = Node(
        package='drone_slam',
        executable='slam_node',
        name='drone_slam',
        output='screen',
        parameters=[{'use_sim_time': use_sim_time}],
    )

    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2_slam',
        arguments=['-d', rviz_cfg],
        output='screen',
        parameters=[{'use_sim_time': use_sim_time}],
    )

    return LaunchDescription([
        DeclareLaunchArgument(
            'use_sim_time',
            default_value='true',
            description='Use simulation (Gazebo) clock if true',
        ),
        slam_node,
        rviz_node,
    ])
