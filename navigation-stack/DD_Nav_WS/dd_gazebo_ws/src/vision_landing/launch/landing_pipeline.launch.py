"""Vision landing pipeline — search and precision landing on an AprilTag.

Single entry point for the whole mission stack:

  zed_apriltag_node  perception. Decodes an H.264 RTP stream and publishes
                     /landing_target_pose (camera-frame tag pose). Identical
                     node in SITL and on the aircraft — only the UDP port and
                     the intrinsics differ.
  gimbal_bridge      /gimbal_tilt_cmd -> PX4 gimbal manager. PX4 routes it to
                     the sim's MAVLink gimbal or, on hardware, to an AUX PWM
                     output feeding the RCTimer/BruGi board.
  mission_controller the mission FSM. Starts IDLE.

Defaults are the SITL values (gazebo gst camera plugin on UDP 5600, a
640x360 hfov=2.0 camera, and the tag in apriltag_landing.world — a 0.5 m
box whose detectable black quad measures 0.4 m).

Fly it:
    ros2 topic pub --once /arc/mission/start std_msgs/msg/Bool "{data: true}"
Abort into PX4 AUTO.LAND:
    ros2 topic pub --once /arc/mission/start std_msgs/msg/Bool "{data: false}"

On hardware, override: udp_port, tag_size_m, and calib_file (the ZED 2i
calibration — generate it with
landing/zed_apriltag_streaming/scripts/zed_calib_to_yaml.py). Do NOT fly with
the sim intrinsics; they are wrong for the ZED by roughly 2.5x.

Deliberately NOT launched:
  - vision_landing/apriltag_detector — superseded by zed_apriltag_node; it
    would be a second publisher of /landing_target_pose and its hardcoded
    intrinsics are wrong for the sim camera.
  - landing_controller — publishes to /fmu/in/trajectory_setpoint and would
    fight mission_controller for the aircraft.
  - pose_estimator — reads /tag_detections, which nothing publishes.
  - vio_node — feeds the PX4 EKF; unvalidated, so opt in with vio:=true.
"""
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.actions import Node


