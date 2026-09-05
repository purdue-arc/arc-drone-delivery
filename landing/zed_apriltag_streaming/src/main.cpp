#include "apriltag_detector.hpp"
#include "calibration.hpp"
#include "zed_gst_pipeline.hpp"

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <apriltag_msgs/msg/april_tag_detection.hpp>
#include <apriltag_msgs/msg/april_tag_detection_array.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <cv_bridge/cv_bridge.hpp>

#include <opencv2/opencv.hpp>
#include <thread>
#include <chrono>
#include <string>
#include <cmath>
#include <optional>


class ZedApriltagNode : public rclcpp::Node
{
public:
  explicit ZedApriltagNode()
  : Node("zed_apriltag")
  {
    declare_parameter("source", "gst");
    declare_parameter("gst_pipeline", zas::default_udp_h264_pipeline(5000));
    declare_parameter("device_index", 0);
    declare_parameter("video_path", "");
    declare_parameter("frame_id", "camera");
    declare_parameter("publish_debug_image", false);
    declare_parameter("tag_family", "tag36h11");
    declare_parameter("tag_size_m", 0.0);
    declare_parameter("detector_threads", 2);
    declare_parameter("detector_decimate", 2.0);
    declare_parameter("detector_blur", 0.0);
    declare_parameter("detector_refine_edges", true);
    declare_parameter("calib_file", "");
    declare_parameter("fx", 0.0);
    declare_parameter("fy", 0.0);
    declare_parameter("cx", 0.0);
    declare_parameter("cy", 0.0);
    // Tag whose camera-frame pose feeds the mission controller; -1 = any tag.
    declare_parameter("target_tag_id", 0);
    // Refuse to run without usable intrinsics. Without them the node can still
    // detect tags but can never publish /landing_target_pose, so the mission
    // searches, finds nothing and failsafe-lands — a silent failure that looks
    // exactly like a missing tag. On the aircraft this must stay true.
    declare_parameter("require_pose", true);
    // Seconds of dead stream before the capture is torn down and reopened.
    declare_parameter("stream_timeout_sec", 3.0);
    declare_parameter("reopen_backoff_sec", 1.0);

    det_pub_ = create_publisher<apriltag_msgs::msg::AprilTagDetectionArray>(
      "detections", rclcpp::SensorDataQoS());
    img_pub_ = create_publisher<sensor_msgs::msg::Image>(
      "image_debug", rclcpp::SensorDataQoS());
    target_pub_ = create_publisher<geometry_msgs::msg::PoseStamped>(
      "/landing_target_pose", rclcpp::SensorDataQoS());
  }

