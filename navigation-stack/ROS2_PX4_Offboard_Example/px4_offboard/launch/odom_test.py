import rclpy
from rclpy.node import Node
from geometry_msgs.msg import TransformStamped
from nav_msgs.msg import Odometry
from tf2_ros import TransformBroadcaster
import math
import time

class SimpleOdometry(Node):
    def __init__(self):
        super().__init__('simple_odometry')

        self.odom_pub = self.create_publisher(Odometry, 'odom', 10)
        self.tf_broadcaster = TransformBroadcaster(self)

        self.timer = self.create_timer(0.05, self.update)  # 20 Hz
        self.x = 0.0
        self.y = 0.0
        self.th = 0.0
        self.vx = 0.1  # m/s
        self.vth = 0.1  # rad/s
        self.last_time = self.get_clock().now()

    def update(self):
        now = self.get_clock().now()
        dt = (now - self.last_time).nanoseconds * 1e-9
        self.last_time = now

        # Integrate
        delta_x = self.vx * math.cos(self.th) * dt
        delta_y = self.vx * math.sin(self.th) * dt
        delta_th = self.vth * dt

        self.x += delta_x
        self.y += delta_y
        self.th += delta_th

        # Create Odometry message
        odom = Odometry()
        odom.header.stamp = now.to_msg()
        odom.header.frame_id = "odom"
        odom.child_frame_id = "base_link"

        odom.pose.pose.position.x = self.x
        odom.pose.pose.position.y = self.y
        odom.pose.pose.orientation.z = math.sin(self.th / 2.0)
        odom.pose.pose.orientation.w = math.cos(self.th / 2.0)

        odom.twist.twist.linear.x = self.vx
        odom.twist.twist.angular.z = self.vth

        # Publish Odometry
        self.odom_pub.publish(odom)

        # Broadcast TF
        tf = TransformStamped()
        tf.header.stamp = now.to_msg()
        tf.header.frame_id = "odom"
        tf.child_frame_id = "base_link"
        tf.transform.translation.x = self.x
        tf.transform.translation.y = self.y
        tf.transform.translation.z = 0.0
        tf.transform.rotation = odom.pose.pose.orientation

        self.tf_broadcaster.sendTransform(tf)
        
        
        

def main():
    rclpy.init()
    node = SimpleOdometry()
    rclpy.spin(node)
    rclpy.shutdown()

if __name__ == '__main__':
    main()

