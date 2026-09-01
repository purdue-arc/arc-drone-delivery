"""Bridges apriltag_ros detections to MAVROS landing_target/pose."""
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, QoSReliabilityPolicy, QoSDurabilityPolicy

import tf2_ros
from tf2_ros.buffer import Buffer
from tf2_ros.transform_listener import TransformListener

from apriltag_msgs.msg import AprilTagDetectionArray
from geometry_msgs.msg import PoseStamped
from std_msgs.msg import Bool


class LandingTargetNode(Node):
    def __init__(self) -> None:
        super().__init__('landing_target_node')
        self.declare_parameter('target_tag_id', 0)
        self.declare_parameter('detection_timeout_sec', 0.5)
        self.declare_parameter('body_frame', 'base_link')
        self.declare_parameter('publish_rate_hz', 20.0)

        self.target_id = self.get_parameter('target_tag_id').value
        self.timeout = self.get_parameter('detection_timeout_sec').value
        self.body_frame = self.get_parameter('body_frame').value

        self.tf_buffer = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)

        self.create_subscription(AprilTagDetectionArray, '/detections',
                                 self._on_detections, 10)

        mavros_qos = QoSProfile(
            depth=10,
            reliability=QoSReliabilityPolicy.RELIABLE,
            durability=QoSDurabilityPolicy.VOLATILE,
        )
        self.pose_pub = self.create_publisher(
            PoseStamped, '/mavros/landing_target/pose', mavros_qos)
        self.visible_pub = self.create_publisher(
            Bool, '/arc/landing/target_visible', 10)

        self.last_seen = self.get_clock().now()
        self.last_pose = None

        rate = self.get_parameter('publish_rate_hz').value
        self.create_timer(1.0 / rate, self._tick)
        self.get_logger().info(
            f'Landing target node up. Tracking tag id={self.target_id}, '
            f'timeout={self.timeout}s')

    def _on_detections(self, msg):
        for det in msg.detections:
            if det.id != self.target_id:
                continue
            try:
                tf_msg = self.tf_buffer.lookup_transform(
                    self.body_frame, f'tag36h11:{self.target_id}',
                    rclpy.time.Time())
            except tf2_ros.TransformException as e:
                self.get_logger().warn(
                    f'TF lookup failed: {e}', throttle_duration_sec=2.0)
                return
            pose = PoseStamped()
            pose.header.stamp = self.get_clock().now().to_msg()
            pose.header.frame_id = self.body_frame
            pose.pose.position.x = tf_msg.transform.translation.x
            pose.pose.position.y = tf_msg.transform.translation.y
            pose.pose.position.z = tf_msg.transform.translation.z
            pose.pose.orientation = tf_msg.transform.rotation
            self.last_pose = pose
            self.last_seen = self.get_clock().now()
            return

    def _tick(self):
        now = self.get_clock().now()
        dt = (now - self.last_seen).nanoseconds / 1e9
        visible = dt < self.timeout and self.last_pose is not None
        vis_msg = Bool()
        vis_msg.data = visible
        self.visible_pub.publish(vis_msg)
        if visible and self.last_pose is not None:
            self.last_pose.header.stamp = now.to_msg()
            self.pose_pub.publish(self.last_pose)


def main(args=None):
    rclpy.init(args=args)
    node = LandingTargetNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
