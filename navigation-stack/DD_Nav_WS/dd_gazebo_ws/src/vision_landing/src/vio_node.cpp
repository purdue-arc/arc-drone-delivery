#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <px4_msgs/msg/vehicle_odometry.hpp>
#include <px4_msgs/msg/vehicle_local_position.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/opencv.hpp>
#include <deque>

// Visual-Inertial Odometry node
//
// Pipeline:
//   1. Detect GFTT features in the camera image
//   2. Track them across frames with Lucas-Kanade optical flow
//   3. Recover relative pose (R, t) via essential matrix decomposition
//   4. Scale translation using altitude from PX4 local position
//   5. Pre-integrate IMU gyroscope between frames for orientation
//   6. Publish fused pose as VehicleOdometry on /fmu/in/vehicle_visual_odometry

static constexpr int    MAX_FEATURES   = 200;
static constexpr int    MIN_FEATURES   = 50;
static constexpr float  FOCAL_PX       = 525.0f;
static constexpr float  CX             = 320.0f;
static constexpr float  CY             = 240.0f;

struct ImuSample {
  double stamp;
  cv::Vec3d gyro;
  cv::Vec3d accel;
};

class VioNode : public rclcpp::Node
{
public:
  VioNode() : Node("vio_node")
  {
    // This node publishes to /fmu/in/vehicle_visual_odometry — it feeds the
    // flight controller's state estimator. It has never been validated, its
    // intrinsics are hardcoded (525/320/240, wrong for both the SITL camera
    // and the ZED), and the topics it consumes are not published anywhere in
    // this stack. Enabling it on an aircraft would corrupt the estimate the
    // aircraft flies on. It stays behind vio:=false; say so at startup rather
    // than let it look like a working part of the pipeline.
    RCLCPP_WARN(get_logger(),
      "vio_node is UNVALIDATED and feeds the PX4 EKF "
      "(/fmu/in/vehicle_visual_odometry). Hardcoded intrinsics fx=fy=%.0f "
      "cx=%.0f cy=%.0f are wrong for every camera on this aircraft, and it "
      "subscribes to /camera/image_raw and /imu/data, which nothing here "
      "publishes. Do NOT run this on hardware.",
      FOCAL_PX, CX, CY);

    img_sub_ = create_subscription<sensor_msgs::msg::Image>(
      "/camera/image_raw", 10,
      std::bind(&VioNode::image_callback, this, std::placeholders::_1));

    imu_sub_ = create_subscription<sensor_msgs::msg::Imu>(
      "/demo/imu", rclcpp::QoS(50).best_effort(),
      std::bind(&VioNode::imu_callback, this, std::placeholders::_1));

    local_pos_sub_ = create_subscription<px4_msgs::msg::VehicleLocalPosition>(
      "/fmu/out/vehicle_local_position", rclcpp::QoS(10).best_effort(),
      std::bind(&VioNode::local_pos_callback, this, std::placeholders::_1));

    vio_pub_ = create_publisher<px4_msgs::msg::VehicleOdometry>(
      "/fmu/in/vehicle_visual_odometry", rclcpp::QoS(10).best_effort());

    K_ = (cv::Mat_<double>(3, 3)
      << FOCAL_PX, 0, CX,
         0, FOCAL_PX, CY,
         0, 0, 1);

    // Identity quaternion (w, x, y, z)
    q_[0] = 1.0f; q_[1] = 0.0f; q_[2] = 0.0f; q_[3] = 0.0f;
    pos_[0] = 0.0f; pos_[1] = 0.0f; pos_[2] = 0.0f;

    initialized_ = false;
    altitude_ned_ = 0.0f;
    last_img_stamp_ = 0.0;

    RCLCPP_INFO(get_logger(), "VIO node started");
  }

private:
  void imu_callback(const sensor_msgs::msg::Imu::SharedPtr msg)
  {
    ImuSample s;
    s.stamp = msg->header.stamp.sec + msg->header.stamp.nanosec * 1e-9;
    s.gyro  = {msg->angular_velocity.x,
               msg->angular_velocity.y,
               msg->angular_velocity.z};
    s.accel = {msg->linear_acceleration.x,
               msg->linear_acceleration.y,
               msg->linear_acceleration.z};
    imu_buf_.push_back(s);
    if (imu_buf_.size() > 500) imu_buf_.pop_front();
  }

