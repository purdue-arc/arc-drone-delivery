#!/usr/bin/env python3
"""
drone_slam/slam_3d_node.py
==========================
3D occupancy mapping from the lidar, for a PX4 drone.

The 2D node next door (`slam_node.py`) flattens the world into one horizontal
slice. That is the right input for route planning, and useless for looking at
a building. This one keeps the height: it accumulates every lidar return into
a 3D voxel grid and publishes the occupied voxels as a coloured point cloud,
so RViz draws the actual shape of what the aircraft flew past.

Subscribes
----------
  /livox/points   (sensor_msgs/PointCloud2)  full 3D cloud, sensor frame

Publishes
---------
  /drone_slam_3d/map_cloud  (sensor_msgs/PointCloud2)  occupied voxel centres,
                                                       coloured by height
  /drone_slam_3d/frontier   (sensor_msgs/PointCloud2)  voxels seen this scan

TF
--
  Reads map <- sensor frame. Publishes nothing. The delivery mission already
  owns the transform chain; a second publisher of it would give one frame two
  parents and stop Nav2 planning. See `publish_tf` in slam_node.py.

WHAT THIS IS, AND WHAT IT IS NOT

This is *mapping with known poses*. The aircraft's position comes from the
flight controller's own estimator, via TF, and this node does not correct it.
Real SLAM closes the loop: it recognises somewhere it has been before and
retro-fits the whole trajectory to make the map consistent. This does not do
that, so it inherits whatever drift the position estimate has.

That is the honest description, and it is still worth having: if the lidar is
mis-mounted, mis-scaled or mis-aimed, this map shows it immediately — walls
lean, doubles appear, or the ground curves. Those are exactly the faults that
are miserable to diagnose from a live point cloud and obvious in an
accumulated map.

WHY A SPARSE GRID

A dense array over the flight volume is not affordable: 200 x 200 x 40 m at
0.2 m resolution is 40 million cells, and most of them never see a return.
Voxels are therefore held in a dictionary keyed by integer coordinates, so
memory tracks what was actually observed rather than what might be.
"""

import struct

import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.qos import (DurabilityPolicy, HistoryPolicy, QoSProfile,
                       ReliabilityPolicy)

import sensor_msgs_py.point_cloud2 as pc2
from sensor_msgs.msg import PointCloud2, PointField
from std_msgs.msg import Header

import tf2_ros


def quat_to_matrix(x: float, y: float, z: float, w: float) -> np.ndarray:
    """Rotation matrix from a quaternion. (tf_transformations is not installed.)"""
    n = x * x + y * y + z * z + w * w
    if n < 1e-12:
        return np.eye(3)
    s = 2.0 / n
    xx, yy, zz = x * x * s, y * y * s, z * z * s
    xy, xz, yz = x * y * s, x * z * s, y * z * s
    wx, wy, wz = w * x * s, w * y * s, w * z * s
    return np.array([
        [1.0 - (yy + zz), xy - wz,         xz + wy],
        [xy + wz,         1.0 - (xx + zz), yz - wx],
        [xz - wy,         yz + wx,         1.0 - (xx + yy)],
    ])


