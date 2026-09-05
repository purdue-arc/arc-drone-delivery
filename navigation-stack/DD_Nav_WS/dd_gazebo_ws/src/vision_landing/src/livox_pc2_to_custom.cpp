// livox_pc2_to_custom — sensor_msgs/PointCloud2 -> livox_ros_driver2/CustomMsg.
//
// FAST-LIO2 reads the Livox CustomMsg (`preprocess.lidar_type: 1`), because
// that is the only Livox format carrying a PER-POINT time offset. The Mid-360
// sweeps for 100 ms per frame; at 4 m/s that is 0.4 m of aircraft travel inside
// one "scan", and de-skewing it is most of what makes a lidar-inertial
// estimator work on a moving vehicle rather than a cart. Feed FAST-LIO a cloud
// with no per-point time and every frame is smeared by the distance flown.
//
// WHY A CONVERTER INSTEAD OF SWITCHING THE DRIVER TO CustomMsg
//
// The obvious alternative is `xfer_format: 1` on livox_ros_driver2 and a
// CustomMsg -> PointCloud2 converter pointing the other way. That was rejected:
// it puts a converter in the FLIGHT-CRITICAL path. /livox/points is the only
// obstacle source Nav2's costmap has, and with `require_costmap_to_fly` the
// mission refuses to transit without it. A converter feeding the costmap is one
// more process that can die between the sensor and the thing that stops the
// aircraft hitting a building.
//
// This direction inverts that. The driver keeps publishing PointCloud2 exactly
// as it does today, the costmap path is untouched, and if this node dies the
// only thing that stops is FAST-LIO — which is an observer. The cost is one
// extra copy of the cloud on the wire, about 0.5 MB/s at 10 Hz.
//
// THE SIMULATOR GOES THROUGH THE SAME NODE
//
// gazebo_scan_bridge publishes bare xyz on the same topic, so SITL exercises
// this converter and FAST-LIO's Livox path rather than a second code path that
// only simulation ever runs. Its cloud has no `timestamp` field, and that is
// CORRECT rather than degraded: a Gazebo ray sensor samples the whole frame in
// one instant, so every point genuinely has offset_time 0 and there is no skew
// to remove. Which means SITL cannot exercise de-skewing at all — the one part
// of this that only the real sensor will test.

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <livox_ros_driver2/msg/custom_msg.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

namespace
{
// Byte offset of `name` in the cloud, or -1. Also checks the datatype, because
// reading a float32 field as float64 does not fail, it just returns nonsense.
int field_offset(const sensor_msgs::msg::PointCloud2 & c, const std::string & name,
                 uint8_t datatype)
{
  for (const auto & f : c.fields) {
    if (f.name == name) return f.datatype == datatype ? static_cast<int>(f.offset) : -1;
  }
  return -1;
}

template <typename T>
T read_at(const uint8_t * p, int offset)
{
  T v;
  std::memcpy(&v, p + offset, sizeof(T));
  return v;
}
}  // namespace

class LivoxPc2ToCustom : public rclcpp::Node
{
public:
  LivoxPc2ToCustom() : Node("livox_pc2_to_custom")
  {
    input_topic_ = declare_parameter<std::string>("input_topic", "/livox/points");
    output_topic_ = declare_parameter<std::string>("output_topic", "/livox/lidar_custom");
    // A frame is 100 ms at 10 Hz. Anything claiming a larger offset than this
    // is a misread field, not a late point, and handing FAST-LIO a point
    // stamped seconds into the future would drag the whole de-skew with it.
    max_offset_ns_ = declare_parameter<int64_t>("max_offset_ns", 200000000);
    pub_ = create_publisher<livox_ros_driver2::msg::CustomMsg>(
      output_topic_, rclcpp::QoS(20));
    sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
      input_topic_, rclcpp::SensorDataQoS(),
      std::bind(&LivoxPc2ToCustom::on_cloud, this, std::placeholders::_1));

    RCLCPP_INFO(get_logger(), "Converting '%s' (PointCloud2) -> '%s' (CustomMsg)",
                input_topic_.c_str(), output_topic_.c_str());
  }

