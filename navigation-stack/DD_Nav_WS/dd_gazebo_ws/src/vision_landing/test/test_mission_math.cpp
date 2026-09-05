// Unit tests for the two transforms that decide where the aircraft ends up.
//
// These are the functions whose failure mode is not a crash but a landing in
// the wrong place, which is exactly the kind of bug a flight test is a very
// expensive way to find.

#include <gtest/gtest.h>

#include "vision_landing/mission_math.hpp"

#include <cmath>

using vision_landing::camera_offset_body;
using vision_landing::body_offset_range;
using vision_landing::body_offset_to_ned;
using vision_landing::gps_to_local_ned;
using vision_landing::gps_range_m;
using vision_landing::quat_pitch;

namespace
{
constexpr float kDeg = static_cast<float>(M_PI) / 180.0f;
constexpr float kTol = 1e-4f;
}  // namespace

// ── camera_offset_body ──────────────────────────────────────────────────────

// Camera pointing straight down: depth is pure altitude, so a tag directly
// under the aircraft has no horizontal offset, and the image-down axis maps
// onto backwards along the body.
TEST(CameraOffsetBody, StraightDownCentredTag)
{
  const auto o = camera_offset_body(0.0f, 0.0f, 5.0f, 90.0f * kDeg);
  EXPECT_NEAR(o.forward, 0.0f, kTol);
  EXPECT_NEAR(o.right, 0.0f, kTol);
}

TEST(CameraOffsetBody, StraightDownOffsetTag)
{
  // 1 m right, 2 m "down" in the image at 90 deg tilt = 2 m BEHIND the aircraft.
  const auto o = camera_offset_body(1.0f, 2.0f, 5.0f, 90.0f * kDeg);
  EXPECT_NEAR(o.forward, -2.0f, 1e-3f);
  EXPECT_NEAR(o.right, 1.0f, kTol);
}

// Camera level with the horizon: depth is pure forward range.
TEST(CameraOffsetBody, LevelCameraDepthIsForward)
{
  const auto o = camera_offset_body(0.0f, 0.0f, 8.0f, 0.0f);
  EXPECT_NEAR(o.forward, 8.0f, kTol);
  EXPECT_NEAR(o.right, 0.0f, kTol);
}

// The regression this whole file exists for: at 80 deg the -y*sin(theta) term
// contributes most of the correction. Dropping it (forward = z*cos(theta))
// would give 0.868 instead of 0.0824 — the ~0.9 m landing error the comment
// in mission_controller.cpp warns about.
TEST(CameraOffsetBody, DownTiltIncludesImageDownTerm)
{
  const float theta = 80.0f * kDeg;
  const auto o = camera_offset_body(0.0f, 0.5f, 5.0f, theta);
  const float expected = 5.0f * std::cos(theta) - 0.5f * std::sin(theta);
  EXPECT_NEAR(o.forward, expected, kTol);

  const float naive = 5.0f * std::cos(theta);
  EXPECT_GT(std::fabs(naive - o.forward), 0.4f)
    << "the image-down term must matter at a steep tilt, or this test is not "
       "testing what it claims to";
}

TEST(CameraOffsetBody, RangeIsEuclidean)
{
  const vision_landing::BodyOffset o{3.0f, 4.0f};
  EXPECT_NEAR(body_offset_range(o), 5.0f, kTol);
}

// ── body_offset_to_ned ──────────────────────────────────────────────────────

// Heading 0 = pointing north, so forward is north and right is east.
TEST(BodyOffsetToNed, HeadingNorth)
{
  float n = 0.0f, e = 0.0f;
  body_offset_to_ned(10.0f, 20.0f, {3.0f, 4.0f}, 0.0f, n, e);
  EXPECT_NEAR(n, 13.0f, kTol);
  EXPECT_NEAR(e, 24.0f, kTol);
}

// Heading 90 deg = pointing east, so forward is east and right is south.
TEST(BodyOffsetToNed, HeadingEast)
{
  float n = 0.0f, e = 0.0f;
  body_offset_to_ned(0.0f, 0.0f, {3.0f, 4.0f}, 90.0f * kDeg, n, e);
  EXPECT_NEAR(n, -4.0f, 1e-3f);
  EXPECT_NEAR(e, 3.0f, 1e-3f);
}

// Rotating the aircraft must not change how far away the tag is.
TEST(BodyOffsetToNed, RotationPreservesRange)
{
  const vision_landing::BodyOffset o{3.0f, 4.0f};
  for (float hdg = -180.0f; hdg <= 180.0f; hdg += 15.0f) {
    float n = 0.0f, e = 0.0f;
    body_offset_to_ned(0.0f, 0.0f, o, hdg * kDeg, n, e);
    EXPECT_NEAR(std::sqrt(n * n + e * e), 5.0f, 1e-3f) << "heading " << hdg;
  }
}