class DroneSLAM3D(Node):
    def __init__(self) -> None:
        super().__init__('drone_slam_3d')

        self.res        = self.declare_parameter('resolution', 0.4).value
        self.max_range  = self.declare_parameter('max_range', 40.0).value
        self.min_range  = self.declare_parameter('min_range', 0.6).value
        # Log-odds: a voxel needs several hits to be believed, and free-space
        # observations erode it, so a bird or a one-off reflection does not
        # leave a permanent brick in the map.
        self.l_occ      = self.declare_parameter('l_occ', 0.9).value
        self.l_free     = self.declare_parameter('l_free', -0.25).value
        self.l_max      = self.declare_parameter('l_max', 4.0).value
        self.l_min      = self.declare_parameter('l_min', -2.0).value
        self.l_thresh   = self.declare_parameter('occupied_threshold', 0.8).value
        # Free-space carving samples along each ray. Every ray, every 0.2 of
        # its length, would be exact and far too slow in Python; this is the
        # trade. Raising ray_stride costs map quality, lowering it costs CPU.
        self.ray_stride = int(self.declare_parameter('ray_stride', 3).value)
        self.ray_steps  = int(self.declare_parameter('ray_steps', 24).value)
        self.map_frame  = self.declare_parameter('map_frame', 'map').value
        self.publish_hz = self.declare_parameter('publish_hz', 2.0).value
        self.max_voxels = int(self.declare_parameter('max_voxels', 400000).value)

        # voxel key (i, j, k) -> log-odds
        self.grid: dict[tuple[int, int, int], float] = {}
        self.last_hits = np.zeros((0, 3), dtype=np.float32)

        self.tf_buffer = tf2_ros.Buffer()
        self.tf_listener = tf2_ros.TransformListener(self.tf_buffer, self)

        latched = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
        )
        self.pub_map = self.create_publisher(
            PointCloud2, '/drone_slam_3d/map_cloud', latched)
        # Reliable, not sensor-data: this is a 2 Hz overlay, not a sensor
        # stream, and a best-effort publisher silently delivers nothing to the
        # reliable subscriber RViz creates by default.
        self.pub_frontier = self.create_publisher(
            PointCloud2, '/drone_slam_3d/frontier',
            QoSProfile(reliability=ReliabilityPolicy.RELIABLE,
                       durability=DurabilityPolicy.VOLATILE,
                       history=HistoryPolicy.KEEP_LAST, depth=2))

        self.create_subscription(
            PointCloud2,
            self.declare_parameter('cloud_topic', '/livox/points').value,
            self._on_cloud,
            rclpy.qos.qos_profile_sensor_data,
        )

        self.create_timer(1.0 / self.publish_hz, self._publish_map)
        self.scans = 0
        self.dropped_no_tf = 0
        self.get_logger().info(
            f"3D mapping up — {self.res:.2f} m voxels, range "
            f"{self.min_range:.1f}..{self.max_range:.1f} m, frame '{self.map_frame}'"
        )

    # ── ingest ────────────────────────────────────────────────────────────────
    def _on_cloud(self, msg: PointCloud2) -> None:
        try:
            # Latest available rather than the cloud's own stamp: the lidar
            # bridge and the transform publishers run off different clocks, and
            # demanding an exact match drops nearly every scan.
            tf = self.tf_buffer.lookup_transform(
                self.map_frame, msg.header.frame_id, rclpy.time.Time())
        except Exception as exc:                                # noqa: BLE001
            self.dropped_no_tf += 1
            self.get_logger().warn(
                f"No transform {self.map_frame} <- {msg.header.frame_id} "
                f"({self.dropped_no_tf} scans dropped): {exc}",
                throttle_duration_sec=5.0)
            return

        pts = pc2.read_points_numpy(msg, field_names=('x', 'y', 'z'),
                                    skip_nans=True)
        if pts.size == 0:
            return
        pts = np.asarray(pts, dtype=np.float64).reshape(-1, 3)

        rng = np.linalg.norm(pts, axis=1)
        pts = pts[(rng > self.min_range) & (rng < self.max_range)]
        if pts.shape[0] == 0:
            return

        t = tf.transform.translation
        q = tf.transform.rotation
        rot = quat_to_matrix(q.x, q.y, q.z, q.w)
        origin = np.array([t.x, t.y, t.z])

        hits = pts @ rot.T + origin          # sensor frame -> map frame

        # Free space: sample along each ray from the sensor to the return.
        # Vectorised over rays and steps at once — a Python loop per ray is
        # roughly a hundred times slower and cannot keep up at 10 Hz.
        sub = hits[::self.ray_stride]
        if sub.shape[0]:
            fracs = np.linspace(0.05, 0.92, self.ray_steps)[None, :, None]
            free = origin + (sub - origin)[:, None, :] * fracs
            self._bump(free.reshape(-1, 3), self.l_free)

        self._bump(hits, self.l_occ)
        self.last_hits = hits.astype(np.float32)
        self.scans += 1

    def _bump(self, points: np.ndarray, delta: float) -> None:
        keys = np.floor(points / self.res).astype(np.int32)
        # Collapse duplicates so one scan cannot drive a voxel to saturation
        # in a single update just because many rays landed in it.
        for key in map(tuple, np.unique(keys, axis=0)):
            v = self.grid.get(key, 0.0) + delta
            self.grid[key] = max(self.l_min, min(self.l_max, v))

    # ── output ────────────────────────────────────────────────────────────────
    def _publish_map(self) -> None:
        if not self.grid:
            return

        occ = [k for k, v in self.grid.items() if v >= self.l_thresh]
        if not occ:
            return

        # Bound memory and render cost. Dropping the oldest is wrong (the map
        # would erase where it started); dropping the least certain keeps the
        # structure and sheds the noise.
        if len(self.grid) > self.max_voxels:
            self._prune()

        arr = (np.asarray(occ, dtype=np.float32) + 0.5) * self.res

        z = arr[:, 2]
        lo, hi = float(z.min()), float(z.max())
        span = max(hi - lo, 1e-3)
        rgb = self._height_colours((z - lo) / span)

        cloud = np.empty(arr.shape[0], dtype=[
            ('x', np.float32), ('y', np.float32),
            ('z', np.float32), ('rgb', np.float32)])
        cloud['x'], cloud['y'], cloud['z'] = arr[:, 0], arr[:, 1], arr[:, 2]
        cloud['rgb'] = rgb

        self.pub_map.publish(self._to_msg(cloud))

        if self.last_hits.shape[0]:
            f = np.empty(self.last_hits.shape[0], dtype=[
                ('x', np.float32), ('y', np.float32),
                ('z', np.float32), ('rgb', np.float32)])
            f['x'] = self.last_hits[:, 0]
            f['y'] = self.last_hits[:, 1]
            f['z'] = self.last_hits[:, 2]
            f['rgb'] = _pack_rgb(255, 120, 0)
            self.pub_frontier.publish(self._to_msg(f))

        self.get_logger().info(
            f"3D map: {len(occ)} occupied voxels of {len(self.grid)} observed "
            f"({self.scans} scans)", throttle_duration_sec=10.0)

    def _prune(self) -> None:
        """Evict the least CERTAIN voxels, not the least occupied.

        Sorting by log-odds descending looks right and is wrong: log-odds is
        negative for free space, so it evicts confidently-empty voxels first.
        Free-space carving then stops working as soon as the cap is reached and
        the map can only ever accumulate — observed in the 2026-09-02 flight,
        which hit the cap partway through and kept growing.

        Sorting by |log-odds| keeps everything the map is sure about, in either
        direction, and discards the maybes.
        """
        keep = sorted(self.grid.items(), key=lambda kv: -abs(kv[1]))[:self.max_voxels]
        self.grid = dict(keep)

    @staticmethod
    def _height_colours(norm: np.ndarray) -> np.ndarray:
        """Deep blue (low) through cyan and green to warm yellow (high)."""
        n = np.clip(norm, 0.0, 1.0)
        r = np.clip(1.6 * n - 0.35, 0, 1)
        g = np.clip(1.5 * n + 0.10, 0, 1)
        b = np.clip(1.15 - 1.5 * n, 0, 1)
        packed = ((r * 255).astype(np.uint32) << 16 |
                  (g * 255).astype(np.uint32) << 8 |
                  (b * 255).astype(np.uint32))
        return packed.view(np.float32) if packed.dtype == np.float32 else \
            np.frombuffer(packed.astype(np.uint32).tobytes(), dtype=np.float32)

    def _to_msg(self, cloud: np.ndarray) -> PointCloud2:
        header = Header()
        header.stamp = self.get_clock().now().to_msg()
        header.frame_id = self.map_frame
        msg = PointCloud2()
        msg.header = header
        msg.height = 1
        msg.width = cloud.shape[0]
        msg.fields = [
            PointField(name='x', offset=0, datatype=PointField.FLOAT32, count=1),
            PointField(name='y', offset=4, datatype=PointField.FLOAT32, count=1),
            PointField(name='z', offset=8, datatype=PointField.FLOAT32, count=1),
            PointField(name='rgb', offset=12, datatype=PointField.FLOAT32, count=1),
        ]
        msg.is_bigendian = False
        msg.point_step = 16
        msg.row_step = 16 * cloud.shape[0]
        msg.is_dense = True
        msg.data = cloud.tobytes()
        return msg


def _pack_rgb(r: int, g: int, b: int) -> float:
    return struct.unpack('f', struct.pack('I', (r << 16) | (g << 8) | b))[0]


def main(args=None) -> None:
    rclpy.init(args=args)
    node = DroneSLAM3D()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
