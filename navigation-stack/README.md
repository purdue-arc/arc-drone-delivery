# Navigation Stack

This directory contains the core ROS 2 navigation workspace for the ARC Drone Delivery project, including the PX4 Gazebo simulation packages from [DroneDeliverySim](https://github.com/Ymz2006/DroneDeliverySim).

## 📁 Structure

```
navigation-stack/
├── DD_Nav_WS/                    # Core navigation workspace
├── Path_Planning/                # Path planning algorithms
├── dd_gazebo_ws/                 # Gazebo workspace
├── lib/                          # Libraries (Eigen)
├── ROS2_PX4_Offboard_Example/   # PX4 offboard velocity & navigation control
├── simple_odometry/              # Odometry TF publisher for PX4
├── config/                       # SLAM toolbox + Nav2 parameter YAMLs
├── default.sdf                   # Gazebo world file
└── model.sdf                     # Drone model file
```

## Prerequisites

- ROS2 Humble
- Gazebo Harmonic
- PX4 Autopilot
- Nav2
- SLAM Toolbox

## Setup

### Install PX4

```bash
git clone https://github.com/PX4/PX4-Autopilot.git --recursive -b release/1.15
bash ./PX4-Autopilot/Tools/setup/ubuntu.sh --no-sim-tools
```

### Install Python Dependencies

```bash
pip3 install --user -U empy pyros-genmsg setuptools
pip3 install kconfiglib
pip install --user jsonschema
pip install --user jinja2
```

### Install Micro DDS

```bash
git clone https://github.com/eProsima/Micro-XRCE-DDS-Agent.git
cd Micro-XRCE-DDS-Agent
mkdir build && cd build
cmake ..
make
sudo make install
sudo ldconfig /usr/local/lib/
```

### Install ROS2 Gazebo Bridge

```bash
apt-get install ros-humble-ros-gzharmonic
```

### Install Navigation 2 + SLAM Toolbox

```bash
sudo apt install ros-humble-navigation2 ros-humble-nav2-bringup
sudo apt install ros-humble-slam-toolbox
```

### Build the Workspace

```bash
source /opt/ros/humble/setup.bash
cd navigation-stack
colcon build
```

## 🗺️ Launching Mapping

SLAM-based mapping using the drone's LiDAR sensor.

```bash
# Source the workspace
source install/setup.bash

# Terminal 1 — velocity controller
ros2 launch px4_offboard offboard_velocity_control.launch.py

# Terminal 2 — TF publisher
ros2 launch simple_odometry px4_odom_tf.launch.py

# Terminal 3 — Gazebo bridge (clock)
ros2 run ros_gz_bridge parameter_bridge /clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock

# Terminal 4 — Gazebo bridge (LiDAR)
ros2 run ros_gz_bridge parameter_bridge /scan@sensor_msgs/msg/LaserScan@gz.msgs.LaserScan

# Terminal 5 — SLAM toolbox
ros2 launch slam_toolbox online_async_launch.py slam_params_file:=./config/mapper_params_online_async.yaml

# Terminal 6 — RViz2
ros2 run rviz2 rviz2
```

## 🧭 Launching Navigation

Autonomous goal-based navigation using Nav2. Set a goal pose in RViz2 and the drone will fly to it.

```bash
# Source the workspace
source install/setup.bash

# Terminal 1 — navigation controller
ros2 launch px4_offboard offboard_navigation_launch.py

# Terminal 2 — TF publisher
ros2 launch simple_odometry px4_odom_tf.launch.py

# Terminal 3 — Gazebo bridge (clock)
ros2 run ros_gz_bridge parameter_bridge /clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock

# Terminal 4 — Gazebo bridge (LiDAR)
ros2 run ros_gz_bridge parameter_bridge /scan@sensor_msgs/msg/LaserScan@gz.msgs.LaserScan

# Terminal 5 — SLAM toolbox
ros2 launch slam_toolbox online_async_launch.py slam_params_file:=./config/mapper_params_online_async.yaml

# Terminal 6 — Nav2
ros2 launch nav2_bringup navigation_launch.py use_sim_time:=true params_file:=./config/drone_nav2_params.yaml

# Terminal 7 — RViz2
ros2 run rviz2 rviz2
```

## 📚 Resources

- [ROS 2 Documentation](https://docs.ros.org/en/humble/)
- [PX4 Autopilot](https://px4.io/)
- [Gazebo Simulation](https://gazebosim.org/)
- [Nav2 Documentation](https://navigation.ros.org/)
- [SLAM Toolbox](https://github.com/SteveMacenski/slam_toolbox)
- [DroneDeliverySim (original)](https://github.com/Ymz2006/DroneDeliverySim)
- [ARK Electronics ROS2 PX4 Offboard Example](https://github.com/ARK-Electronics/ROS2_PX4_Offboard_Example)