// ── gps_to_local_ned ────────────────────────────────────────────────────────

TEST(GpsToLocalNed, SamePointIsOrigin)
{
  float n = 0.0f, e = 0.0f;
  gps_to_local_ned(47.397742, 8.545594, 12.0f, -3.0f,
                   47.397742, 8.545594, n, e);
  EXPECT_NEAR(n, 12.0f, kTol);
  EXPECT_NEAR(e, -3.0f, kTol);
}

// One degree of latitude north, from a zero local origin.
TEST(GpsToLocalNed, LatitudeScale)
{
  float n = 0.0f, e = 0.0f;
  gps_to_local_ned(0.0, 0.0, 0.0f, 0.0f, 1.0, 0.0, n, e);
  EXPECT_NEAR(n, 111320.0f, 1.0f);
  EXPECT_NEAR(e, 0.0f, kTol);
}

// Longitude compresses with the cosine of latitude: at 60 deg north a degree
// of longitude is half what it is at the equator. Getting this backwards puts
// the aircraft twice as far east as intended at Purdue's latitude.
TEST(GpsToLocalNed, LongitudeCompressesWithLatitude)
{
  float n = 0.0f, e = 0.0f;
  gps_to_local_ned(60.0, 0.0, 0.0f, 0.0f, 60.0, 1.0, n, e);
  EXPECT_NEAR(e, 111320.0f * 0.5f, 5.0f);
  EXPECT_NEAR(n, 0.0f, kTol);
}

// A realistic delivery leg: ~150 m north-east of the launch point.
TEST(GpsToLocalNed, ShortDeliveryLeg)
{
  const double home_lat = 47.397742, home_lon = 8.545594;
  const double dest_lat = home_lat + 100.0 / 111320.0;
  const double dest_lon = home_lon +
    100.0 / (111320.0 * std::cos(home_lat * M_PI / 180.0));

  float n = 0.0f, e = 0.0f;
  gps_to_local_ned(home_lat, home_lon, 0.0f, 0.0f, dest_lat, dest_lon, n, e);
  EXPECT_NEAR(n, 100.0f, 0.1f);
  EXPECT_NEAR(e, 100.0f, 0.1f);
  EXPECT_NEAR(gps_range_m(home_lat, home_lon, dest_lat, dest_lon), 141.42f, 0.2f);
}

// The preflight range gate and the in-flight geofence must measure the same
// distance, or the mission can pass on the ground and trip in the air.
TEST(GpsRange, MatchesLocalNedMagnitude)
{
  const double home_lat = 40.4237, home_lon = -86.9212;   // West Lafayette
  const double dest_lat = 40.4260, dest_lon = -86.9180;

  float n = 0.0f, e = 0.0f;
  gps_to_local_ned(home_lat, home_lon, 0.0f, 0.0f, dest_lat, dest_lon, n, e);
  EXPECT_NEAR(gps_range_m(home_lat, home_lon, dest_lat, dest_lon),
              std::sqrt(n * n + e * e), 1e-3f);
}

// ── quat_pitch ──────────────────────────────────────────────────────────────

TEST(QuatPitch, IdentityIsLevel)
{
  const float q[4] = {1.0f, 0.0f, 0.0f, 0.0f};
  EXPECT_NEAR(quat_pitch(q), 0.0f, kTol);
}

TEST(QuatPitch, PitchedNoseDown)
{
  // -45 deg about the body y axis.
  const float half = -45.0f * kDeg / 2.0f;
  const float q[4] = {std::cos(half), 0.0f, std::sin(half), 0.0f};
  EXPECT_NEAR(quat_pitch(q), -45.0f * kDeg, 1e-3f);
}

TEST(QuatPitch, StraightDownDoesNotReturnNan)
{
  const float half = -90.0f * kDeg / 2.0f;
  const float q[4] = {std::cos(half), 0.0f, std::sin(half), 0.0f};
  const float pitch = quat_pitch(q);
  EXPECT_FALSE(std::isnan(pitch));
  EXPECT_NEAR(pitch, -90.0f * kDeg, 1e-3f);
}

// A quaternion that arrives very slightly non-unit must not produce NaN, which
// would silently poison every projection downstream of it.
TEST(QuatPitch, NonUnitQuaternionIsClamped)
{
  const float q[4] = {0.7072f, 0.0f, -0.7072f, 0.0f};   // |q| slightly > 1
  const float pitch = quat_pitch(q);
  EXPECT_FALSE(std::isnan(pitch));
  EXPECT_NEAR(pitch, -90.0f * kDeg, 1e-2f);
}
