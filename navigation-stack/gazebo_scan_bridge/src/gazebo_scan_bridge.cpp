// gazebo_scan_bridge — Gazebo Classic 3D ray sensor -> sensor_msgs/PointCloud2.
//
// Stands in for the Livox Mid-360 driver in simulation. The real aircraft's
// Mid-360 publishes a PointCloud2 from the Livox ROS driver; this node
// publishes the same message type on the same topic so Nav2's costmap
// configuration is identical in SITL and in flight.
//
// Why a bridge at all: gazebo_ros_pkgs (libgazebo_ros_ray_sensor.so) is not
// installed and was never released for Gazebo Classic on ROS 2 Jazzy —
// Classic reached end-of-life before Jazzy. Without it there is no path from
// a Gazebo Classic sensor into ROS 2, which is also why the typhoon's depth
// camera publishes nothing. A Gazebo <sensor type="ray"> does publish on
// gazebo transport with no plugin, so this node subscribes there and
// republishes.
//
// Builds against ROS 2 HUMBLE on the host, because that is where the gazebo11
// development headers live. The rest of the stack runs Jazzy in the
// container; PointCloud2 is wire-compatible across those distros.
//
//   source /opt/ros/humble/setup.bash
//   ros2 run gazebo_scan_bridge gazebo_scan_bridge

#include <gazebo/transport/transport.hh>
#include <gazebo/msgs/msgs.hh>
#include <gazebo/gazebo_client.hh>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/point_cloud2_iterator.hpp>

#include <cmath>
#include <limits>
#include <memory>
#include <string>

class GazeboScanBridge : public rclcpp::Node
{
public:
  GazeboScanBridge() : Node("gazebo_scan_bridge")
  {
    gazebo_topic_ = declare_parameter<std::string>(
      "gazebo_topic",
      "/gazebo/apriltag_world/typhoon_h480/base_link/lidar/scan");
    frame_id_    = declare_parameter<std::string>("frame_id", "base_link");
    cloud_topic_ = declare_parameter<std::string>("cloud_topic", "/livox/points");
    // Drop returns closer than this so the airframe itself is not an obstacle.
    min_range_   = declare_parameter("min_range", 0.5);

    pub_ = create_publisher<sensor_msgs::msg::PointCloud2>(
      cloud_topic_, rclcpp::SensorDataQoS());

    RCLCPP_INFO(get_logger(), "Bridging gazebo '%s' -> ROS '%s' (frame %s)",
                gazebo_topic_.c_str(), cloud_topic_.c_str(), frame_id_.c_str());
  }

  bool connect()
  {
    gazebo::client::setup();
    node_ = gazebo::transport::NodePtr(new gazebo::transport::Node());
    node_->Init();
    sub_ = node_->Subscribe(gazebo_topic_, &GazeboScanBridge::on_scan, this);
    if (!sub_) {
      RCLCPP_ERROR(get_logger(), "Could not subscribe to '%s'. Is gzserver "
                   "running and does the sensor exist? Check `gz topic -l`.",
                   gazebo_topic_.c_str());
      return false;
    }
    return true;
  }

  ~GazeboScanBridge() override
  {
    sub_.reset();
    node_.reset();
    gazebo::client::shutdown();
  }

private:
  void on_scan(ConstLaserScanStampedPtr & msg)
  {
    const auto & s = msg->scan();
    const int nh = s.count();
    const int nv = s.vertical_count() > 0 ? s.vertical_count() : 1;

    sensor_msgs::msg::PointCloud2 out;
    // ROS wall time, not Gazebo sim time: the ROS 2 side runs with
    // use_sim_time false, and a sim-time stamp would look permanently stale
    // to the Nav2 costmap and be discarded.
    out.header.stamp = now();
    out.header.frame_id = frame_id_;
    out.height = 1;
    out.is_dense = false;
    out.is_bigendian = false;

    sensor_msgs::PointCloud2Modifier mod(out);
    mod.setPointCloud2FieldsByString(1, "xyz");
    mod.resize(nh * nv);

    sensor_msgs::PointCloud2Iterator<float> ix(out, "x");
    sensor_msgs::PointCloud2Iterator<float> iy(out, "y");
    sensor_msgs::PointCloud2Iterator<float> iz(out, "z");

    const double a0  = s.angle_min();
    const double da  = s.angle_step();
    const double va0 = s.vertical_angle_min();
    const double dva = nv > 1 ? s.vertical_angle_step() : 0.0;
    const double rmax = s.range_max();

    size_t kept = 0;
    for (int v = 0; v < nv; ++v) {
      const double pitch = va0 + v * dva;
      for (int h = 0; h < nh; ++h) {
        const int idx = v * nh + h;
        if (idx >= s.ranges_size()) break;
        const double r = s.ranges(idx);
        // Gazebo reports "no hit" as +inf or as range_max. Emitting those as
        // points would ring the aircraft in phantom obstacles at max range.
        if (!std::isfinite(r) || r >= rmax || r < min_range_) continue;

        const double yaw = a0 + h * da;
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

  std::string gazebo_topic_, frame_id_, cloud_topic_;
  double min_range_{0.5};
  gazebo::transport::NodePtr node_;
  gazebo::transport::SubscriberPtr sub_;
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr pub_;
  unsigned long count_{0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<GazeboScanBridge>();
  if (!node->connect()) {
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
