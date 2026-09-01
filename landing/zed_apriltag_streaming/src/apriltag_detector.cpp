#include "apriltag_detector.hpp"
#include <cmath>
#include <stdexcept>

static apriltag_family_t* make_family(const std::string& name) {
  if (name == "tag36h11") return tag36h11_create();
  if (name == "tag25h9")  return tag25h9_create();
  if (name == "tag16h5")  return tag16h5_create();
  throw std::runtime_error("Unsupported tag family: " + name + " (add it in make_family)");
}

AprilTagDetector::AprilTagDetector() {
  td_ = apriltag_detector_create();
  if (!td_) throw std::runtime_error("Failed to create apriltag detector");
  configure("tag36h11", 2, 2.0f, 0.0f, 1);
}

AprilTagDetector::~AprilTagDetector() {
  if (td_) apriltag_detector_destroy(td_);
  if (tf_) destroy_family(tf_, family_name_);
}

void AprilTagDetector::destroy_family(apriltag_family_t* family, const std::string& name) {
  if (!family) return;
  if (name == "tag36h11") { tag36h11_destroy(family); return; }
  if (name == "tag25h9") { tag25h9_destroy(family); return; }
  if (name == "tag16h5") { tag16h5_destroy(family); return; }
  throw std::runtime_error("Unsupported tag family for destroy: " + name);
}

void AprilTagDetector::configure(const std::string& family,
                                 int threads,
                                 float decimate,
                                 float blur,
                                 int refine_edges) {
  if (td_) apriltag_detector_destroy(td_);
  if (tf_) destroy_family(tf_, family_name_);
  td_ = apriltag_detector_create();
  if (!td_) throw std::runtime_error("Failed to recreate apriltag detector");
  tf_ = make_family(family);
  family_name_ = family;
  apriltag_detector_add_family(td_, tf_);

  td_->nthreads = threads;
  td_->quad_decimate = decimate;
  td_->quad_sigma = blur;
  td_->refine_edges = static_cast<bool>(refine_edges);
  // newer apriltag versions removed `refine_decode`/`refine_pose`; the
  // detector now exposes `decode_sharpening` instead.  Keep the default
  // sharpening factor for compatibility.
  td_->decode_sharpening = 0.25;
}

void AprilTagDetector::enable_pose(double fx, double fy, double cx, double cy, double tag_size_m) {
  pose_enabled_ = true;
  info_.fx = fx; info_.fy = fy; info_.cx = cx; info_.cy = cy;
  info_.tagsize = tag_size_m;
}

std::vector<TagDetection> AprilTagDetector::detect(const cv::Mat& gray) {
  if (gray.empty()) return {};
  if (gray.type() != CV_8UC1) throw std::runtime_error("detect() expects CV_8UC1");

  image_u8_t im{ gray.cols, gray.rows, gray.cols, gray.data };

  zarray_t* dets = apriltag_detector_detect(td_, &im);
  std::vector<TagDetection> out;
  int n = zarray_size(dets);
  out.reserve(static_cast<size_t>(n));

  for (int i = 0; i < n; i++) {
    apriltag_detection_t* det = nullptr;
    zarray_get(dets, i, &det);
    if (!det) continue;

    TagDetection td;
    td.id = det->id;
    td.family = det->family ? std::string(det->family->name) : "unknown";
    td.center = cv::Point2d(det->c[0], det->c[1]);
    for (int k = 0; k < 4; k++) td.p[k] = cv::Point2d(det->p[k][0], det->p[k][1]);
    td.hamming = det->hamming;
    td.decision_margin = det->decision_margin;
    if (det->H && det->H->nrows == 3 && det->H->ncols == 3) {
      for (int k = 0; k < 9; ++k) td.homography[k] = det->H->data[k];
    }

    if (pose_enabled_) {
      info_.det = det;
      apriltag_pose_t pose;
      td.reproj_err = estimate_tag_pose(&info_, &pose);
      if (std::isfinite(td.reproj_err)) {
        td.has_pose = true;
        td.t = cv::Vec3d(pose.t->data[0], pose.t->data[1], pose.t->data[2]);
        td.R = cv::Matx33d(
          pose.R->data[0], pose.R->data[1], pose.R->data[2],
          pose.R->data[3], pose.R->data[4], pose.R->data[5],
          pose.R->data[6], pose.R->data[7], pose.R->data[8]
        );
      }
    }

    out.push_back(td);
  }

  apriltag_detections_destroy(dets);
  return out;
}
