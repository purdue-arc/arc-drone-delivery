#include "zed_gst_pipeline.hpp"
#include <sstream>

namespace zas {
std::string default_udp_h264_pipeline(int port) {
  std::ostringstream oss;
  oss
    << "udpsrc port=" << port
    << " caps=application/x-rtp,media=video,encoding-name=H264,payload=96 ! "
       "rtph264depay ! avdec_h264 ! videoconvert ! appsink drop=1";
  return oss.str();
}
} // namespace zas
