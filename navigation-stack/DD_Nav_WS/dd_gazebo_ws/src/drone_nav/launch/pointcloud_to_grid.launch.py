from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    # The depth camera in drone_description.urdf publishes PointCloud2 at
    # /camera/depth/points via libgazebo_ros_camera.so.
    # Adjust 'cloud_topic' if your setup uses a different topic.
    return LaunchDescription([
        DeclareLaunchArgument(
            'cloud_topic',
            default_value='/camera/depth/points',
            description='PointCloud2 input topic (depth camera or lidar)'
        ),
        DeclareLaunchArgument(
            'cell_size',
            default_value='0.25',
            description='Grid cell size in metres'
        ),
        DeclareLaunchArgument(
            'length_x',
            default_value='20.0',
            description='Grid length along X (metres)'
        ),
        DeclareLaunchArgument(
            'length_y',
            default_value='20.0',
            description='Grid length along Y (metres)'
        ),
        Node(
            package='pointcloud_to_grid',
            executable='pointcloud_to_grid_node',
            name='pointcloud_to_grid',
            output='screen',
            parameters=[
                {'cloud_in_topic': LaunchConfiguration('cloud_topic')},
                {'cell_size': LaunchConfiguration('cell_size')},
                {'length_x': LaunchConfiguration('length_x')},
                {'length_y': LaunchConfiguration('length_y')},
                {'position_x': 0.0},
                {'position_y': 0.0},
                {'verbose1': False},
                {'verbose2': False},
                # OccupancyGrid output topics
                {'mapi_topic_name': 'intensity_grid'},
                {'maph_topic_name': 'height_grid'},
                # GridMap output topics
                {'mapi_gridmap_topic_name': 'intensity_gridmap'},
                {'maph_gridmap_topic_name': 'height_gridmap'},
            ],
        ),
    ])