  void local_pos_callback(const px4_msgs::msg::VehicleLocalPosition::SharedPtr msg)
  {
    altitude_ned_ = msg->z;  // NED z (negative = up)
  }

  void image_callback(const sensor_msgs::msg::Image::SharedPtr msg)
  {
    cv::Mat frame;
    try {
      frame = cv_bridge::toCvCopy(msg, "mono8")->image;
    } catch (cv_bridge::Exception & e) {
      RCLCPP_ERROR(get_logger(), "cv_bridge: %s", e.what());
      return;
    }

    double stamp = msg->header.stamp.sec + msg->header.stamp.nanosec * 1e-9;

    if (!initialized_) {
      detect_features(frame);
      prev_frame_ = frame.clone();
      last_img_stamp_ = stamp;
      initialized_ = true;
      return;
    }

    // --- Track features ---
    std::vector<cv::Point2f> curr_pts;
    std::vector<uchar> status;
    std::vector<float> err;
    cv::calcOpticalFlowPyrLK(prev_frame_, frame, prev_pts_, curr_pts,
                              status, err,
                              cv::Size(21, 21), 3,
                              cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS,
                                              30, 0.01));

    // Keep only successfully tracked points
    std::vector<cv::Point2f> good_prev, good_curr;
    for (size_t i = 0; i < status.size(); i++) {
      if (status[i]) {
        good_prev.push_back(prev_pts_[i]);
        good_curr.push_back(curr_pts[i]);
      }
    }

    if ((int)good_prev.size() < MIN_FEATURES) {
      detect_features(frame);
      prev_frame_ = frame.clone();
      last_img_stamp_ = stamp;
      return;
    }

    // --- Recover relative pose from essential matrix ---
    double dt = stamp - last_img_stamp_;
    if (dt <= 0.0) dt = 0.033;

    cv::Mat R_cv, t_cv;
    std::vector<uchar> inliers;
    cv::Mat E = cv::findEssentialMat(good_prev, good_curr, K_,
                                     cv::RANSAC, 0.999, 1.0, inliers);
    int inlier_count = cv::recoverPose(E, good_prev, good_curr, K_, R_cv, t_cv, inliers);

    if (inlier_count < 10) {
      prev_pts_  = good_curr;
      prev_frame_ = frame.clone();
      last_img_stamp_ = stamp;
      return;
    }

    // --- Scale translation using barometric altitude ---
    // altitude_ned_ is NED z (negative = above ground), so actual height = -altitude_ned_
    float height = std::max(0.3f, -altitude_ned_);
    // t_cv is unit vector; scale so vertical component matches dt*vertical_velocity estimate
    // Use a conservative scale tied to feature parallax and known height
    double scale = height * 0.02 * dt;
    cv::Mat t_scaled = t_cv * scale;

    // --- Integrate IMU gyroscope for orientation ---
    integrate_imu(last_img_stamp_, stamp);

    // --- Accumulate position in NED ---
    // Camera frame: x=right, y=down, z=forward -> NED: x=North, y=East, z=Down
    // Map camera translation: t[2]=forward->NED x, t[0]=right->NED y, t[1]=down->NED z
    pos_[0] += (float)t_scaled.at<double>(2);   // North
    pos_[1] += (float)t_scaled.at<double>(0);   // East
    pos_[2] += (float)t_scaled.at<double>(1);   // Down

    // Override z with barometric altitude (more reliable for scale)
    pos_[2] = altitude_ned_;

    publish_odometry(stamp);

    // --- Refresh feature set ---
    prev_pts_  = good_curr;
    prev_frame_ = frame.clone();
    last_img_stamp_ = stamp;

