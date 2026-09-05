#include "calibration.hpp"
#include <opencv2/core.hpp>

std::optional<CameraIntrinsics> load_intrinsics_yaml(const std::string& path) {
  cv::FileStorage fs(path, cv::FileStorage::READ);
  if (!fs.isOpened()) return std::nullopt;

  cv::Mat K;
  if (!fs["camera_matrix"].empty()) fs["camera_matrix"] >> K;
  else if (!fs["K"].empty()) fs["K"] >> K;

  if (K.empty() || K.rows != 3 || K.cols != 3) return std::nullopt;
  K.convertTo(K, CV_64F);

  CameraIntrinsics intr;
  intr.fx = K.at<double>(0,0);
  intr.fy = K.at<double>(1,1);
  intr.cx = K.at<double>(0,2);
  intr.cy = K.at<double>(1,2);

  if (!fs["image_width"].empty())
    intr.width = static_cast<int>(fs["image_width"]);
  else if (!fs["width"].empty())
    intr.width = static_cast<int>(fs["width"]);

  if (!fs["image_height"].empty())
    intr.height = static_cast<int>(fs["image_height"]);
  else if (!fs["height"].empty())
    intr.height = static_cast<int>(fs["height"]);

  return intr;
}
