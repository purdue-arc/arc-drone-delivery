// cloud_to_scan — PointCloud2 -> LaserScan.
//
//   /livox/points (sensor_msgs/PointCloud2)  in
//   /scan         (sensor_msgs/LaserScan)    out
//
// SLAM here is 2D: it maps a horizontal slice of the world. The lidar is 3D —
// the Mid-360 on the aircraft, and a 360x32 ray sensor in simulation — so
// something has to flatten it.
//
// Why this node rather than pointcloud_to_laserscan: that package is not in
// the arc-drone image, and adding it would mean the SITL path and the
// hardware path could drift. This consumes /livox/points, which is what the
// real Livox driver publishes AND what gazebo_scan_bridge publishes, so the
// same node serves both — the same principle the rest of this stack follows.
//
// The flattening takes the NEAREST return in each angular bin, within a
// height band around the sensor. Nearest rather than furthest because for
// obstacle mapping a wall at 5 m matters more than the ground at 30 m, and a
// band rather than the whole cloud because otherwise the ground plane and the
// sky both collapse into the slice and the map fills with false walls.

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <sensor_msgs/point_cloud2_iterator.hpp>

#include <cmath>
#include <limits>
#include <vector>

class CloudToScan : public rclcpp::Node
{
public:
  CloudToScan() : Node("cloud_to_scan")
  {
    cloud_topic_ = declare_parameter<std::string>("cloud_topic", "/livox/points");
    scan_topic_  = declare_parameter<std::string>("scan_topic", "/scan");
    // Height band around the sensor, in the cloud's own frame. The Mid-360
    // sits above the airframe looking slightly down; +/- 1 m keeps building
    // walls and rejects both the ground directly below and returns from well
    // above the flight path.
    min_z_       = declare_parameter("min_z", -1.0);
    max_z_       = declare_parameter("max_z", 1.0);
    range_min_   = declare_parameter("range_min", 0.5);
    range_max_   = declare_parameter("range_max", 40.0);
    // One bin per degree by default: finer than the map resolution SLAM uses,
    // so the binning is not what limits map quality.
    angle_increment_ = declare_parameter("angle_increment", 0.0174533);

    scan_pub_ = create_publisher<sensor_msgs::msg::LaserScan>(
      scan_topic_, rclcpp::SensorDataQoS());
    cloud_sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
      cloud_topic_, rclcpp::SensorDataQoS(),
      std::bind(&CloudToScan::on_cloud, this, std::placeholders::_1));

    RCLCPP_INFO(get_logger(),
      "Flattening '%s' -> '%s' (band %.1f..%.1f m, range %.1f..%.1f m)",
      cloud_topic_.c_str(), scan_topic_.c_str(), min_z_, max_z_,
      range_min_, range_max_);
  }

private:
  void on_cloud(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
  {
    const size_t bins =
      static_cast<size_t>(std::lround(2.0 * M_PI / angle_increment_));

    sensor_msgs::msg::LaserScan scan;
    scan.header          = msg->header;
    scan.angle_min       = -static_cast<float>(M_PI);
    scan.angle_max       =  static_cast<float>(M_PI);
    scan.angle_increment = static_cast<float>(angle_increment_);
    scan.time_increment  = 0.0f;
    scan.scan_time       = 0.0f;
    scan.range_min       = static_cast<float>(range_min_);
    scan.range_max       = static_cast<float>(range_max_);
    // inf, not range_max: a bin with no return is "nothing seen", which SLAM
    // must raytrace through as free space. Filling it with range_max would
    // paint a wall at 40 m in every empty direction.
    scan.ranges.assign(bins, std::numeric_limits<float>::infinity());

    sensor_msgs::PointCloud2ConstIterator<float> ix(*msg, "x");
    sensor_msgs::PointCloud2ConstIterator<float> iy(*msg, "y");
    sensor_msgs::PointCloud2ConstIterator<float> iz(*msg, "z");

    size_t kept = 0;
    for (; ix != ix.end(); ++ix, ++iy, ++iz) {
      const float x = *ix, y = *iy, z = *iz;
      if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) continue;
      if (z < min_z_ || z > max_z_) continue;

      const double r = std::hypot(x, y);
      if (r < range_min_ || r > range_max_) continue;

      const double a = std::atan2(y, x);
      auto bin = static_cast<size_t>((a - scan.angle_min) / angle_increment_);
      if (bin >= bins) continue;

      if (static_cast<float>(r) < scan.ranges[bin]) {
        scan.ranges[bin] = static_cast<float>(r);
        ++kept;
      }
    }

    scan_pub_->publish(scan);
    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 10000,
      "scan: %zu of %u points inside the band", kept, msg->width * msg->height);
  }

  std::string cloud_topic_, scan_topic_;
  double min_z_, max_z_, range_min_, range_max_, angle_increment_;
  rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_pub_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr cloud_sub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<CloudToScan>());
  rclcpp::shutdown();
  return 0;
}
