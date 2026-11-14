// gz_lidar_bridge — Gazebo Harmonic gpu_lidar -> sensor_msgs/PointCloud2.
//
// The Harmonic counterpart of navigation-stack/gazebo_scan_bridge (Gazebo
// Classic). It stands in for the Livox Mid-360 driver in simulation: same
// message type, same topic (/livox/points), same frame (livox_frame), so
// Nav2, flight_level_filter, cloud_to_scan and the SLAM nodes cannot tell
// SITL from the aircraft.
//
// Why not ros_gz_bridge, which is already in the image: it would publish
// gz.msgs.PointCloudPacked straight through, and that is wrong for this stack
// in two ways the costmap does not complain about.
//
//   stamps  ros_gz_bridge copies the Gazebo SIM time into the header. The ROS
//           side runs use_sim_time:=false (PX4 over uXRCE-DDS publishes no
//           /clock), so every cloud would look stale and be dropped. This
//           node stamps ROS wall time, as gazebo_scan_bridge did.
//   no-hit  Gazebo reports a ray that hit nothing as +inf. Passed through,
//           those either poison the filters or, clamped, ring the aircraft
//           in phantom obstacles at max range. They are dropped here, along
//           with returns closer than min_range (the airframe itself).
//
// Unlike gazebo_scan_bridge this builds in the arc-drone:jazzy container:
// Jazzy's gz vendor packages ship the Harmonic transport libraries, so the
// host no longer needs ROS 2 Humble. It talks to the host's Gazebo over
// gz-transport; with network_mode: host both sides must set GZ_IP=127.0.0.1.
//
//   ros2 run gz_lidar_bridge gz_lidar_bridge

#include <gz/msgs/laserscan.pb.h>
#include <gz/transport/Node.hh>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/point_cloud2_iterator.hpp>

#include <cmath>
#include <memory>
#include <string>

class GzLidarBridge : public rclcpp::Node
{
public:
  GzLidarBridge() : Node("gz_lidar_bridge")
  {
    // Fixed in sim/gz/models/arc_x500/model.sdf, so it does not depend on
    // the world or model name the way the Classic topic did.
    gz_topic_    = declare_parameter<std::string>("gz_topic", "/arc/lidar/scan");
    frame_id_    = declare_parameter<std::string>("frame_id", "livox_frame");
    cloud_topic_ = declare_parameter<std::string>("cloud_topic", "/livox/points");
    // Drop returns closer than this so the airframe itself is not an obstacle.
    min_range_   = declare_parameter("min_range", 0.5);
    // The render engine Gazebo's sensors use (run_sitl.sh GZ_RENDER_ENGINE).
    // Ogre 1.9's gpu_lidar returns every ray one sample off in BOTH axes:
    // ray (v, h) actually travels along the nominal direction of (v+1, h+1).
    // Measured against ogre2 on arc_x500 (32 x 360 rays): flat ground matched
    // only with the row shifted by one (mean range error 2.78 m -> 0.08 m),
    // and a box at a known bearing appeared one beam early. Uncorrected,
    // every obstacle sits 1.9 deg low and 1 deg off in azimuth.
    render_engine_ = declare_parameter<std::string>("render_engine", "ogre2");
    if (render_engine_ == "ogre") {
      sample_offset_ = 1;
    } else if (render_engine_ != "ogre2") {
      RCLCPP_WARN(get_logger(), "Unknown render_engine '%s'; treating it as "
                  "ogre2 (no ray correction)", render_engine_.c_str());
    }

    pub_ = create_publisher<sensor_msgs::msg::PointCloud2>(
      cloud_topic_, rclcpp::SensorDataQoS());

    RCLCPP_INFO(get_logger(), "Bridging gz '%s' -> ROS '%s' (frame %s, "
                "render engine %s, ray offset %d)",
                gz_topic_.c_str(), cloud_topic_.c_str(), frame_id_.c_str(),
                render_engine_.c_str(), sample_offset_);
  }

  bool connect()
  {
    if (!node_.Subscribe(gz_topic_, &GzLidarBridge::on_scan, this)) {
      RCLCPP_ERROR(get_logger(), "Could not subscribe to '%s'. Is Gazebo "
                   "running with arc_x500, and is GZ_IP the same on both "
                   "sides? Check `gz topic -l`.", gz_topic_.c_str());
      return false;
    }
    return true;
  }

private:
  // Called on a gz-transport thread; rclcpp publishers are thread-safe.
  void on_scan(const gz::msgs::LaserScan & s)
  {
    const int nh = static_cast<int>(s.count());
    const int nv = s.vertical_count() > 0 ? static_cast<int>(s.vertical_count()) : 1;

    sensor_msgs::msg::PointCloud2 out;
    // ROS wall time, not Gazebo sim time: see the header.
    out.header.stamp = now();
    out.header.frame_id = frame_id_;
    out.height = 1;
    out.is_dense = false;
    out.is_bigendian = false;

    sensor_msgs::PointCloud2Modifier mod(out);
    mod.setPointCloud2FieldsByString(1, "xyz");
    mod.resize(static_cast<size_t>(nh) * nv);

    sensor_msgs::PointCloud2Iterator<float> ix(out, "x");
    sensor_msgs::PointCloud2Iterator<float> iy(out, "y");
    sensor_msgs::PointCloud2Iterator<float> iz(out, "z");

    const double a0   = s.angle_min();
    const double da   = s.angle_step();
    const double va0  = s.vertical_angle_min();
    const double dva  = nv > 1 ? s.vertical_angle_step() : 0.0;
    const double rmax = s.range_max();

    size_t kept = 0;
    for (int v = 0; v < nv; ++v) {
      const double pitch = va0 + (v + sample_offset_) * dva;
      for (int h = 0; h < nh; ++h) {
        // Row-major, one horizontal sweep per vertical ring, as in Classic.
        const int idx = v * nh + h;
        if (idx >= s.ranges_size()) break;
        const double r = s.ranges(idx);
        if (!std::isfinite(r) || r >= rmax || r < min_range_) continue;

        const double yaw = a0 + (h + sample_offset_) * da;
        *ix = static_cast<float>(r * std::cos(pitch) * std::cos(yaw));
        *iy = static_cast<float>(r * std::cos(pitch) * std::sin(yaw));
        *iz = static_cast<float>(r * std::sin(pitch));
        ++ix; ++iy; ++iz; ++kept;
      }
    }
    mod.resize(kept);
    out.width = kept;
    out.row_step = out.point_step * kept;

    pub_->publish(out);
    if (++count_ % 50 == 1) {
      RCLCPP_INFO(get_logger(), "cloud #%lu: %zu points (%dx%d rays)",
                  count_, kept, nh, nv);
    }
  }

  std::string gz_topic_, frame_id_, cloud_topic_, render_engine_;
  double min_range_{0.5};
  int sample_offset_{0};
  gz::transport::Node node_;
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr pub_;
  unsigned long count_{0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<GzLidarBridge>();
  if (!node->connect()) {
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
