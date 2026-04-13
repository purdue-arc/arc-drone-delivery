# DD_Nav_WS

## TEMP COPY PASTE:

https://ivory-sale-974.notion.site/DD_Nav_WS-fd09e9d851dd440f878da854978af649

## Simulation Setup                                                              
                                                                                
  You need 4 terminals (+ an optional 5th for the grid map).
                                                                                
  ## Terminal 1 — PX4 SITL + Gazebo                                                
  ```bash                                                              
    cd /home/raghav/Documents/arc-drone-delivery/navigation-stack/DD_Nav_WS/PX4-Autopilot
    export GAZEBO_MODEL_PATH=$GAZEBO_MODEL_PATH:/home/raghav/Documents/arc-drone-delivery/navigation-stack/DD_Nav_WS/gazebo_apriltag/models
    PX4_SITL_WORLD=apriltag_landing PX4_HOME_ALT=5 make px4_sitl gazebo-classic_typhoon_h480    
  ```                                               
  This launches Gazebo Classic with the Typhoon H480 drone and the AprilTag
  landing pad in the world.                                                     
                                                            
  ## Terminal 2 — uXRCE DDS Agent (PX4 ↔ ROS 2 bridge)                             
  ```bash                                                                            
  MicroXRCEAgent udp4 -p 8888
  ```
  Bridges PX4's internal uORB messages to ROS 2 topics. Must be running before  
  the ROS 2 nodes.                                                              
   
  ## Terminal 3 — ROS 2 Stack (navigation + vision landing)                        
  ```bash        
  cd /home/raghav/Documents/arc-drone-delivery/navigation-stack/DD_Nav_WS/navigation-stack/DD_Nav_WS/dd_gazebo_ws    
  source install/setup.bash
  ros2 launch drone_nav master.launch.py 
  ```                                       
  Starts: robot state publisher, TF broadcasters, odometry, AprilTag detector,
  pose estimator, landing controller, VIO, and camera relay.                    
                                                            
  ## Terminal 4 — Mission Controller                                               
  ```bash                                                                           
  cd /home/raghav/Documents/arc-drone-delivery/navigation-stack/DD_Nav_WS/dd_gazebo_ws
  source install/setup.bash                                 
  ros2 run vision_landing mission_controller
  ```
  Runs the autonomous search + precision landing state machine.
                                                                             
  ## Terminal 5 — pointcloud_to_grid (optional)
  ```bash                                                                           
  cd /home/raghav/Documents/arc-drone-delivery/navigation-stack/DD_Nav_WS/dd_gazebo_ws
  source install/setup.bash
  ros2 launch drone_nav pointcloud_to_grid.launch.py 
  ```                           
  Converts the depth camera's PointCloud2 (/camera/depth/points) into occupancy
  and height grid maps.   
                                     
   
  ---                                                                           
 ### Order matters                                             
                                                                                
  Start them in order — PX4/Gazebo first, then the DDS agent, then ROS 2 nodes.
  If the DDS agent isn't up before Terminal 3, the PX4 bridge topics won't      
  connect.                                                  
                                                                                
  First time only — build everything                                            

  ```bash
  cd /home/raghav/Documents/arc-drone-delivery/navigation-stack/DD_Nav_WS/dd_gazebo_ws
  source /opt/ros/humble/setup.bash
  colcon build --symlink-install                                                
  source install/setup.bash
  ```