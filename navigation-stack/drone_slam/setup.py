from setuptools import find_packages, setup

package_name = 'drone_slam'

setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/launch', ['launch/slam.launch.py']),
        ('share/' + package_name + '/rviz',   ['rviz/slam_view.rviz',
                                               'rviz/slam_3d.rviz']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='ARC',
    maintainer_email='arc@purdue.edu',
    description='Lightweight 2-D SLAM for PX4 drones with RViz2 visualisation',
    license='MIT',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'slam_node = drone_slam.slam_node:main',
            'slam_3d_node = drone_slam.slam_3d_node:main',
        ],
    },
)
