#pragma once
#include <string>
#include <optional>

enum class InputSourceType {
  GST_PIPELINE,
  V4L2_DEVICE,
  VIDEO_FILE
};

struct CliOptions {
  // Input selection
  InputSourceType source = InputSourceType::GST_PIPELINE;

  // GST input (default)
  std::string gst_pipeline;

  // Device input (e.g., /dev/video0)
  int device_index = 0;

  // Video file input
  std::string video_path;

  // AprilTag params
  std::string family = "tag36h11";
  int threads = 2;
  float decimate = 2.0f;
  float blur = 0.0f;
  int refine_edges = 1;

  bool show_fps = true;
  bool show_help = false;

  // Pose params
  std::optional<std::string> calib_path;
  std::optional<double> fx, fy, cx, cy;
  std::optional<double> tag_size_m;
};

CliOptions parse_cli(int argc, char** argv);
std::string help_text();
