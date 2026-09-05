from setuptools import find_packages, setup

package_name = 'simple_odometry'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/launch', ['launch/px4_odom_tf.launch.py']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='jeffrey',
    maintainer_email='jeffrey@todo.todo',
    description='TODO: Package description',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'px4_odom_tf = simple_odometry.px4_odom_tf:main',
        ],
    },
)
