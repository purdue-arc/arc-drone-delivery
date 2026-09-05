from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.substitutions import FindPackageShare
import os

def generate_launch_description():
    drone_nav_share = FindPackageShare('drone_nav').find('drone_nav')
    vision_landing_share = FindPackageShare('vision_landing').find('vision_landing')

    drone_nav_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(drone_nav_share, 'launch', 'navigation.launch.py')
        )
    )

    vision_landing_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(vision_landing_share, 'launch', 'landing_pipeline.launch.py')
        )
    )

    camera_bridge = Node(
        package='topic_tools',
        executable='relay',
        name='camera_relay',
        parameters=[{
            'input_topic': '/camera/image_raw',
            'output_topic': '/zed/zed_node/rgb/image_rect_color',
        }]
    )

    return LaunchDescription([
        drone_nav_launch,
        vision_landing_launch,
        camera_bridge,
    ])