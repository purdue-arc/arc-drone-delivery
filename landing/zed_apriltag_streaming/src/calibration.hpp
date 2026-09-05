#pragma once
#include <string>
#include <optional>

struct CameraIntrinsics {
  double fx = 0.0;
  double fy = 0.0;
  double cx = 0.0;
  double cy = 0.0;
  int width = 0;
  int height = 0;
};

std::optional<CameraIntrinsics> load_intrinsics_yaml(const std::string& path);