  // Blocking capture loop — call from main() while rclcpp::spin runs in a thread.
  void run()
  {
    const auto source = get_parameter("source").as_string();
    frame_id_ = get_parameter("frame_id").as_string();
    publish_debug_ = get_parameter("publish_debug_image").as_bool();
    target_tag_id_ = static_cast<int>(get_parameter("target_tag_id").as_int());

    const auto family = get_parameter("tag_family").as_string();
    const int threads = static_cast<int>(get_parameter("detector_threads").as_int());
    const float decimate = static_cast<float>(get_parameter("detector_decimate").as_double());
    const float blur = static_cast<float>(get_parameter("detector_blur").as_double());
    const int refine = get_parameter("detector_refine_edges").as_bool() ? 1 : 0;
    detector_.configure(family, threads, decimate, blur, refine);

    const double tag_size = get_parameter("tag_size_m").as_double();
    const auto calib_file = get_parameter("calib_file").as_string();
    const double p_fx = get_parameter("fx").as_double();
    const double p_fy = get_parameter("fy").as_double();
    const double p_cx = get_parameter("cx").as_double();
    const double p_cy = get_parameter("cy").as_double();

    std::optional<CameraIntrinsics> intr;
    if (!calib_file.empty()) {
      intr = load_intrinsics_yaml(calib_file);
      if (!intr) {
        RCLCPP_WARN(get_logger(), "Could not load intrinsics from: %s", calib_file.c_str());
      }
    }
    if (!intr && (p_fx > 0 || p_fy > 0)) {
      intr = CameraIntrinsics{};
    }
    if (intr) {
      if (p_fx > 0) intr->fx = p_fx;
      if (p_fy > 0) intr->fy = p_fy;
      if (p_cx > 0) intr->cx = p_cx;
      if (p_cy > 0) intr->cy = p_cy;
    }

    const bool want_pose = (intr && intr->fx > 0 && intr->fy > 0 && tag_size > 0);

    // Checked BEFORE the capture is opened, so a misconfigured aircraft fails
    // on the pad in one obvious line instead of flying a search that could
    // never have succeeded. Running without pose is only useful for bench
    // detection work — hence require_pose:=false, never on the vehicle.
    if (!want_pose) {
      const auto msg = std::string(
        "No usable camera intrinsics: /landing_target_pose can never be "
        "published and the mission would search until it failsafe-lands. "
        "Set calib_file (preferred) or fx/fy/cx/cy, and tag_size_m > 0. ") +
        "calib_file='" + calib_file + "' tag_size_m=" + std::to_string(tag_size);
      if (get_parameter("require_pose").as_bool()) {
        RCLCPP_FATAL(get_logger(), "%s Refusing to start.", msg.c_str());
        rclcpp::shutdown();
        return;
      }
      RCLCPP_WARN(get_logger(), "%s require_pose is false, continuing anyway.",
                  msg.c_str());
    }

    const double stream_timeout = get_parameter("stream_timeout_sec").as_double();
    const double reopen_backoff = get_parameter("reopen_backoff_sec").as_double();

    // Pose setup is deferred to the first frame: the encoded stream may be a
    // different resolution than the calibration was taken at, and intrinsics
    // scale with resolution. Using unscaled values silently biases every range
    // estimate (and therefore the landing point).
    bool pose_ready = false;

    cv::VideoCapture cap;
    cv::Mat frame, gray;
    auto last_frame = std::chrono::steady_clock::now();
    bool have_first_frame = false;

    while (rclcpp::ok()) {
      // (Re)open the capture whenever it is not usable. The old code opened
      // once and, on failure, returned — and a stream that died mid-flight hit
      // a bare `continue` that spun a core at 100% forever. Neither is
      // acceptable on an aircraft: the ZED pipeline can be restarted under us,
      // and perception must come back on its own when it is.
      if (!cap.isOpened()) {
        if (!open_capture(cap, source)) {
          RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 5000,
            "Cannot open video source '%s' — retrying every %.1fs. %s",
            source.c_str(), reopen_backoff,
            source == "gst" ? get_parameter("gst_pipeline").as_string().c_str() : "");
          std::this_thread::sleep_for(
            std::chrono::duration<double>(reopen_backoff));
          continue;
        }
        RCLCPP_INFO(get_logger(),
          "Capture open on source '%s' — waiting for the first frame", source.c_str());
        last_frame = std::chrono::steady_clock::now();
        have_first_frame = false;
      }

      if (!cap.read(frame) || frame.empty()) {
        if (source == "file") break;
        const double idle = std::chrono::duration<double>(
          std::chrono::steady_clock::now() - last_frame).count();
        if (idle > stream_timeout) {
          RCLCPP_WARN(get_logger(),
            "No frames for %.1fs on '%s' — reopening the capture", idle, source.c_str());
          cap.release();
        } else {
          // Waiting, not spinning. Silence here used to be indistinguishable
          // from a healthy stream with no tag in view.
          RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
            "%s (%.1fs)", have_first_frame ? "Stream stalled" : "No frames yet", idle);
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        continue;
      }
      if (!have_first_frame) {
        have_first_frame = true;
        RCLCPP_INFO(get_logger(), "Receiving video: %dx%d", frame.cols, frame.rows);
      }
      last_frame = std::chrono::steady_clock::now();

      if (want_pose && !pose_ready) {
        double fx = intr->fx, fy = intr->fy, cx = intr->cx, cy = intr->cy;
        if (intr->width > 0 && intr->height > 0 &&
            (intr->width != frame.cols || intr->height != frame.rows)) {
          const double sx = static_cast<double>(frame.cols) / intr->width;
          const double sy = static_cast<double>(frame.rows) / intr->height;
          fx *= sx;  cx *= sx;
          fy *= sy;  cy *= sy;
          RCLCPP_WARN(get_logger(),
            "Calibration is %dx%d but stream is %dx%d — scaled intrinsics by "
            "(%.3f, %.3f). Calibrate at the streamed resolution to avoid this.",
            intr->width, intr->height, frame.cols, frame.rows, sx, sy);
        } else if (intr->width <= 0 || intr->height <= 0) {
          RCLCPP_WARN(get_logger(),
            "Calibration has no image_width/image_height — cannot verify it "
            "matches the %dx%d stream; using intrinsics unscaled.",
            frame.cols, frame.rows);
        }
        detector_.enable_pose(fx, fy, cx, cy, tag_size);
        RCLCPP_INFO(get_logger(),
          "Pose enabled: fx=%.1f fy=%.1f cx=%.1f cy=%.1f tag=%.4fm @ %dx%d",
          fx, fy, cx, cy, tag_size, frame.cols, frame.rows);
        pose_ready = true;
      }

      cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
      publish(detector_.detect(gray), frame);
    }
  }

