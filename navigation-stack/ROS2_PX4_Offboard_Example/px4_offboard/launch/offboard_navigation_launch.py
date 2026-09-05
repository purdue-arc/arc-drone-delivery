
from launch import LaunchDescription
from launch.actions import ExecuteProcess
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    package_dir = get_package_share_directory('px4_offboard')
    # bash_script_path = os.path.join(package_dir, 'scripts', 'TerminatorScript.sh')


    return LaunchDescription([
        # ExecuteProcess(cmd=['bash', bash_script_path], output='screen'),
        
    

        Node(
            package='px4_offboard',
            namespace='px4_offboard',
            executable='visualizer',
            name='visualizer'
        ),
        Node(
            package='px4_offboard',
            namespace='px4_offboard',
            executable='processes',
            name='processes',
            prefix='gnome-terminal --'
        ),
        Node(
            package='px4_offboard',
            executable='nav2_control',
            name='teleop_twist_keyboard',
            output='screen',
            prefix='gnome-terminal --',
            parameters=[{'use_sim_time': True}]
        ),

        Node(
            package='px4_offboard',
            namespace='px4_offboard',
            executable='velocity_control',
            name='velocity'
        ),
        Node(
            package='rviz2',
            namespace='',
            executable='rviz2',
            name='rviz2',
            arguments=['-d', [os.path.join(package_dir, 'visualize.rviz')]]
        ),
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='static_transform_publisher_base_to_lidar',
            arguments=['0', '0', '0', '0', '0', '0', 'base_link', 'lidar_link'],
            parameters=[{'use_sim_time': True}],
            output='screen'
        ),


    

    ])
