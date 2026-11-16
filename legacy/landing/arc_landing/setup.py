from setuptools import find_packages, setup
from glob import glob
import os

package_name = 'arc_landing'

setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'), glob('launch/*.py')),
        (os.path.join('share', package_name, 'config'), glob('config/*.yaml')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='ARC Drone Team',
    maintainer_email='arc@purdue.edu',
    description='AprilTag-based autonomous landing for ARC drone delivery',
    license='MIT',
    entry_points={
        'console_scripts': [
            'landing_target_node = arc_landing.landing_target_node:main',
            'landing_fsm_node = arc_landing.landing_fsm_node:main',
            'tag_tf_broadcaster = arc_landing.tag_tf_broadcaster:main',
        ],
    },
)
