#!/usr/bin/env python3

import math
import time
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import TransformStamped
from nav_msgs.msg import Odometry
from px4_msgs.msg import VehicleOdometry
from tf2_ros import TransformBroadcaster
from rclpy.qos import QoSProfile, ReliabilityPolicy


class PX4OdometryNode(Node):
    def __init__(self):
        super().__init__('px4_odometry_node')
        

        # Publisher for Odometry message
        self.odom_pub = self.create_publisher(Odometry, 'odom', 10)

        # TF broadcaster
        self.tf_broadcaster = TransformBroadcaster(self)

        qos_profile = QoSProfile(depth=10)
        qos_profile.reliability = ReliabilityPolicy.BEST_EFFORT
        # Subscriber to PX4 odometry
        self.subscription = self.create_subscription(
            VehicleOdometry,
            '/fmu/out/vehicle_odometry',
            self.odometry_callback,
            qos_profile
        )

    def odometry_callback(self, msg: VehicleOdometry):
        
        print(f"[DEBUG] VehicleOdometry.pose_framehi: {msg.pose_frame}")  #ONLY SENDS 1 = NED
        now = self.get_clock().now().to_msg()

        # Convert PX4 quaternion [w, x, y, z] → ROS [x, y, z, w]
        q = msg.q
        # Extract yaw only
        yaw = math.atan2(2.0 * (q[0] * q[3] + q[1] * q[2]),
                        1.0 - 2.0 * (q[2] * q[2] + q[3] * q[3]))

        yaw = -yaw
        yaw += math.pi/2 # angle conversion 
        # Create 2D quaternion from yaw
        orientation = {
            'x': 0.0,
            'y': 0.0,
            'z': math.sin(yaw / 2.0),
            'w': math.cos(yaw / 2.0)
        }

        ros_x =  msg.position[1]        # PX4 y → ROS x
        ros_y =  msg.position[0]        # PX4 x → ROS y
        
        lin_x =  msg.velocity[1]        # PX4 y → ROS x
        lin_y =  msg.velocity[0]        # PX4 x → ROS y

        ang_z = -msg.angular_velocity[2]  # PX4 yaw rate → ROS yaw rate (negate)

        
    
        
        # Create and publish nav_msgs/Odometry message
        odom = Odometry()
        odom.header.stamp = now
        odom.header.frame_id = "odom"
        odom.child_frame_id = "base_link"

        odom.pose.pose.position.x = float(ros_x)
        odom.pose.pose.position.y = float(ros_y)
        
        odom.pose.pose.position.z = float(0)

        odom.pose.pose.orientation.x = orientation['x']
        odom.pose.pose.orientation.y = orientation['y']
        odom.pose.pose.orientation.z = orientation['z']
        odom.pose.pose.orientation.w = orientation['w']

        odom.twist.twist.linear.x = float(lin_x)
        odom.twist.twist.linear.y = float(lin_y)
        odom.twist.twist.linear.z = float(0)

        odom.twist.twist.angular.x = float(0)
        odom.twist.twist.angular.y = float(0)
        odom.twist.twist.angular.z = float(ang_z)

        self.odom_pub.publish(odom)
 
        # Broadcast transform: odom → base_link
        tf = TransformStamped()
        tf.header.stamp = now
        tf.header.frame_id = "odom"
        tf.child_frame_id = "base_link"


        tf.transform.translation.x = float(ros_x)
        tf.transform.translation.y = float(ros_y)
        tf.transform.translation.z = float(0)

        tf.transform.rotation.x = orientation['x']
        tf.transform.rotation.y = orientation['y']
        tf.transform.rotation.z = orientation['z']
        tf.transform.rotation.w = orientation['w']

        self.tf_broadcaster.sendTransform(tf)
        


def main():
    
    rclpy.init()
    node = PX4OdometryNode()
    
        # Wait for sim time to start, allowing message processing
    while node.get_clock().now().nanoseconds == 0:
        rclpy.spin_once(node, timeout_sec=0.1)
        node.get_logger().info("Waiting for simulation time to start hi...")
        
        
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()



