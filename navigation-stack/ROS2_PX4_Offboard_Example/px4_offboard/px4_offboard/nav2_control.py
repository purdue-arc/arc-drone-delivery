#!/usr/bin/env python3
import sys
import geometry_msgs.msg
import rclpy
from rclpy.node import Node
from std_msgs.msg import Bool
from rclpy.qos import QoSProfile, QoSReliabilityPolicy, QoSHistoryPolicy, QoSDurabilityPolicy
from geometry_msgs.msg import PoseStamped
from geometry_msgs.msg import Twist,  Quaternion
from visualization_msgs.msg import Marker
import math 
if sys.platform == 'win32':
    import msvcrt
else:
    import termios
    import tty


msg = """
NAV2 MODE.


Press SPACE to arm/disarm the drone
"""



class TeleopNode(Node):
    def __init__(self):
        super().__init__('teleop_twist_keyboard')

        qos_profile = QoSProfile(
            reliability=QoSReliabilityPolicy.BEST_EFFORT,
            durability=QoSDurabilityPolicy.TRANSIENT_LOCAL,
            history=QoSHistoryPolicy.KEEP_LAST,
            depth=10
        )


        self.pub = self.create_publisher(geometry_msgs.msg.Twist, '/offboard_velocity_cmd', qos_profile)
        self.arm_pub = self.create_publisher(Bool, '/arm_message', qos_profile)
        
        self.goal_sub = self.create_subscription(PoseStamped, '/goal_pose', self.goal_callback, 10)
        self.cmd_vel_sub = self.create_subscription(Twist,'/cmd_vel', self.cmd_vel_callback, 10)

        self.arm_toggle = False
        
        
        self.start_flight = False
        self.started_flight = False 
        
        # store current state
        self.x = 0.0
        self.y = 0.0
        self.z = 0.1
        self.th = 0.0

        # publish at 20 Hz
        self.timer = self.create_timer(0.03, self.publish_cmd)
        
        
        
        # Publisher for RViz Marker
        self.marker_pub = self.create_publisher(Marker, '/cmd_vel_marker', 10)

        # Marker template
        self.marker = Marker()
        self.marker.header.frame_id = "base_link"  # visualize relative to robot
        self.marker.ns = "cmd_vel"
        self.marker.id = 0
        self.marker.type = Marker.ARROW
        self.marker.action = Marker.ADD
        self.marker.scale.x = 0.5  # shaft length (will scale with velocity)
        self.marker.scale.y = 0.05  # shaft diameter
        self.marker.scale.z = 0.05  # head diameter
        self.marker.color.a = 1.0
        self.marker.color.r = 1.0
        self.marker.color.g = 0.0
        self.marker.color.b = 0.0


    def cmd_vel_callback(self, msg: Twist):
        self.get_logger().info(
            f"Received cmd_vel -> Linear: [x={msg.linear.x:.2f}, y={msg.linear.y:.2f}, z={msg.linear.z:.2f}], "
            f"Angular: [x={msg.angular.x:.2f}, y={msg.angular.y:.2f}, z={msg.angular.z:.2f}]"
        )
        self.x = float(msg.linear.x)
        self.y = float(msg.linear.y)
        self.th = float(msg.angular.z)
        
        
        
        lin = msg.linear
        ang = msg.angular

        # Arrow direction in XY plane (ignore Z for visualization)
        yaw = math.atan2(lin.y, lin.x)
        mag = math.sqrt(lin.x**2 + lin.y**2)

        # Set marker orientation
        self.marker.pose.position.x = 0.0
        self.marker.pose.position.y = 0.0
        self.marker.pose.position.z = 0.0
        self.marker.pose.orientation = Quaternion(
            x=0.0,
            y=0.0,
            z=math.sin(yaw/2.0),
            w=math.cos(yaw/2.0)
        )

        # Scale arrow length according to linear speed
        self.marker.scale.x = 1.0

        # Publish marker
        self.marker.header.stamp = self.get_clock().now().to_msg()
        self.marker_pub.publish(self.marker)
        
        
    def goal_callback(self, msg: PoseStamped):
        pos = msg.pose.position
        ori = msg.pose.orientation
        self.get_logger().info(
            f"Received goal pose -> Position: [x={pos.x:.2f}, y={pos.y:.2f}, z={pos.z:.2f}] "
            f"Orientation: [x={ori.x:.2f}, y={ori.y:.2f}, z={ori.z:.2f}, w={ori.w:.2f}]"
        )
        self.start_flight = True
        self.get_logger().info("tried to start")
        self.toggle_arm()
        
        
        
    def publish_cmd(self):
        twist = geometry_msgs.msg.Twist()
        twist.linear.x = self.x
        twist.linear.y = self.y
        twist.linear.z = self.z
        twist.angular.z = self.th
        
    
        self.pub.publish(twist)

    def toggle_arm(self):
        self.arm_toggle = not self.arm_toggle
        msg = Bool()
        msg.data = self.arm_toggle
        self.arm_pub.publish(msg)
        self.get_logger().info(f"Arm toggle is now: {self.arm_toggle}")


def main():
    rclpy.init()
    node = TeleopNode()
    try:
        rclpy.spin(node)
    except Exception as e:
        print("Exception:", e)
    finally:
        node.destroy_node()
        rclpy.shutdown()
        input("Press Enter to exit...")  # Keeps the terminal open
if __name__ == '__main__':
    main()