private:
  void on_cloud(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
  {
    const int ox = field_offset(*msg, "x", sensor_msgs::msg::PointField::FLOAT32);
    const int oy = field_offset(*msg, "y", sensor_msgs::msg::PointField::FLOAT32);
    const int oz = field_offset(*msg, "z", sensor_msgs::msg::PointField::FLOAT32);
    if (ox < 0 || oy < 0 || oz < 0) {
      RCLCPP_ERROR_THROTTLE(
        get_logger(), *get_clock(), 5000,
        "'%s' has no float32 x/y/z — cannot convert. FAST-LIO will see nothing.",
        input_topic_.c_str());
      return;
    }
    // Optional. The real driver's PointXYZRTLT carries all four; the simulator
    // bridge carries none of them.
    const int oi = field_offset(*msg, "intensity", sensor_msgs::msg::PointField::FLOAT32);
    const int otag = field_offset(*msg, "tag", sensor_msgs::msg::PointField::UINT8);
    const int oline = field_offset(*msg, "line", sensor_msgs::msg::PointField::UINT8);
    const int ots = field_offset(*msg, "timestamp", sensor_msgs::msg::PointField::FLOAT64);

    const size_t n = static_cast<size_t>(msg->width) * msg->height;
    const size_t step = msg->point_step;
    if (n == 0 || msg->data.size() < n * step) return;

    // livox_ros_driver2 writes the point's ABSOLUTE epoch time into the
    // PointCloud2 `timestamp` field, while CustomMsg wants an offset from the
    // frame's base. Recover the base as the earliest point in the frame, which
    // is what the driver itself uses.
    double base_ts = 0.0;
    if (ots >= 0) {
      base_ts = std::numeric_limits<double>::max();
      for (size_t i = 0; i < n; ++i) {
        const double t = read_at<double>(msg->data.data(), static_cast<int>(i * step) + ots);
        if (std::isfinite(t) && t < base_ts) base_ts = t;
      }
      if (!std::isfinite(base_ts) || base_ts == std::numeric_limits<double>::max()) {
        base_ts = 0.0;
      }
    }

    livox_ros_driver2::msg::CustomMsg out;
    // FAST-LIO derives the frame's time from header.stamp, so this must be the
    // stamp the rest of the stack already agrees on — ROS wall time in both
    // worlds, since PX4 publishes no /clock and use_sim_time is false
    // everywhere here.
    out.header = msg->header;
    out.lidar_id = 0;
    out.timebase = ots >= 0
      ? static_cast<uint64_t>(base_ts)
      : static_cast<uint64_t>(rclcpp::Time(msg->header.stamp).nanoseconds());
    out.points.resize(n);

    size_t kept = 0;
    for (size_t i = 0; i < n; ++i) {
      const uint8_t * p = msg->data.data() + i * step;
      const float x = read_at<float>(p, ox);
      const float y = read_at<float>(p, oy);
      const float z = read_at<float>(p, oz);
      // A NaN reaches FAST-LIO's blind-radius test as false and then poisons
      // the ikd-Tree, whose nearest-neighbour search has no NaN guard.
      if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) continue;

      auto & q = out.points[kept];
      q.x = x;
      q.y = y;
      q.z = z;
      q.reflectivity = oi >= 0
        ? static_cast<uint8_t>(std::clamp(read_at<float>(p, oi), 0.0f, 255.0f))
        : 0;
      // FAST-LIO keeps a point only when (tag & 0x30) is 0x00 or 0x10, so an
      // absent tag field must read as 0 rather than as anything invented.
      q.tag = otag >= 0 ? read_at<uint8_t>(p, otag) : 0;
      // And it drops any point whose line is >= preprocess.scan_line.
      q.line = oline >= 0 ? read_at<uint8_t>(p, oline) : 0;

      if (ots >= 0) {
        const double t = read_at<double>(p, ots);
        const double d = std::isfinite(t) ? t - base_ts : 0.0;
        q.offset_time = (d < 0.0 || d > static_cast<double>(max_offset_ns_))
          ? 0u
          : static_cast<uint32_t>(d);
      } else {
        // No per-point time. In simulation the whole frame IS one instant, so
        // zero is the truth rather than a fallback.
        q.offset_time = 0u;
      }
      ++kept;
    }

    out.points.resize(kept);
    out.point_num = static_cast<uint32_t>(kept);
    pub_->publish(out);

    if (++count_ % 100 == 1) {
      RCLCPP_INFO(get_logger(), "custom msg #%lu: %zu points, per-point time: %s",
                  count_, kept, ots >= 0 ? "yes" : "no (frame treated as instantaneous)");
    }
  }

  std::string input_topic_, output_topic_;
  int64_t max_offset_ns_{200000000};
  unsigned long count_{0};
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_;
  rclcpp::Publisher<livox_ros_driver2::msg::CustomMsg>::SharedPtr pub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LivoxPc2ToCustom>());
  rclcpp::shutdown();
  return 0;
}