    if ((int)prev_pts_.size() < MIN_FEATURES) {
      detect_features(frame);
    }
  }

  void detect_features(const cv::Mat & frame)
  {
    cv::goodFeaturesToTrack(frame, prev_pts_, MAX_FEATURES,
                            0.01, 10, cv::noArray(), 3, false, 0.04);
    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000,
      "VIO: detected %zu features", prev_pts_.size());
  }

  void integrate_imu(double t_start, double t_end)
  {
    for (const auto & s : imu_buf_) {
      if (s.stamp < t_start || s.stamp > t_end) continue;

      // Simple quaternion integration from gyroscope: q_dot = 0.5 * q * [0, w]
      double dt_imu = 0.01;  // assume ~100 Hz IMU
      double wx = s.gyro[0] * dt_imu;
      double wy = s.gyro[1] * dt_imu;
      double wz = s.gyro[2] * dt_imu;

      float dq[4];
      dq[0] = q_[0] - 0.5f * (q_[1]*wx + q_[2]*wy + q_[3]*wz);
      dq[1] = q_[1] + 0.5f * (q_[0]*wx - q_[3]*wy + q_[2]*wz);
      dq[2] = q_[2] + 0.5f * (q_[3]*wx + q_[0]*wy - q_[1]*wz);
      dq[3] = q_[3] + 0.5f * (-q_[2]*wx + q_[1]*wy + q_[0]*wz);

      float norm = std::sqrt(dq[0]*dq[0]+dq[1]*dq[1]+dq[2]*dq[2]+dq[3]*dq[3]);
      for (int i = 0; i < 4; i++) q_[i] = dq[i] / norm;
    }
  }

  void publish_odometry(double stamp)
  {
    px4_msgs::msg::VehicleOdometry msg;
    msg.timestamp        = (uint64_t)(stamp * 1e6);
    msg.timestamp_sample = msg.timestamp;
    msg.pose_frame       = px4_msgs::msg::VehicleOdometry::POSE_FRAME_NED;
    msg.velocity_frame   = px4_msgs::msg::VehicleOdometry::VELOCITY_FRAME_NED;

    msg.position[0] = pos_[0];
    msg.position[1] = pos_[1];
    msg.position[2] = pos_[2];

    // Orientation (w, x, y, z)
    msg.q[0] = q_[0];
    msg.q[1] = q_[1];
    msg.q[2] = q_[2];
    msg.q[3] = q_[3];

    // Velocity unknown
    msg.velocity[0] = std::numeric_limits<float>::quiet_NaN();
    msg.velocity[1] = std::numeric_limits<float>::quiet_NaN();
    msg.velocity[2] = std::numeric_limits<float>::quiet_NaN();

    // Covariances — conservative values for simulation
    msg.position_variance    = {0.1f, 0.1f, 0.05f};
    msg.orientation_variance = {0.01f, 0.01f, 0.01f};
    msg.velocity_variance    = {std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::quiet_NaN()};
    msg.quality = 1;

    vio_pub_->publish(msg);

    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000,
      "VIO: pos NED (%.2f, %.2f, %.2f)", pos_[0], pos_[1], pos_[2]);
  }

  // ROS handles
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr img_sub_;
  rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
  rclcpp::Subscription<px4_msgs::msg::VehicleLocalPosition>::SharedPtr local_pos_sub_;
  rclcpp::Publisher<px4_msgs::msg::VehicleOdometry>::SharedPtr vio_pub_;

  // Camera
  cv::Mat K_;
  cv::Mat prev_frame_;
  std::vector<cv::Point2f> prev_pts_;
  bool initialized_;
  double last_img_stamp_;

  // IMU buffer
  std::deque<ImuSample> imu_buf_;

  // State
  float pos_[3];   // NED position
  float q_[4];     // Orientation quaternion (w, x, y, z)
  float altitude_ned_;  // From PX4 local position (for scale)
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<VioNode>());
  rclcpp::shutdown();
  return 0;
}
