#!/usr/bin/env python3
"""
drone_slam/slam_node.py
=======================
Lightweight 2D occupancy-grid SLAM for a PX4 drone.

Subscribes
----------
  /fmu/out/vehicle_local_position  (px4_msgs/VehicleLocalPosition)
  /scan                            (sensor_msgs/LaserScan)

Publishes
---------
  /drone_slam/map          (nav_msgs/OccupancyGrid)   – accumulated 2D map
  /drone_slam/path         (nav_msgs/Path)             – drone trajectory (green in RViz)
  /drone_slam/lidar_points (sensor_msgs/PointCloud2)   – live LiDAR hits in world frame (cyan)

TF
--
  map → odom → base_link   (broadcast at position-update rate)

Run standalone (separate terminal):
  ros2 launch drone_slam slam.launch.py
"""

import math
import struct

import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.qos import (DurabilityPolicy, HistoryPolicy, QoSProfile,
                        ReliabilityPolicy)

from geometry_msgs.msg import PoseStamped, TransformStamped
from nav_msgs.msg import OccupancyGrid, Path
from px4_msgs.msg import VehicleLocalPosition
from sensor_msgs.msg import LaserScan, PointCloud2, PointField
from tf2_ros import TransformBroadcaster

# ── Tunable constants ─────────────────────────────────────────────────────────
MAP_SIZE_M     = 200.0   # total map extent (metres); centred on origin
RESOLUTION     = 0.2     # metres per grid cell
MAX_RANGE_M    = 20.0    # ignore LiDAR returns beyond this
L_OCC          =  0.85   # log-odds increment for an occupied cell
L_FREE         = -0.40   # log-odds increment for a free cell
L_MAX          =  3.5    # log-odds ceiling
L_MIN          = -2.0    # log-odds floor
PATH_MIN_DIST  =  0.10   # minimum metres between successive path waypoints
SCAN_STRIDE    =  2      # process every Nth ray (lowers CPU on dense scans)
MAP_PUB_HZ     =  2.0    # how often to serialise + publish the full map

# Cyan packed as a float (for the PointCloud2 "rgb" field that RViz reads)
_CYAN_PACKED = struct.unpack('f', struct.pack('I', (0 << 16) | (210 << 8) | 255))[0]
# ─────────────────────────────────────────────────────────────────────────────


