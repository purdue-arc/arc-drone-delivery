// mission_math.hpp — the two coordinate transforms the mission depends on.
//
// These are pure functions of their inputs, deliberately separated from
// MissionController so they can be unit tested without a ROS graph, a flight
// controller, or a camera. They are also the two places where a sign error
// does not crash anything — it just puts the aircraft somewhere else:
//
//   camera_offset_body / body_offset_to_ned  place the aircraft over the pad
//   gps_to_local_ned                         places the aircraft over the customer
//
// See test/test_mission_math.cpp.

#ifndef VISION_LANDING__MISSION_MATH_HPP_
#define VISION_LANDING__MISSION_MATH_HPP_

#include <cmath>

namespace vision_landing
{

// Metres per degree of latitude. Constant to well within the accuracy of the
// flat-earth approximation below.
inline constexpr double kMetersPerDegLat = 111320.0;

// Tag offset resolved into the body frame (forward/right of the airframe).
struct BodyOffset
{
  float forward;
  float right;
};

// Project a camera-frame tag position onto the horizontal body plane.
//
// Camera frame (apriltag convention): x = right, y = down-in-image, z = depth.
// The camera is pitched nose-down by gimbal_abs radians, measured as a
// magnitude below horizontal (0 = looking at the horizon, pi/2 = straight
// down). Rotating the camera axes about the body-right axis gives:
//
//   forward = z*cos(theta) - y*sin(theta)
//   right   = x
//
// Dropping the -y*sin(theta) term is the classic error here: at an 80 deg
// tilt it biases the landing point by roughly 0.9 m.
inline BodyOffset camera_offset_body(float tag_x, float tag_y, float tag_z,
                                     float gimbal_abs)
{
  const float cos_t = std::cos(gimbal_abs);
  const float sin_t = std::sin(gimbal_abs);
  return BodyOffset{tag_z * cos_t - tag_y * sin_t, tag_x};
}

// Horizontal range to the tag in the body frame. Needs no EKF position, which
// is why GOTO_TAG can still converge when the position estimate is unusable.
inline float body_offset_range(const BodyOffset & o)
{
  return std::sqrt(o.forward * o.forward + o.right * o.right);
}

// Rotate a body-frame offset by the vehicle heading and add it to a known NED
// position. heading is the PX4 convention: radians clockwise from north.
inline void body_offset_to_ned(float base_north, float base_east,
                               const BodyOffset & o, float heading,
                               float & north, float & east)
{
  const float cos_psi = std::cos(heading);
  const float sin_psi = std::sin(heading);
  north = base_north + o.forward * cos_psi - o.right * sin_psi;
  east  = base_east  + o.forward * sin_psi + o.right * cos_psi;
}

// Convert a WGS-84 coordinate into the local NED frame, anchored on the
// global/local correspondence captured at mission start.
//
// Flat-earth approximation: good to well under a metre over the few-km range
// this aircraft flies, and it avoids depending on PX4's EKF origin, which can
// shift on a position reset mid-flight.
inline void gps_to_local_ned(double home_lat, double home_lon,
                             float home_north, float home_east,
                             double lat, double lon,
                             float & north, float & east)
{
  const double dlat = lat - home_lat;
  const double dlon = lon - home_lon;
  north = home_north + static_cast<float>(dlat * kMetersPerDegLat);
  east  = home_east  + static_cast<float>(dlon * kMetersPerDegLat *
                                          std::cos(home_lat * M_PI / 180.0));
}

// Great-circle-free horizontal range between two WGS-84 coordinates, using the
// same flat-earth model as gps_to_local_ned so the preflight range check and
// the in-flight navigation cannot disagree.
inline float gps_range_m(double from_lat, double from_lon,
                         double to_lat, double to_lon)
{
  float n, e;
  gps_to_local_ned(from_lat, from_lon, 0.0f, 0.0f, to_lat, to_lon, n, e);
  return std::sqrt(n * n + e * e);
}

// Pitch angle (radians, negative = nose down) of a PX4 quaternion given in
// [w, x, y, z] order — the layout of GimbalDeviceAttitudeStatus.q.
//
// Clamped before asin because a slightly non-unit quaternion off the wire can
// push the argument past 1.0 and return NaN, which would silently poison every
// downstream projection.
inline float quat_pitch(const float q[4])
{
  const float sin_pitch = 2.0f * (q[0] * q[2] - q[3] * q[1]);
  const float clamped = sin_pitch > 1.0f ? 1.0f : (sin_pitch < -1.0f ? -1.0f : sin_pitch);
  return std::asin(clamped);
}

}  // namespace vision_landing

#endif  // VISION_LANDING__MISSION_MATH_HPP_
