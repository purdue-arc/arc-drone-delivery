#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_array.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <opencv2/opencv.hpp>

class PoseEstimator : public rclcpp::Node
{
public:
  PoseEstimator() : Node("pose_estimator")
  {
    /* Camera intrinsics — update these from /zed/zed_node/rgb/camera_info
    camera_matrix_ = (cv::Mat_<double>(3, 3) 
      525.0, 0.0, 320.0,
      0.0, 525.0, 240.0,
      0.0, 0.0, 1.0);
    dist_coeffs_ = cv::Mat::zeros(4, 1, CV_64F);*/

    camera_matrix_ = cv::Mat::eye(3, 3, CV_64F);
    camera_matrix_.at<double>(0,0) = 525.0;
    camera_matrix_.at<double>(0,2) = 320.0;
    camera_matrix_.at<double>(1,1) = 525.0;
    camera_matrix_.at<double>(1,2) = 240.0;

    // AprilTag size in meters
    tag_size_ = 0.5;

    sub_ = this->create_subscription<geometry_msgs::msg::PoseArray>(
      "/tag_detections", 10,
      std::bind(&PoseEstimator::detection_callback, this, std::placeholders::_1));

    pub_ = this->create_publisher<geometry_msgs::msg::PoseStamped>("/landing_target_pose", 10);

    RCLCPP_INFO(this->get_logger(), "Pose estimator started");
  }

private:
  void detection_callback(const geometry_msgs::msg::PoseArray::SharedPtr msg)
  {
    if (msg->poses.empty()) return;

   /* double h = tag_size_ / 2.0;
    std::vector<cv::Point3f> object_points = {
      {-h,  h, 0}, { h,  h, 0},
      { h, -h, 0}, {-h, -h, 0}
    };*/

    float h = (float)(tag_size_ / 2.0);
    std::vector<cv::Point3f> object_points = {
        {-h,  h, 0}, { h,  h, 0},
        { h, -h, 0}, {-h, -h, 0}
    };

    // Use first detection's center as image point placeholder
    auto & p = msg->poses[0];
    std::vector<cv::Point2f> image_points = {
      {(float)p.position.x - 50, (float)p.position.y - 50},
      {(float)p.position.x + 50, (float)p.position.y - 50},
      {(float)p.position.x + 50, (float)p.position.y + 50},
      {(float)p.position.x - 50, (float)p.position.y + 50}
    };

    cv::Mat rvec, tvec;
    cv::solvePnP(object_points, image_points, camera_matrix_, dist_coeffs_, rvec, tvec);

    geometry_msgs::msg::PoseStamped pose;
    pose.header = msg->header;
    pose.pose.position.x = tvec.at<double>(0);
    pose.pose.position.y = tvec.at<double>(1);
    pose.pose.position.z = tvec.at<double>(2);
    pose.pose.orientation.w = 1.0;

    pub_->publish(pose);
    RCLCPP_INFO(this->get_logger(), "Tag pose: x=%.2f y=%.2f z=%.2f",
      tvec.at<double>(0), tvec.at<double>(1), tvec.at<double>(2));
  }

  cv::Mat camera_matrix_;
  cv::Mat dist_coeffs_;
  double tag_size_;
  rclcpp::Subscription<geometry_msgs::msg::PoseArray>::SharedPtr sub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr pub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<PoseEstimator>());
  rclcpp::shutdown();
  return 0;
}
