# Vision Landing

Autonomous AprilTag detection and precision landing for the Typhoon H480 in Gazebo SITL.

## Running the Simulation

Run each command in a separate terminal, in order:

**Terminal 1 — PX4 SITL + Gazebo**
```bash
cd /home/raghav/Documents/arc-drone-delivery/navigation-stack/DD_Nav_WS/PX4-Autopilot
export GAZEBO_MODEL_PATH=$GAZEBO_MODEL_PATH:/home/raghav/Documents/arc-drone-delivery/navigation-stack/DD_Nav_WS/gazebo_apriltag/models
PX4_SITL_WORLD=apriltag_landing PX4_HOME_ALT=5 make px4_sitl gazebo-classic_typhoon_h480
```

**Terminal 2 — uXRCE DDS Agent (PX4 ↔ ROS2 bridge)**
```bash
MicroXRCEAgent udp4 -p 8888
```

**Terminal 3 — ROS2 Navigation + Vision Landing**
```bash
cd /home/raghav/Documents/arc-drone-delivery/navigation-stack/DD_Nav_WS
source install/setup.bash
ros2 launch drone_nav master.launch.py
```
**Terminal 4 — Vision Landing Mission Controller**
```bash
cd /home/raghav/Documents/arc-drone-delivery/navigation-stack/DD_Nav_WS
source install/setup.bash
ros2 run vision_landing mission_controller
```