class DroneSLAM(Node):
    """2-D log-odds occupancy grid SLAM node."""

    def __init__(self) -> None:
        super().__init__('drone_slam')

        # ── Occupancy grid ────────────────────────────────────────────────────
        self.res     = RESOLUTION
        self.grid_n  = int(MAP_SIZE_M / self.res)            # cells per side
        self.log_odds = np.zeros((self.grid_n, self.grid_n), dtype=np.float32)
        # Map origin (bottom-left corner in ENU metres)
        self.map_ox = -MAP_SIZE_M / 2.0
        self.map_oy = -MAP_SIZE_M / 2.0

        # ── Drone state (ENU) ─────────────────────────────────────────────────
        self.drone_x   = 0.0   # metres East
        self.drone_y   = 0.0   # metres North
        self.drone_z   = 0.0   # metres Up
        self.drone_yaw = 0.0   # radians CCW from East
        self.has_pos   = False

        # ── Path bookkeeping ──────────────────────────────────────────────────
        self.path_msg      = Path()
        self.path_msg.header.frame_id = 'map'
        self.last_path_xy  = None          # (x, y) of last appended pose

        # ── QoS: PX4 uses BEST_EFFORT + TRANSIENT_LOCAL ───────────────────────
        px4_qos = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
        )

        # ── Parameters ────────────────────────────────────────────────────────
        # PX4 appends a version suffix to the topic name for messages that
        # carry a versioned definition. VehicleLocalPosition is one of them, so
        # the bare name receives nothing at all — the node runs, reports no
        # error, and simply never sees the aircraft move. Verify against the
        # aircraft with:  ros2 topic list | grep fmu/out
        pos_topic = self.declare_parameter(
            'position_topic', '/fmu/out/vehicle_local_position_v1'
        ).value

        # Whether to broadcast the map->odom->base_link transform chain.
        #
        # FALSE when running inside the delivery mission. That stack already
        # publishes map->odom (static) and odom->base_footprint->base_link, so
        # broadcasting here would give map->odom two publishers fighting each
        # other AND give base_link two different parents, which is an invalid
        # transform tree — Nav2 stops planning entirely.
        #
        # TRUE when running this node standalone, where nothing else supplies
        # the chain.
        self.publish_tf = self.declare_parameter('publish_tf', True).value

        # ── Subscribers ───────────────────────────────────────────────────────
        self.create_subscription(
            VehicleLocalPosition,
            pos_topic,
            self._cb_position,
            px4_qos,
        )
        self.get_logger().info(
            f"drone_slam up — pose from '{pos_topic}', scans from '/scan', "
            f"publish_tf={self.publish_tf}"
        )
        self.create_subscription(
            LaserScan,
            '/scan',
            self._cb_scan,
            rclpy.qos.qos_profile_sensor_data,
        )

        # ── Publishers ────────────────────────────────────────────────────────
        # Latched (transient local), the convention for map topics: an RViz or
        # any other viewer that connects late immediately receives the current
        # map instead of waiting for the next 2 Hz publish. Nav2's costmap
        # topics behave the same way, so a viewer configured for one works for
        # the other. A volatile publisher here silently delivers nothing to a
        # subscriber that asks for transient local.
        map_qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
        )
        self.pub_map   = self.create_publisher(OccupancyGrid, '/drone_slam/map', map_qos)
        self.pub_path  = self.create_publisher(Path,          '/drone_slam/path',         10)
        self.pub_cloud = self.create_publisher(PointCloud2,   '/drone_slam/lidar_points', 10)

        # ── TF ────────────────────────────────────────────────────────────────
        self.tf_br = TransformBroadcaster(self)

        # ── Timers ────────────────────────────────────────────────────────────
        self.create_timer(1.0 / MAP_PUB_HZ, self._publish_map)

        self.get_logger().info(
            f'Drone SLAM started  |  map {MAP_SIZE_M:.0f} m × {MAP_SIZE_M:.0f} m'
            f'  @  {self.res:.2f} m/cell  ({self.grid_n}×{self.grid_n})'
        )

    # ── Position callback ─────────────────────────────────────────────────────

    def _cb_position(self, msg: VehicleLocalPosition) -> None:
        """
        PX4 publishes position in NED (x=North, y=East, z=Down).
        Convert to ENU (x=East, y=North, z=Up) for ROS.

        The `heading` field is the yaw from North, clockwise (NED convention).
        In ENU: yaw_enu = π/2 − heading_ned
        """
        if not (msg.xy_valid and msg.z_valid):
            return

        self.drone_x   =  msg.y          # East
        self.drone_y   =  msg.x          # North
        self.drone_z   = -msg.z          # Up
        self.drone_yaw = math.pi / 2.0 - msg.heading   # ENU yaw (CCW from East)
        self.has_pos   = True

        self._maybe_append_path()
        self._broadcast_tf()

    # ── LiDAR callback ────────────────────────────────────────────────────────

    def _cb_scan(self, msg: LaserScan) -> None:
        """
        Project 2-D LaserScan rays into the world (map) frame using the current
        drone position + yaw, update the occupancy grid via Bresenham ray-casting,
        and publish a coloured PointCloud2 of the live LiDAR hits.
        """
        if not self.has_pos:
            return

        cx, cy = self.drone_x, self.drone_y
        cos_y  = math.cos(self.drone_yaw)
        sin_y  = math.sin(self.drone_yaw)

        world_hits: list[tuple[float, float]] = []
        ranges = msg.ranges

        for i in range(0, len(ranges), SCAN_STRIDE):
            r = ranges[i]
            if not (msg.range_min < r < min(msg.range_max, MAX_RANGE_M)):
                continue

            # Angle in sensor frame (assumed aligned with drone body → yaw correction only)
            a  = msg.angle_min + i * msg.angle_increment
            lx = r * math.cos(a)   # local sensor frame
            ly = r * math.sin(a)

            # Rotate into world frame
            wx = cx + cos_y * lx - sin_y * ly
            wy = cy + sin_y * lx + cos_y * ly

            world_hits.append((wx, wy))
            self._raytrace(cx, cy, wx, wy)

        if world_hits:
            self._publish_cloud(world_hits, msg.header.stamp)

    # ── Occupancy grid ────────────────────────────────────────────────────────

    def _w2g(self, wx: float, wy: float) -> tuple[int, int]:
        """Convert world-frame (ENU) metres → grid cell indices."""
        return (
            int((wx - self.map_ox) / self.res),
            int((wy - self.map_oy) / self.res),
        )

    def _raytrace(self, x0: float, y0: float, x1: float, y1: float) -> None:
        """
        Bresenham line from drone position to LiDAR hit:
          • cells along the ray → mark FREE (l_free)
          • endpoint cell       → mark OCCUPIED (l_occ)
        """
        gx0, gy0 = self._w2g(x0, y0)
        gx1, gy1 = self._w2g(x1, y1)
        n = self.grid_n

        dx = abs(gx1 - gx0);  dy = abs(gy1 - gy0)
        sx = 1 if gx0 < gx1 else -1
        sy = 1 if gy0 < gy1 else -1
        err = dx - dy
        x, y = gx0, gy0

        while True:
            if x == gx1 and y == gy1:
                break
            if 0 <= x < n and 0 <= y < n:
                v = self.log_odds[y, x] + L_FREE
                self.log_odds[y, x] = v if v > L_MIN else L_MIN
            e2 = err * 2
            if e2 > -dy:
                err -= dy; x += sx
            if e2 <  dx:
                err += dx; y += sy

        # Endpoint → occupied
        if 0 <= gx1 < n and 0 <= gy1 < n:
            v = self.log_odds[gy1, gx1] + L_OCC
            self.log_odds[gy1, gx1] = v if v < L_MAX else L_MAX

    # ── Map publisher ─────────────────────────────────────────────────────────

    def _publish_map(self) -> None:
        """Serialise log-odds grid → OccupancyGrid and publish."""
        now = self.get_clock().now().to_msg()

        msg = OccupancyGrid()
        msg.header.stamp    = now
        msg.header.frame_id = 'map'
        msg.info.resolution = self.res
        msg.info.width      = self.grid_n
        msg.info.height     = self.grid_n
        msg.info.origin.position.x = self.map_ox
        msg.info.origin.position.y = self.map_oy
        msg.info.origin.orientation.w = 1.0

        # Convert log-odds to OccupancyGrid values:
        #   -1  → unknown  (|log-odds| < threshold)
        #   0   → free
        #   100 → occupied
        lo   = self.log_odds
        prob = (1.0 / (1.0 + np.exp(-lo)) * 100).astype(np.int8)
        prob[np.abs(lo) < 0.05] = -1          # unknown cells
        msg.data = prob.flatten().tolist()

        self.pub_map.publish(msg)

    # ── Path publisher ────────────────────────────────────────────────────────

    def _maybe_append_path(self) -> None:
        """Append current drone position to the path if it moved far enough."""
        px, py = self.drone_x, self.drone_y

        if self.last_path_xy is not None:
            lx, ly = self.last_path_xy
            if math.hypot(px - lx, py - ly) < PATH_MIN_DIST:
                return

        now = self.get_clock().now().to_msg()
        ps  = PoseStamped()
        ps.header.stamp    = now
        ps.header.frame_id = 'map'
        ps.pose.position.x = px
        ps.pose.position.y = py
        ps.pose.position.z = self.drone_z

        # Yaw-only quaternion: q = (cos(yaw/2), 0, 0, sin(yaw/2))
        half = self.drone_yaw / 2.0
        ps.pose.orientation.w = math.cos(half)
        ps.pose.orientation.z = math.sin(half)

        self.path_msg.poses.append(ps)
        self.path_msg.header.stamp = now
        self.pub_path.publish(self.path_msg)
        self.last_path_xy = (px, py)

    # ── LiDAR cloud publisher ─────────────────────────────────────────────────

    def _publish_cloud(
        self,
        pts: list[tuple[float, float]],
        stamp,
    ) -> None:
        """
        Publish live LiDAR hits (world frame) as a PointCloud2 with RGB colour
        (cyan) so RViz can display them in a distinct colour from the grey map.
        """
        fields = [
            PointField(name='x',   offset=0,  datatype=PointField.FLOAT32, count=1),
            PointField(name='y',   offset=4,  datatype=PointField.FLOAT32, count=1),
            PointField(name='z',   offset=8,  datatype=PointField.FLOAT32, count=1),
            PointField(name='rgb', offset=12, datatype=PointField.FLOAT32, count=1),
        ]
        POINT_STEP = 16
        z = float(self.drone_z)
        buf = bytearray(len(pts) * POINT_STEP)
        for i, (px, py) in enumerate(pts):
            struct.pack_into('ffff', buf, i * POINT_STEP, px, py, z, _CYAN_PACKED)

        cloud = PointCloud2()
        cloud.header.stamp    = stamp
        cloud.header.frame_id = 'map'
        cloud.height          = 1
        cloud.width           = len(pts)
        cloud.fields          = fields
        cloud.is_bigendian    = False
        cloud.point_step      = POINT_STEP
        cloud.row_step        = POINT_STEP * len(pts)
        cloud.is_dense        = True
        cloud.data            = bytes(buf)
        self.pub_cloud.publish(cloud)

    # ── TF broadcaster ────────────────────────────────────────────────────────

    def _broadcast_tf(self) -> None:
        """
        Publish the TF chain:  map → odom (identity) → base_link (drone pose).

        Keeping map→odom as identity simplifies things; a proper SLAM back-end
        would update the map→odom transform when loop closures are detected.
        """
        if not self.publish_tf:
            return

        now = self.get_clock().now().to_msg()

        # map → odom  (no correction yet; identity)
        t_mo = TransformStamped()
        t_mo.header.stamp    = now
        t_mo.header.frame_id = 'map'
        t_mo.child_frame_id  = 'odom'
        t_mo.transform.rotation.w = 1.0

        # odom → base_link  (drone pose in ENU)
        half = self.drone_yaw / 2.0
        t_ob = TransformStamped()
        t_ob.header.stamp       = now
        t_ob.header.frame_id    = 'odom'
        t_ob.child_frame_id     = 'base_link'
        t_ob.transform.translation.x = self.drone_x
        t_ob.transform.translation.y = self.drone_y
        t_ob.transform.translation.z = self.drone_z
        t_ob.transform.rotation.w    = math.cos(half)
        t_ob.transform.rotation.z    = math.sin(half)

        self.tf_br.sendTransform([t_mo, t_ob])


# ── Entry point ───────────────────────────────────────────────────────────────

def main(args=None) -> None:
    rclpy.init(args=args)
    node = DroneSLAM()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
