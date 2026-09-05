"""Static TF: base_link -> camera_down matching the downward camera mount."""
import math
import rclpy
from rclpy.node import Node
from tf2_ros import StaticTransformBroadcaster
from geometry_msgs.msg import TransformStamped


class TagTFBroadcaster(Node):
    def __init__(self) -> None:
        super().__init__('tag_tf_broadcaster')
        self.broadcaster = StaticTransformBroadcaster(self)
        self._publish_camera_tf()
        self.get_logger().info('Static camera_down TF published')

    def _publish_camera_tf(self) -> None:
        t = TransformStamped()
        t.header.stamp = self.get_clock().now().to_msg()
        t.header.frame_id = 'base_link'
        t.child_frame_id = 'camera_down'
        t.transform.translation.x = 0.0
        t.transform.translation.y = 0.0
        t.transform.translation.z = -0.1
        pitch = math.pi / 2.0
        cp = math.cos(pitch * 0.5)
        sp = math.sin(pitch * 0.5)
        t.transform.rotation.w = cp
        t.transform.rotation.x = 0.0
        t.transform.rotation.y = sp
        t.transform.rotation.z = 0.0
        self.broadcaster.sendTransform(t)


def main(args=None) -> None:
    rclpy.init(args=args)
    node = TagTFBroadcaster()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
