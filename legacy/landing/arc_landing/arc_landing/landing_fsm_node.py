"""Mission FSM: IDLE -> TAKEOFF -> SEARCH -> APPROACH -> DESCEND -> LANDED."""
import math
import enum
from dataclasses import dataclass

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, QoSReliabilityPolicy, QoSDurabilityPolicy

from geometry_msgs.msg import PoseStamped
from std_msgs.msg import Bool
from mavros_msgs.msg import State
from mavros_msgs.srv import CommandBool, SetMode


class FsmState(enum.Enum):
    IDLE = 0
    TAKEOFF = 1
    SEARCH = 2
    APPROACH = 3
    DESCEND = 4
    LANDED = 5
    ABORT = 6


@dataclass
class Params:
    cruise_altitude_m: float = 5.0
    approach_altitude_m: float = 3.0
    descend_rate_m_per_s: float = 0.5
    touchdown_altitude_m: float = 0.3
    search_radius_m: float = 4.0
    approach_threshold_m: float = 0.3
    takeoff_tolerance_m: float = 0.3
    offboard_rate_hz: float = 20.0


class LandingFSM(Node):
    def __init__(self):
        super().__init__('landing_fsm_node')
        self.params = Params()
        self.mavros_state = None
        self.current_pose = None
        self.tag_visible = False
        self.tag_pose_body = None

        qos_best_effort = QoSProfile(
            depth=10,
            reliability=QoSReliabilityPolicy.BEST_EFFORT,
            durability=QoSDurabilityPolicy.VOLATILE,
        )
        self.create_subscription(State, '/mavros/state', self._on_state, 10)
        self.create_subscription(PoseStamped, '/mavros/local_position/pose',
                                 self._on_pose, qos_best_effort)
        self.create_subscription(Bool, '/arc/landing/target_visible',
                                 self._on_tag_visible, 10)
        self.create_subscription(PoseStamped, '/mavros/landing_target/pose',
                                 self._on_tag_pose, 10)

        self.sp_pub = self.create_publisher(
            PoseStamped, '/mavros/setpoint_position/local', 10)
        self.arming_cli = self.create_client(CommandBool, '/mavros/cmd/arming')
        self.mode_cli = self.create_client(SetMode, '/mavros/set_mode')

        self.state = FsmState.IDLE
        self.state_entered_at = self.get_clock().now()
        self.create_timer(1.0 / self.params.offboard_rate_hz, self._tick)
        self.get_logger().info('Landing FSM initialized, waiting for MAVROS...')

    def _on_state(self, msg): self.mavros_state = msg
    def _on_pose(self, msg): self.current_pose = msg
    def _on_tag_visible(self, msg): self.tag_visible = msg.data
    def _on_tag_pose(self, msg): self.tag_pose_body = msg

    def _transition(self, new_state):
        if new_state != self.state:
            self.get_logger().info(f'FSM: {self.state.name} -> {new_state.name}')
            self.state = new_state
            self.state_entered_at = self.get_clock().now()

    def _time_in_state(self):
        return (self.get_clock().now() - self.state_entered_at).nanoseconds / 1e9

    def _publish_setpoint(self, x, y, z):
        sp = PoseStamped()
        sp.header.stamp = self.get_clock().now().to_msg()
        sp.header.frame_id = 'map'
        sp.pose.position.x = x
        sp.pose.position.y = y
        sp.pose.position.z = z
        sp.pose.orientation.w = 1.0
        self.sp_pub.publish(sp)

    def _current_xyz(self):
        if self.current_pose is None:
            return (0.0, 0.0, 0.0)
        p = self.current_pose.pose.position
        return (p.x, p.y, p.z)

    def _tick(self):
        if self.mavros_state is None or self.current_pose is None:
            return
        x, y, z = self._current_xyz()

        if self.state in (FsmState.TAKEOFF, FsmState.SEARCH,
                          FsmState.APPROACH, FsmState.DESCEND):
            if self.mavros_state.mode != 'OFFBOARD':
                self._request_offboard()
            if not self.mavros_state.armed:
                self._request_arm()

        if self.state == FsmState.IDLE:
            self._publish_setpoint(x, y, self.params.cruise_altitude_m)
            if self._time_in_state() > 2.0:
                self._transition(FsmState.TAKEOFF)
        elif self.state == FsmState.TAKEOFF:
            self._publish_setpoint(0.0, 0.0, self.params.cruise_altitude_m)
            if abs(z - self.params.cruise_altitude_m) < self.params.takeoff_tolerance_m:
                self._transition(FsmState.SEARCH)
        elif self.state == FsmState.SEARCH:
            t = self._time_in_state()
            angle = 0.3 * t
            cx = self.params.search_radius_m * math.cos(angle)
            cy = self.params.search_radius_m * math.sin(angle)
            self._publish_setpoint(cx, cy, self.params.cruise_altitude_m)
            if self.tag_visible and self.tag_pose_body is not None:
                self._transition(FsmState.APPROACH)
        elif self.state == FsmState.APPROACH:
            if not self.tag_visible or self.tag_pose_body is None:
                self.get_logger().warn('Lost tag in APPROACH -> SEARCH',
                                       throttle_duration_sec=2.0)
                self._transition(FsmState.SEARCH)
                return
            tp = self.tag_pose_body.pose.position
            target_x = x + tp.x
            target_y = y + tp.y
            horizontal_err = math.hypot(tp.x, tp.y)
            self._publish_setpoint(target_x, target_y, self.params.approach_altitude_m)
            if horizontal_err < self.params.approach_threshold_m:
                self._transition(FsmState.DESCEND)
        elif self.state == FsmState.DESCEND:
            if not self.tag_visible:
                self.get_logger().warn('Lost tag during DESCEND -> ABORT',
                                       throttle_duration_sec=2.0)
                self._transition(FsmState.ABORT)
                return
            tp = self.tag_pose_body.pose.position
            target_x = x + tp.x
            target_y = y + tp.y
            target_z = max(
                self.params.touchdown_altitude_m,
                z - self.params.descend_rate_m_per_s / self.params.offboard_rate_hz,
            )
            self._publish_setpoint(target_x, target_y, target_z)
            if z < self.params.touchdown_altitude_m + 0.1:
                self._transition(FsmState.LANDED)
        elif self.state == FsmState.LANDED:
            self._request_land()
        elif self.state == FsmState.ABORT:
            self._publish_setpoint(x, y, self.params.cruise_altitude_m)
            if z > self.params.cruise_altitude_m - self.params.takeoff_tolerance_m:
                self._transition(FsmState.SEARCH)

    def _request_offboard(self):
        if not self.mode_cli.service_is_ready():
            return
        req = SetMode.Request()
        req.custom_mode = 'OFFBOARD'
        self.mode_cli.call_async(req)

    def _request_arm(self):
        if not self.arming_cli.service_is_ready():
            return
        req = CommandBool.Request()
        req.value = True
        self.arming_cli.call_async(req)

    def _request_land(self):
        if not self.mode_cli.service_is_ready():
            return
        req = SetMode.Request()
        req.custom_mode = 'AUTO.LAND'
        self.mode_cli.call_async(req)


def main(args=None):
    rclpy.init(args=args)
    node = LandingFSM()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
