#pragma once
#include <string>
#include <vector>
#include <opencv2/core.hpp>

extern "C" {
#include <apriltag.h>
#include <apriltag_pose.h>
#include <tag36h11.h>
#include <tag25h9.h>
#include <tag16h5.h>
}

struct TagDetection {
  int id = -1;
  std::string family;
  cv::Point2d p[4];
  cv::Point2d center;
  int hamming = 0;
  float decision_margin = 0.0f;
  double homography[9] = {};   // row-major 3x3

  bool has_pose = false;
  cv::Vec3d t;      // camera->tag translation (meters)
  cv::Matx33d R;    // camera->tag rotation (3x3)
  double reproj_err = 0.0;
};

class AprilTagDetector {
public:
  AprilTagDetector();
  ~AprilTagDetector();

  AprilTagDetector(const AprilTagDetector&) = delete;
  AprilTagDetector& operator=(const AprilTagDetector&) = delete;

  void configure(const std::string& family,
                 int threads,
                 float decimate,
                 float blur,
                 int refine_edges);

  void enable_pose(double fx, double fy, double cx, double cy, double tag_size_m);

  std::vector<TagDetection> detect(const cv::Mat& gray);

private:
  static void destroy_family(apriltag_family_t* family, const std::string& name);

  apriltag_family_t* tf_ = nullptr;
  apriltag_detector_t* td_ = nullptr;
  std::string family_name_;

  bool pose_enabled_ = false;
  apriltag_detection_info_t info_{};
};
