from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='simple_odometry',
            executable='px4_odom_tf',
            name='px4_odometry_node',
            output='screen',
            parameters=[{'use_sim_time': True}],
        ),
    ])