def generate_launch_description():
    perception = LaunchConfiguration('perception')
    udp_port = LaunchConfiguration('udp_port')
    tag_size_m = LaunchConfiguration('tag_size_m')
    target_tag_id = LaunchConfiguration('target_tag_id')
    calib_file = LaunchConfiguration('calib_file')
    fx = LaunchConfiguration('fx')
    fy = LaunchConfiguration('fy')
    cx = LaunchConfiguration('cx')
    cy = LaunchConfiguration('cy')
    vio = LaunchConfiguration('vio')

    gst = PythonExpression([
        "'udpsrc port=' + '", udp_port,
        "' + ' caps=application/x-rtp,media=video,encoding-name=H264,payload=96"
        " ! rtph264depay ! avdec_h264 ! videoconvert ! appsink drop=1 sync=false'",
    ])

    return LaunchDescription([
        DeclareLaunchArgument('perception', default_value='true'),
        DeclareLaunchArgument(
            'udp_port', default_value='5600',
            description='RTP port. 5600 = gazebo gst camera plugin (SITL); '
                        'the Jetson ZED stream uses 5000 by default.'),
        DeclareLaunchArgument(
            'tag_size_m', default_value='0.4',
            description='Edge length of the tag\'s BLACK QUAD, not the printed '
                        'sheet. The sim box is 0.5 m but the quad covers 80% of '
                        'the texture, so 0.4. Set to the measured black square '
                        'on hardware.'),
        DeclareLaunchArgument('target_tag_id', default_value='0'),
        DeclareLaunchArgument(
            'calib_file', default_value='',
            description='Camera intrinsics YAML. Overrides fx/fy/cx/cy. '
                        'Required on hardware.'),
        # SITL camera: 640x360, horizontal_fov 2.0 rad
        #   fx = fy = (640/2)/tan(1.0) = 205.47, cx = 320, cy = 180
        DeclareLaunchArgument('fx', default_value='205.47'),
        DeclareLaunchArgument('fy', default_value='205.47'),
        DeclareLaunchArgument('cx', default_value='320.0'),
        DeclareLaunchArgument('cy', default_value='180.0'),
        DeclareLaunchArgument('vio', default_value='false'),
        DeclareLaunchArgument('delivery_lat', default_value='nan'),
        DeclareLaunchArgument('delivery_lon', default_value='nan'),
        DeclareLaunchArgument('use_nav2', default_value='false'),
        DeclareLaunchArgument('transit_height_m', default_value='15.0'),
        DeclareLaunchArgument('winch_hover_height_m', default_value='12.0'),
        DeclareLaunchArgument(
            'max_altitude_m', default_value='40.0',
            description='Companion-side altitude fence, metres above the launch '
                        'point. Must exceed transit_height_m. This does NOT '
                        'replace the PX4 GF_* geofence — see config/px4/.'),
        DeclareLaunchArgument(
            'max_range_m', default_value='2000.0',
            description='Companion-side range fence from the launch point.'),
        DeclareLaunchArgument(
            'require_plan_to_transit', default_value='true',
            description='With use_nav2, refuse to start a transit until Nav2 has '
                        'returned at least one plan. Set false only when you '
                        'have accepted flying the leg with no obstacle data.'),
        DeclareLaunchArgument(
            'record', default_value='false',
            description='Record a rosbag of the mission (state, detections, '
                        'PX4 telemetry). Fly every hardware test with this on: '
                        'it is the only post-flight record of WHY the '
                        'controller did what the PX4 log shows it doing.'),
        DeclareLaunchArgument('bag_dir', default_value='mission_bag'),
        # Mission timings. Declared HERE so they can actually be set: passing
        # transit_timeout_sec:= to a launch file that never declared it is
        # accepted in silence and ignored, which cost a 596 m flight that
        # timed out 58 m short on a default nobody meant to be using.
        DeclareLaunchArgument('transit_speed_mps', default_value='4.0'),
        DeclareLaunchArgument(
            'transit_timeout_sec', default_value='300.0',
            description='FLOOR for the per-leg deadline. The actual deadline '
                        'scales with the distance being flown — see '
                        'transit_timeout_margin.'),
        DeclareLaunchArgument('transit_timeout_margin', default_value='3.5'),

        # PX4 topic names.
        #
        # PX4 v1.16's uxrce_dds_client appends a version suffix to exactly
        # those messages that have a versioned definition in msg/versioned/,
        # and leaves the others bare. So the set is MIXED, and that is not a
        # mistake:
        #     vehicle_status_v2, vehicle_local_position_v1, battery_status_v1
        #     vehicle_land_detected, vehicle_global_position   (no suffix)
        #
        # Do not "fix" the inconsistency by making them uniform. Verified
        # against the running SITL with `ros2 topic list | grep fmu/out`;
        # dds_topics.yaml lists the BASE names and is not what appears on the
        # wire. A partial override is the failure that looks exactly like a
        # dead DDS link, so if the aircraft differs, override all four.
        DeclareLaunchArgument('status_topic',
                              default_value='/fmu/out/vehicle_status_v2'),
        DeclareLaunchArgument('local_position_topic',
                              default_value='/fmu/out/vehicle_local_position_v1'),
        DeclareLaunchArgument('battery_topic',
                              default_value='/fmu/out/battery_status_v1'),
        DeclareLaunchArgument('land_detected_topic',
                              default_value='/fmu/out/vehicle_land_detected'),

        Node(
            package='zed_apriltag_streaming',
            executable='zed_apriltag_node',
            name='zed_apriltag_node',
            output='screen',
            condition=IfCondition(perception),
            parameters=[{
                'source': 'gst',
                'gst_pipeline': gst,
                'tag_family': 'tag36h11',
                'tag_size_m': tag_size_m,
                'target_tag_id': target_tag_id,
                'calib_file': calib_file,
                'fx': fx, 'fy': fy, 'cx': cx, 'cy': cy,
                'frame_id': 'camera_down',
                # Without intrinsics the node can detect tags but can never
                # publish a pose, so the mission searches until it gives up.
                # Fail loudly on the pad instead.
                'require_pose': True,
            }],
        ),
        Node(
            package='vision_landing',
            executable='gimbal_bridge',
            name='gimbal_bridge',
            output='screen',
        ),
        Node(
            package='vision_landing',
            executable='mission_controller',
            name='mission_controller',
            output='screen',
            # A bare LaunchConfiguration reaches the node as a STRING. The
            # node declares these as double/bool, and the type mismatch aborts
            # it inside declare_parameter — before it logs anything, which is
            # why it started and then went silent. value_type coerces them.
            parameters=[{
                'delivery_lat': ParameterValue(
                    LaunchConfiguration('delivery_lat'), value_type=float),
                'delivery_lon': ParameterValue(
                    LaunchConfiguration('delivery_lon'), value_type=float),
                'use_nav2': ParameterValue(
                    LaunchConfiguration('use_nav2'), value_type=bool),
                'transit_height_m': ParameterValue(
                    LaunchConfiguration('transit_height_m'), value_type=float),
                'winch_hover_height_m': ParameterValue(
                    LaunchConfiguration('winch_hover_height_m'), value_type=float),
                'max_altitude_m': ParameterValue(
                    LaunchConfiguration('max_altitude_m'), value_type=float),
                'transit_speed_mps': ParameterValue(
                    LaunchConfiguration('transit_speed_mps'), value_type=float),
                'transit_timeout_sec': ParameterValue(
                    LaunchConfiguration('transit_timeout_sec'), value_type=float),
                'transit_timeout_margin': ParameterValue(
                    LaunchConfiguration('transit_timeout_margin'), value_type=float),
                'max_range_m': ParameterValue(
                    LaunchConfiguration('max_range_m'), value_type=float),
                'require_plan_to_transit': ParameterValue(
                    LaunchConfiguration('require_plan_to_transit'), value_type=bool),
                'status_topic': LaunchConfiguration('status_topic'),
                'local_position_topic': LaunchConfiguration('local_position_topic'),
                'battery_topic': LaunchConfiguration('battery_topic'),
                'land_detected_topic': LaunchConfiguration('land_detected_topic'),
            }],
        ),

        # Flight recorder. /arc/mission/state carries the FSM's own account of
        # itself — state, time in state, tag age, winch state, failsafe reason
        # — which is the piece the PX4 log cannot supply.
        ExecuteProcess(
            condition=IfCondition(LaunchConfiguration('record')),
            cmd=['ros2', 'bag', 'record', '-o', LaunchConfiguration('bag_dir'),
                 '/arc/mission/state', '/arc/mission/start',
                 '/arc/winch/state', '/arc/winch/command',
                 '/landing_target_pose', '/gimbal_tilt_cmd',
                 '/arc/transit/goal', '/arc/transit/path',
                 LaunchConfiguration('status_topic'),
                 LaunchConfiguration('local_position_topic'),
                 LaunchConfiguration('battery_topic'),
                 LaunchConfiguration('land_detected_topic'),
                 '/fmu/out/vehicle_global_position',
                 '/fmu/out/vehicle_odometry',
                 '/fmu/in/trajectory_setpoint'],
            output='screen',
        ),
        Node(
            package='vision_landing',
            executable='vio_node',
            name='vio_node',
            output='screen',
            condition=IfCondition(vio),
        ),
    ])
