#include "cli.hpp"
#include "zed_gst_pipeline.hpp"

#include <sstream>
#include <stdexcept>
#include <cstring>

static std::string take_value(int& i, int argc, char** argv, const char* flag) {
  if (i + 1 >= argc) {
    std::ostringstream oss;
    oss << "Missing value for " << flag;
    throw std::runtime_error(oss.str());
  }
  return std::string(argv[++i]);
}

CliOptions parse_cli(int argc, char** argv) {
  CliOptions opt;
  opt.gst_pipeline = zas::default_udp_h264_pipeline(5000);

  for (int i = 1; i < argc; i++) {
    const char* a = argv[i];
    if (std::strcmp(a, "-h") == 0 || std::strcmp(a, "--help") == 0) { opt.show_help = true; continue; }

    // Input selection
    if (std::strcmp(a, "--gst") == 0) {
      opt.source = InputSourceType::GST_PIPELINE;
      opt.gst_pipeline = take_value(i, argc, argv, "--gst");
      continue;
    }
    if (std::strcmp(a, "--port") == 0) {
      opt.source = InputSourceType::GST_PIPELINE;
      int port = std::stoi(take_value(i, argc, argv, "--port"));
      opt.gst_pipeline = zas::default_udp_h264_pipeline(port);
      continue;
    }
    if (std::strcmp(a, "--device") == 0) {
      opt.source = InputSourceType::V4L2_DEVICE;
      opt.device_index = std::stoi(take_value(i, argc, argv, "--device"));
      continue;
    }
    if (std::strcmp(a, "--video") == 0) {
      opt.source = InputSourceType::VIDEO_FILE;
      opt.video_path = take_value(i, argc, argv, "--video");
      continue;
    }

    // Detector params
    if (std::strcmp(a, "--family") == 0) { opt.family = take_value(i, argc, argv, "--family"); continue; }
    if (std::strcmp(a, "--threads") == 0) { opt.threads = std::stoi(take_value(i, argc, argv, "--threads")); continue; }
    if (std::strcmp(a, "--decimate") == 0) { opt.decimate = std::stof(take_value(i, argc, argv, "--decimate")); continue; }
    if (std::strcmp(a, "--blur") == 0) { opt.blur = std::stof(take_value(i, argc, argv, "--blur")); continue; }
    if (std::strcmp(a, "--refine-edges") == 0) { opt.refine_edges = std::stoi(take_value(i, argc, argv, "--refine-edges")); continue; }
    if (std::strcmp(a, "--no-fps") == 0) { opt.show_fps = false; continue; }

    // Pose params
    if (std::strcmp(a, "--calib") == 0) { opt.calib_path = take_value(i, argc, argv, "--calib"); continue; }
    if (std::strcmp(a, "--fx") == 0) { opt.fx = std::stod(take_value(i, argc, argv, "--fx")); continue; }
    if (std::strcmp(a, "--fy") == 0) { opt.fy = std::stod(take_value(i, argc, argv, "--fy")); continue; }
    if (std::strcmp(a, "--cx") == 0) { opt.cx = std::stod(take_value(i, argc, argv, "--cx")); continue; }
    if (std::strcmp(a, "--cy") == 0) { opt.cy = std::stod(take_value(i, argc, argv, "--cy")); continue; }
    if (std::strcmp(a, "--tag-size") == 0) { opt.tag_size_m = std::stod(take_value(i, argc, argv, "--tag-size")); continue; }

    std::ostringstream oss;
    oss << "Unknown argument: " << a << "\n\n" << help_text();
    throw std::runtime_error(oss.str());
  }

  return opt;
}

std::string help_text() {
  std::ostringstream oss;
  oss <<
R"(zed_apriltag_streaming (pose-enabled)

Runs AprilTag detection + optional pose from one of:
  1) ZED H264 RTP/UDP stream via GStreamer (default)
  2) a local webcam (V4L2)
  3) a local video file

Usage:
  zed_apriltag [--port 5000] [--gst "<pipeline>"]
               [--device 0] | [--video path.mp4]
               [--family tag36h11]
               [--threads 2] [--decimate 2.0] [--blur 0.0] [--refine-edges 1]
               [--calib <yaml>] [--tag-size <meters>]
               [--fx <px> --fy <px> --cx <px> --cy <px>]
               [--no-fps]

Input:
  --port <p>            Convenience for the default UDP H264 pipeline (payload 96).
  --gst "<pipeline>"    Use a custom OpenCV+GStreamer pipeline string.
  --device <index>      Use a local webcam (/dev/video<index>) through OpenCV.
  --video <path>        Use a local video file through OpenCV.

Pose:
  --tag-size <meters>   Tag size in meters (required for pose).
  --calib <yaml>        OpenCV YAML calibration file with camera_matrix (K) + optional image_width/height.
  OR specify intrinsics manually:
  --fx --fy --cx --cy   Camera intrinsics (pixels).

Examples:
  # 1) UDP stream (Jetson -> laptop)
  ./zed_apriltag --port 5000

  # 2) Webcam test on laptop
  ./zed_apriltag --device 0

  # 3) Video file test
  ./zed_apriltag --video ./sample.mp4

  # Pose
  ./zed_apriltag --device 0 --calib assets/calibration_example.yaml --tag-size 0.162

)";
  return oss.str();
}
