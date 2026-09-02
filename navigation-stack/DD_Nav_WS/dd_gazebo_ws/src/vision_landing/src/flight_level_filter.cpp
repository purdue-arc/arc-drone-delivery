// flight_level_filter — keep only the lidar returns the aircraft could
// actually hit, and drop the rest before they reach the costmap.
//
//   /livox/points               (PointCloud2, sensor frame)  in
//   /livox/points_flight_level  (PointCloud2, map frame)     out
//
// THE PROBLEM THIS SOLVES
//
// Nav2's costmap is 2D. Its obstacle layer takes every return between
// min_obstacle_height and max_obstacle_height and flattens it onto one plane,
// so a building is marked whether the aircraft is flying at 5 m or at 35 m.
// There is no way for a 2D map to say "that is below me and I will clear it".
//
// Flown, that produces two bad outcomes:
//
//   1. The aircraft detours around a 3 m shed it would pass 12 m above.
//   2. Worse, and observed in the 2026-09-02 flight: the aircraft ends up
//      INSIDE the 2D footprint of a building taller than its transit
//      altitude. Every direction it could move is marked lethal, including
//      where it already is, so a path safety check refuses every step and the
//      mission hovers until it times out.
//
// THE FIX
//
// Only returns near the aircraft's own altitude reach the costmap. A building
// that rises through the flight level still marks — its wall is right there at
// that height — while a low roof underneath does not. The 2D map then means
// "things at my level", which is a question a 2D map can answer honestly.
//
// The band is deliberately asymmetric. Above the aircraft it is generous,
// because climbing into something is the failure this cannot be allowed to
// miss and because the aircraft may climb during the leg. Below it is tight,
// because clearing something by a few metres is exactly the case that ought
// to be allowed.
//
// A hard floor above the launch elevation keeps the ground plane out of the
// map when the aircraft is low. Without it, everything within the band during
// takeoff and landing is ground, and the costmap fills with a wall of it.

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/point_cloud2_iterator.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

#include <cmath>
#include <memory>
#include <string>

class FlightLevelFilter : public rclcpp::Node
{
public:
  FlightLevelFilter() : Node("flight_level_filter")
  {
    in_topic_   = declare_parameter<std::string>("input_topic", "/livox/points");
    out_topic_  = declare_parameter<std::string>("output_topic",
                                                 "/livox/points_flight_level");
    map_frame_  = declare_parameter<std::string>("map_frame", "map");
    // Metres above the aircraft still treated as an obstacle.
    above_m_    = declare_parameter("clearance_above_m", 6.0);
    // Metres below. Anything lower than this is something we are passing over.
    below_m_    = declare_parameter("clearance_below_m", 2.5);
    // Absolute floor in the map frame, to keep the ground out of the costmap.
    floor_m_    = declare_parameter("min_absolute_z", 1.5);
    // If TF is unavailable the safe answer is to pass EVERYTHING through, not
    // to publish an empty cloud: an empty cloud reads as "all clear", which is
    // the one wrong answer an obstacle filter must never give.
    passthrough_without_tf_ =
      declare_parameter("passthrough_without_tf", true);

    pub_ = create_publisher<sensor_msgs::msg::PointCloud2>(
      out_topic_, rclcpp::SensorDataQoS());
    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);
    sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
      in_topic_, rclcpp::SensorDataQoS(),
      std::bind(&FlightLevelFilter::on_cloud, this, std::placeholders::_1));

    RCLCPP_INFO(get_logger(),
      "Flight-level filter: '%s' -> '%s', keeping returns from %.1f m below to "
      "%.1f m above the aircraft, floor %.1f m",
      in_topic_.c_str(), out_topic_.c_str(), below_m_, above_m_, floor_m_);
  }

private:
  void on_cloud(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
  {
    geometry_msgs::msg::TransformStamped tf;
    try {
      tf = tf_buffer_->lookupTransform(map_frame_, msg->header.frame_id,
                                       tf2::TimePointZero);
    } catch (const std::exception & e) {
      if (passthrough_without_tf_) {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
          "No transform %s <- %s; passing the cloud through UNFILTERED so "
          "nothing is silently declared clear: %s",
          map_frame_.c_str(), msg->header.frame_id.c_str(), e.what());
        pub_->publish(*msg);
      }
      return;
    }

    const auto & q = tf.transform.rotation;
    const auto & t = tf.transform.translation;
    // Rotation matrix from the quaternion (only the z row is needed to test
    // height, but the full transform is published so the costmap gets points
    // already in the map frame).
    const double xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const double xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const double wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    const double r[9] = {
      1 - 2 * (yy + zz), 2 * (xy - wz),     2 * (xz + wy),
      2 * (xy + wz),     1 - 2 * (xx + zz), 2 * (yz - wx),
      2 * (xz - wy),     2 * (yz + wx),     1 - 2 * (xx + yy)};

    const double craft_z = t.z;
    const double lo = std::max(craft_z - below_m_, floor_m_);
    const double hi = craft_z + above_m_;

    sensor_msgs::PointCloud2ConstIterator<float> ix(*msg, "x");
    sensor_msgs::PointCloud2ConstIterator<float> iy(*msg, "y");
    sensor_msgs::PointCloud2ConstIterator<float> iz(*msg, "z");

    std::vector<std::array<float, 3>> kept;
    kept.reserve(msg->width * msg->height / 4 + 16);
    size_t total = 0;
    for (; ix != ix.end(); ++ix, ++iy, ++iz, ++total) {
      const double x = *ix, y = *iy, z = *iz;
      if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) continue;
      const double mz = r[6] * x + r[7] * y + r[8] * z + t.z;
      if (mz < lo || mz > hi) continue;
      kept.push_back({
        static_cast<float>(r[0] * x + r[1] * y + r[2] * z + t.x),
        static_cast<float>(r[3] * x + r[4] * y + r[5] * z + t.y),
        static_cast<float>(mz)});
    }

    sensor_msgs::msg::PointCloud2 out;
    out.header.stamp = msg->header.stamp;
    out.header.frame_id = map_frame_;
    out.height = 1;
    out.width = static_cast<uint32_t>(kept.size());
    out.is_bigendian = false;
    out.is_dense = true;
    out.point_step = 12;
    out.row_step = 12 * out.width;
    out.fields.resize(3);
    const char * names[3] = {"x", "y", "z"};
    for (int i = 0; i < 3; ++i) {
      out.fields[i].name = names[i];
      out.fields[i].offset = 4 * i;
      out.fields[i].datatype = sensor_msgs::msg::PointField::FLOAT32;
      out.fields[i].count = 1;
    }
    out.data.resize(out.row_step);
    std::memcpy(out.data.data(), kept.data(), out.row_step);
    pub_->publish(out);

    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 10000,
      "flight level %.1f m: kept %zu of %zu returns (band %.1f..%.1f m)",
      craft_z, kept.size(), total, lo, hi);
  }

  std::string in_topic_, out_topic_, map_frame_;
  double above_m_, below_m_, floor_m_;
  bool passthrough_without_tf_;
  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr pub_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FlightLevelFilter>());
  rclcpp::shutdown();
  return 0;
}
