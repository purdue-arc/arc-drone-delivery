#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/opencv.hpp>
extern "C" {
#include <apriltag/apriltag.h>
#include <apriltag/tag36h11.h>
#include <apriltag/apriltag_pose.h>
}

// Camera intrinsics (Gazebo cgo3 camera, 640×480)
static constexpr double FX     = 525.0;
static constexpr double FY     = 525.0;
static constexpr double CX     = 320.0;
static constexpr double CY     = 240.0;
static constexpr double TAGSIZE = 0.5;   // metres (matches world file)

class AprilTagDetector : public rclcpp::Node
{
public:
  AprilTagDetector() : Node("apriltag_detector")
  {
    td_ = apriltag_detector_create();
    tf_ = tag36h11_create();
    apriltag_detector_add_family(td_, tf_);
    td_->quad_decimate = 2.0;
    td_->nthreads      = 4;

    // Default is the SITL Gazebo camera; on hardware use zed_apriltag_node
    // instead of this node (it decodes the ZED stream directly).
    const auto image_topic = this->declare_parameter<std::string>(
      "image_topic", "/camera/image_raw");

    sub_ = this->create_subscription<sensor_msgs::msg::Image>(
      image_topic, 10,
      std::bind(&AprilTagDetector::image_callback, this, std::placeholders::_1));

    // Publish pose in camera frame directly to landing_target_pose
    pub_ = this->create_publisher<geometry_msgs::msg::PoseStamped>("/landing_target_pose", 10);

    RCLCPP_INFO(this->get_logger(), "AprilTag detector started on '%s' (tagsize=%.2f m)",
                image_topic.c_str(), TAGSIZE);
  }

  ~AprilTagDetector()
  {
    apriltag_detector_destroy(td_);
    tag36h11_destroy(tf_);
  }

private:
  void image_callback(const sensor_msgs::msg::Image::SharedPtr msg)
  {
    cv::Mat frame;
    try {
      frame = cv_bridge::toCvCopy(msg, "bgr8")->image;
    } catch (cv_bridge::Exception & e) {
      RCLCPP_ERROR(this->get_logger(), "cv_bridge error: %s", e.what());
      return;
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    image_u8_t im = {
      .width  = gray.cols,
      .height = gray.rows,
      .stride = gray.cols,
      .buf    = gray.data
    };

    zarray_t * detections = apriltag_detector_detect(td_, &im);

    if (zarray_size(detections) > 0) {
      apriltag_detection_t * det;
      zarray_get(detections, 0, &det);   // use first detection

      // Estimate pose using apriltag's built-in PnP solver
      apriltag_detection_info_t info;
      info.det     = det;
      info.tagsize = TAGSIZE;
      info.fx      = FX;
      info.fy      = FY;
      info.cx      = CX;
      info.cy      = CY;

      apriltag_pose_t pose_est;
      double err = estimate_tag_pose(&info, &pose_est);

      // Camera frame: x=right, y=down, z=forward (depth)
      double tx = pose_est.t->data[0];
      double ty = pose_est.t->data[1];
      double tz = pose_est.t->data[2];

      geometry_msgs::msg::PoseStamped pose_msg;
      pose_msg.header           = msg->header;
      pose_msg.pose.position.x  = tx;
      pose_msg.pose.position.y  = ty;
      pose_msg.pose.position.z  = tz;
      pose_msg.pose.orientation.w = 1.0;

      pub_->publish(pose_msg);

      RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 500,
        "Tag ID=%d  cam xyz=(%.3f, %.3f, %.3f)  err=%.4f",
        det->id, tx, ty, tz, err);

      free(pose_est.R);
      free(pose_est.t);
    }

    apriltag_detections_destroy(detections);
  }

  apriltag_detector_t * td_;
  apriltag_family_t   * tf_;
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr  sub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr pub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<AprilTagDetector>());
  rclcpp::shutdown();
  return 0;
}
