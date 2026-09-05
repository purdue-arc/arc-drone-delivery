// costmap_to_cloud — render the occupancy grid as a point cloud.
//
// RViz2's Map display cannot draw the costmap on this machine: its
// indexed_8bit_image shader fails to link against the local Mesa stack
//   "active samplers with a different type refer to the same texture image unit"
// and the grid stays blank. The failure is in the palette-indexed texture path
// and happens identically with hardware GL, llvmpipe, and GPU passthrough, so
// it is not something the launch can configure around.
//
// PointCloud2 renders fine (it is already drawing the Livox returns), so this
// republishes the same OccupancyGrid as one point per interesting cell, with
// the cell cost in an `intensity` field for colouring. Same data, a display
// path that works.
//
//   /global_costmap/costmap  (nav_msgs/OccupancyGrid)  in
//   /arc/costmap_cloud       (sensor_msgs/PointCloud2) out
//
// Only cells at or above min_cost are emitted — free space is the vast
// majority of a 600x600 grid and drawing it would cost 360k points per frame
// for no information.

#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/point_cloud2_iterator.hpp>

#include <string>

class CostmapToCloud : public rclcpp::Node
{
public:
  CostmapToCloud() : Node("costmap_to_cloud")
  {
    const auto in_topic = declare_parameter<std::string>(
      "costmap_topic", "/global_costmap/costmap");
    const auto out_topic = declare_parameter<std::string>(
      "cloud_topic", "/arc/costmap_cloud");
    // 1..100 are inflation/obstacle costs; -1 is unknown. Default keeps
    // lethal and inflated cells and drops everything free.
    min_cost_ = static_cast<int>(declare_parameter("min_cost", 1));
    show_unknown_ = declare_parameter("show_unknown", false);
    z_height_ = declare_parameter("z_height", 0.0);

    pub_ = create_publisher<sensor_msgs::msg::PointCloud2>(
      out_topic, rclcpp::QoS(1).transient_local());

    // The costmap is latched; match it or the first (often only) message on a
    // slow-updating global costmap is missed entirely.
    sub_ = create_subscription<nav_msgs::msg::OccupancyGrid>(
      in_topic, rclcpp::QoS(1).transient_local(),
      std::bind(&CostmapToCloud::grid_callback, this, std::placeholders::_1));

    RCLCPP_INFO(get_logger(), "costmap_to_cloud: %s -> %s (min_cost=%d)",
                in_topic.c_str(), out_topic.c_str(), min_cost_);
  }

private:
  void grid_callback(const nav_msgs::msg::OccupancyGrid::SharedPtr msg)
  {
    const auto & info = msg->info;
    const size_t n = msg->data.size();

    sensor_msgs::msg::PointCloud2 out;
    out.header = msg->header;
    out.height = 1;
    out.is_dense = true;
    out.is_bigendian = false;

    sensor_msgs::PointCloud2Modifier mod(out);
    mod.setPointCloud2Fields(4,
      "x", 1, sensor_msgs::msg::PointField::FLOAT32,
      "y", 1, sensor_msgs::msg::PointField::FLOAT32,
      "z", 1, sensor_msgs::msg::PointField::FLOAT32,
      "intensity", 1, sensor_msgs::msg::PointField::FLOAT32);
    mod.resize(n);

    sensor_msgs::PointCloud2Iterator<float> ix(out, "x");
    sensor_msgs::PointCloud2Iterator<float> iy(out, "y");
    sensor_msgs::PointCloud2Iterator<float> iz(out, "z");
    sensor_msgs::PointCloud2Iterator<float> ii(out, "intensity");

    const double res = info.resolution;
    const double ox = info.origin.position.x;
    const double oy = info.origin.position.y;

    size_t kept = 0;
    for (size_t i = 0; i < n; ++i) {
      const int8_t c = msg->data[i];
      if (c < 0) {                       // unknown
        if (!show_unknown_) continue;
      } else if (c < min_cost_) {
        continue;                        // free / below threshold
      }
      const size_t gx = i % info.width;
      const size_t gy = i / info.width;
      // Cell centre, not corner, so the cloud lines up with the grid.
      *ix = static_cast<float>(ox + (gx + 0.5) * res);
      *iy = static_cast<float>(oy + (gy + 0.5) * res);
      *iz = static_cast<float>(z_height_);
      *ii = static_cast<float>(c < 0 ? 0 : c);
      ++ix; ++iy; ++iz; ++ii; ++kept;
    }
    mod.resize(kept);
    out.width = kept;
    out.row_step = out.point_step * kept;

    pub_->publish(out);
    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 5000,
      "costmap %ux%u @ %.2fm -> %zu points", info.width, info.height,
      info.resolution, kept);
  }

  int    min_cost_{1};
  bool   show_unknown_{false};
  double z_height_{0.0};
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr pub_;
  rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr sub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<CostmapToCloud>());
  rclcpp::shutdown();
  return 0;
}