private:
  bool open_capture(cv::VideoCapture & cap, const std::string & source)
  {
    if (source == "v4l2") {
      cap.open(static_cast<int>(get_parameter("device_index").as_int()));
    } else if (source == "file") {
      cap.open(get_parameter("video_path").as_string());
    } else {
      cap.open(get_parameter("gst_pipeline").as_string(), cv::CAP_GSTREAMER);
    }
    return cap.isOpened();
  }

  void publish(const std::vector<TagDetection> & dets, const cv::Mat & frame)
  {
    auto stamp = now();

    apriltag_msgs::msg::AprilTagDetectionArray arr;
    arr.header.stamp = stamp;
    arr.header.frame_id = frame_id_;

    for (const auto & d : dets) {
      apriltag_msgs::msg::AprilTagDetection msg;
      msg.family = d.family;
      msg.id = d.id;
      msg.hamming = d.hamming;
      msg.decision_margin = d.decision_margin;
      msg.goodness = 0.0f;

      msg.centre.x = d.center.x;
      msg.centre.y = d.center.y;
      for (int i = 0; i < 4; ++i) {
        msg.corners[i].x = d.p[i].x;
        msg.corners[i].y = d.p[i].y;
      }
      for (int i = 0; i < 9; ++i) {
        msg.homography[i] = d.homography[i];
      }

      arr.detections.push_back(msg);

      // Camera-frame pose of the landing tag — the interface the
      // vision_landing mission_controller consumes (x=right, y=down, z=depth).
      if (d.has_pose && (target_tag_id_ < 0 || d.id == target_tag_id_)) {
        geometry_msgs::msg::PoseStamped pose;
        pose.header.stamp = stamp;
        pose.header.frame_id = frame_id_;
        pose.pose.position.x = d.t[0];
        pose.pose.position.y = d.t[1];
        pose.pose.position.z = d.t[2];
        pose.pose.orientation = quat_from_rotation(d.R);
        target_pub_->publish(pose);
      }
    }

    det_pub_->publish(arr);

    if (publish_debug_ && img_pub_->get_subscription_count() > 0) {
      std_msgs::msg::Header hdr;
      hdr.stamp = stamp;
      hdr.frame_id = frame_id_;
      img_pub_->publish(*cv_bridge::CvImage(hdr, "bgr8", frame).toImageMsg());
    }
  }

  static geometry_msgs::msg::Quaternion quat_from_rotation(const cv::Matx33d & R)
  {
    geometry_msgs::msg::Quaternion q;
    double trace = R(0, 0) + R(1, 1) + R(2, 2);
    if (trace > 0.0) {
      double s = 0.5 / std::sqrt(trace + 1.0);
      q.w = 0.25 / s;
      q.x = (R(2, 1) - R(1, 2)) * s;
      q.y = (R(0, 2) - R(2, 0)) * s;
      q.z = (R(1, 0) - R(0, 1)) * s;
    } else if (R(0, 0) > R(1, 1) && R(0, 0) > R(2, 2)) {
      double s = 2.0 * std::sqrt(1.0 + R(0, 0) - R(1, 1) - R(2, 2));
      q.w = (R(2, 1) - R(1, 2)) / s;
      q.x = 0.25 * s;
      q.y = (R(0, 1) + R(1, 0)) / s;
      q.z = (R(0, 2) + R(2, 0)) / s;
    } else if (R(1, 1) > R(2, 2)) {
      double s = 2.0 * std::sqrt(1.0 + R(1, 1) - R(0, 0) - R(2, 2));
      q.w = (R(0, 2) - R(2, 0)) / s;
      q.x = (R(0, 1) + R(1, 0)) / s;
      q.y = 0.25 * s;
      q.z = (R(1, 2) + R(2, 1)) / s;
    } else {
      double s = 2.0 * std::sqrt(1.0 + R(2, 2) - R(0, 0) - R(1, 1));
      q.w = (R(1, 0) - R(0, 1)) / s;
      q.x = (R(0, 2) + R(2, 0)) / s;
      q.y = (R(1, 2) + R(2, 1)) / s;
      q.z = 0.25 * s;
    }
    return q;
  }

  rclcpp::Publisher<apriltag_msgs::msg::AprilTagDetectionArray>::SharedPtr det_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr img_pub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr target_pub_;
  AprilTagDetector detector_;
  std::string frame_id_{"camera"};
  bool publish_debug_{false};
  int target_tag_id_{0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<ZedApriltagNode>();
  auto spin_thread = std::thread([&node]() {rclcpp::spin(node);});
  node->run();
  rclcpp::shutdown();
  spin_thread.join();
  return 0;
}
