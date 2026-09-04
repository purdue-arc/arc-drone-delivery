// lio_odom_bridge — turn FAST-LIO2's solution into a guarded map->odom
// correction, without letting it become a second authority over the aircraft.
//
// WHERE THIS SITS
//
// Today map->odom is a static identity: PX4's EKF is the only localization
// source, so `odom` is treated as globally consistent and Nav2's costmap works
// directly in `map`. That is a claim about PX4's GPS/baro solution, and it is
// wrong in exactly the way GPS is wrong — slowly, and worst near the buildings
// the aircraft is trying not to hit.
//
// FAST-LIO2 gives an independent, drift-slow estimate. The tempting move is to
// let it publish odom->base_link and be "the" state estimator. This node exists
// because that move is wrong here:
//
//   * mission_controller flies on /fmu/out/vehicle_local_position and PX4's
//     position controller closes the loop on PX4's own estimate. A ROS-side
//     estimator cannot replace that without a vision-position feed into the
//     EKF, which is a much larger change with a much worse failure mode.
//   * Two publishers of one TF link invalidates the tree and stops Nav2
//     planning. That is a rule this stack has already been bitten by.
//
// So PX4 keeps odom->base_link and keeps flying the aircraft. FAST-LIO
// corrects the map->odom link underneath it, which is the standard place for a
// SLAM solution to enter a ROS transform tree. The effect is that the costmap
// and everything drawn in `map` stay aligned to the world the lidar sees,
// while the control loop is untouched. If this node stops, map->odom simply
// stops being corrected and the stack degrades to exactly what it does today.
//
// WHY THE CORRECTION IS RATE LIMITED
//
// Because a costmap remembers. Obstacles are inserted at the map coordinates
// implied by map->odom at the moment they were seen. Step that transform and
// every previously-inserted obstacle moves relative to the aircraft at once —
// buildings smear, cleared space is re-marked, and the mission's "am I about to
// fly into something" check is evaluated against a map that just teleported.
//
// This stack has already learned the general form of that lesson from the
// setpoint path: an aircraft handed a step answers it violently, and the fix is
// to rate limit the reference rather than to smooth the check. Same rule here.
// The correction is slew limited so the map can only ever slide, and a
// correction that arrives as a jump is rejected outright rather than smoothed —
// a jump is FAST-LIO relocalizing or diverging, and neither is something to
// follow at any speed.
//
// OBSERVER FIRST. With publish_tf false (the default) this computes the whole
// correction, publishes its size on /arc/lio/correction_norm_m, and touches no
// transform. That is the honest way to earn the right to turn it on: fly the
// mission, watch how far LIO and PX4 disagree over 596 m, and only then decide
// whether the correction is worth applying in flight.
//
// THE DEGENERACY, AND WHY THIS NODE HAS TO DETECT IT ITSELF
//
// A lidar-inertial estimator needs geometry. Registering scans against a
// SINGLE plane leaves three degrees of freedom completely unconstrained — the
// two translations in the plane and the rotation about its normal. Over flat
// open ground that is exactly x, y and yaw, and they drift on IMU alone.
//
// This is not hypothetical. On 2026-09-03, parked on the pad, FAST-LIO placed
// the aircraft 118 m from a pad it had never left.
//
// The obvious defence — trust FAST-LIO's own covariance — DOES NOT WORK, and
// this was measured rather than assumed. While 118 m wrong it reported a
// standard deviation of 2.2 mm in x and 0.24 degrees in yaw. Its most
// confident axes were precisely the unobservable ones. The reason is that its
// map is built from its own drifted pose, so every new scan matches the map
// perfectly and the filter sees no residual to be uncertain about. Drift and
// self-consistency are the same thing here.
//
// So the gate below computes the scene geometry directly from the cloud,
// before the estimator has had a chance to be confident about it: fit the
// dominant plane, and ask what fraction of returns lie off it. All-on-plane
// means nothing constrains x, y or yaw, and the correction is refused no
// matter how good FAST-LIO claims it is. It is deliberately a property of the
// INPUT, since every signal derived from the solution is compromised by the
// same drift it is supposed to detect.

#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/float32.hpp>

#include <tf2/LinearMath/Transform.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2_ros/static_transform_broadcaster.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace
{
tf2::Transform to_tf2(const geometry_msgs::msg::Transform & t)
{
  return tf2::Transform(
    tf2::Quaternion(t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w),
    tf2::Vector3(t.translation.x, t.translation.y, t.translation.z));
}

tf2::Transform to_tf2(const geometry_msgs::msg::Pose & p)
{
  return tf2::Transform(
    tf2::Quaternion(p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w),
    tf2::Vector3(p.position.x, p.position.y, p.position.z));
}

geometry_msgs::msg::Transform from_tf2(const tf2::Transform & t)
{
  geometry_msgs::msg::Transform out;
  out.translation.x = t.getOrigin().x();
  out.translation.y = t.getOrigin().y();
  out.translation.z = t.getOrigin().z();
  const tf2::Quaternion q = t.getRotation().normalized();
  out.rotation.x = q.x();
  out.rotation.y = q.y();
  out.rotation.z = q.z();
  out.rotation.w = q.w();
  return out;
}

// Shortest rotation angle between two orientations, radians.
double angle_between(const tf2::Quaternion & a, const tf2::Quaternion & b)
{
  const double d = std::clamp(std::fabs(a.normalized().dot(b.normalized())), 0.0, 1.0);
  return 2.0 * std::acos(d);
}

int field_offset(const sensor_msgs::msg::PointCloud2 & c, const std::string & name)
{
  for (const auto & f : c.fields) {
    if (f.name == name && f.datatype == sensor_msgs::msg::PointField::FLOAT32) {
      return static_cast<int>(f.offset);
    }
  }
  return -1;
}

// Smallest-eigenvalue eigenvector of a symmetric 3x3, by inverse iteration.
// Used as the dominant plane's normal; a handful of iterations is plenty since
// we only need a direction, not a precise one.
tf2::Vector3 smallest_eigenvector(const double m[3][3])
{
  // Shift by the trace so the smallest eigenvalue is the one nearest zero,
  // then iterate on the adjugate — cheaper and better conditioned here than
  // inverting a matrix that is nearly singular by construction.
  double a[3][3];
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) a[i][j] = m[i][j];
  }
  // Cofactor matrix of a: its dominant eigenvector is a's smallest.
  double c[3][3];
  c[0][0] = a[1][1] * a[2][2] - a[1][2] * a[2][1];
  c[0][1] = a[0][2] * a[2][1] - a[0][1] * a[2][2];
  c[0][2] = a[0][1] * a[1][2] - a[0][2] * a[1][1];
  c[1][0] = c[0][1];
  c[1][1] = a[0][0] * a[2][2] - a[0][2] * a[2][0];
  c[1][2] = a[0][2] * a[1][0] - a[0][0] * a[1][2];
  c[2][0] = c[0][2];
  c[2][1] = c[1][2];
  c[2][2] = a[0][0] * a[1][1] - a[0][1] * a[1][0];

  tf2::Vector3 v(1.0, 1.0, 1.0);
  for (int it = 0; it < 24; ++it) {
    const tf2::Vector3 n(
      c[0][0] * v.x() + c[0][1] * v.y() + c[0][2] * v.z(),
      c[1][0] * v.x() + c[1][1] * v.y() + c[1][2] * v.z(),
      c[2][0] * v.x() + c[2][1] * v.y() + c[2][2] * v.z());
    const double len = n.length();
    if (len < 1e-18) break;
    v = n / len;
  }
  return v;
}
}  // namespace

class LioOdomBridge : public rclcpp::Node
{
public:
  LioOdomBridge() : Node("lio_odom_bridge")
  {
    map_frame_ = declare_parameter<std::string>("map_frame", "map");
    odom_frame_ = declare_parameter<std::string>("odom_frame", "odom");
    base_frame_ = declare_parameter<std::string>("base_frame", "base_link");
    lio_odom_frame_ = declare_parameter<std::string>("lio_odom_frame", "lio_odom");

    // OFF by default. Turning this on hands FAST-LIO the map->odom link, and
    // whatever starts this node must then NOT also start the static identity
    // publisher — two publishers of one link invalidates the tree.
    publish_tf_ = declare_parameter<bool>("publish_tf", false);

    // Translation from FAST-LIO's body frame (its IMU) to base_link, expressed
    // in the body frame. Rotation is assumed identity, which holds only while
    // the lidar's mount_roll/pitch/yaw are zero in livox_mid360.launch.py. On
    // the aircraft FAST-LIO's body is the Mid-360's internal IMU, roughly at
    // livox_frame, so base_link is 0.15 m above it: [0, 0, 0.15]. In SITL the
    // IMU is PX4's, at base_link: [0, 0, 0].
    body_to_base_ = declare_parameter<std::vector<double>>(
      "lio_body_to_base_link", std::vector<double>{0.0, 0.0, 0.0});
    if (body_to_base_.size() != 3) {
      RCLCPP_ERROR(get_logger(),
                   "lio_body_to_base_link needs 3 values, got %zu — using zeros.",
                   body_to_base_.size());
      body_to_base_ = {0.0, 0.0, 0.0};
    }

    // How fast the map is allowed to slide under the aircraft. Deliberately
    // slower than the aircraft flies: the correction is fixing drift that
    // accumulates over minutes, so it never needs to be fast, and every m/s
    // here is a m/s of costmap smear.
    max_slew_m_s_ = declare_parameter<double>("max_slew_m_s", 0.25);
    max_slew_deg_s_ = declare_parameter<double>("max_slew_deg_s", 2.0);

    // A correction that appears faster than this is FAST-LIO relocalizing or
    // diverging, not drift being corrected. Reject the sample; do not slew
    // toward it slowly, because slowly following a divergence still ends up
    // following it.
    max_step_m_ = declare_parameter<double>("max_step_m", 1.0);
    max_step_deg_ = declare_parameter<double>("max_step_deg", 5.0);

    // Total disagreement past which LIO is not to be believed at all. Over the
    // 596 m delivery leg a healthy correction is metres; tens of metres means
    // the estimator has lost the world, most likely to the open-ground
    // degeneracy described in the header.
    max_correction_m_ = declare_parameter<double>("max_correction_m", 20.0);

    // --- scene-geometry gate ---
    // A return is "off plane" if it is further than this from the dominant
    // plane fitted through the scan. 1.0 m is well above the sensor's noise
    // and any ground undulation, and well below the height of anything worth
    // calling structure.
    offplane_tolerance_m_ = declare_parameter<double>("offplane_tolerance_m", 1.0);
    // ...and the scan must have at least this fraction of them before FAST-LIO
    // is believed about x, y or yaw. 5% of a Mid-360 frame is ~1000 returns on
    // real structure; on flat ground it is essentially zero.
    min_offplane_fraction_ = declare_parameter<double>("min_offplane_fraction", 0.05);
    require_structure_ = declare_parameter<bool>("require_structure", true);
    scene_topic_ = declare_parameter<std::string>("scene_topic", "/livox/points");
    // Scans go stale like everything else. If the cloud stops, we cannot know
    // whether there is structure, and "cannot know" must not read as "yes".
    scene_timeout_sec_ = declare_parameter<double>("scene_timeout_sec", 1.0);

    lio_timeout_sec_ = declare_parameter<double>("lio_timeout_sec", 1.0);
    publish_rate_hz_ = declare_parameter<double>("publish_rate_hz", 20.0);
    // How long to wait for PX4's odom->base_link at the LIO sample's own
    // timestamp. Looking up "latest instead" would silently pair poses from
    // different moments, which during a 4 m/s transit is meaningless.
    tf_timeout_sec_ = declare_parameter<double>("tf_timeout_sec", 0.05);

    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);
    tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
    static_broadcaster_ = std::make_unique<tf2_ros::StaticTransformBroadcaster>(*this);

    engaged_pub_ = create_publisher<std_msgs::msg::Bool>("/arc/lio/engaged", 10);
    norm_pub_ = create_publisher<std_msgs::msg::Float32>("/arc/lio/correction_norm_m", 10);
    // The fraction of returns off the dominant plane, published whether or not
    // the gate is enforced. This is the number that says whether a route has
    // enough geometry for a lidar estimator at all, and it is worth recording
    // on every flight even when nothing acts on it.
    scene_pub_ = create_publisher<std_msgs::msg::Float32>(
      "/arc/lio/offplane_fraction", 10);

    sub_ = create_subscription<nav_msgs::msg::Odometry>(
      "/fast_lio/odometry", rclcpp::QoS(20),
      std::bind(&LioOdomBridge::on_lio, this, std::placeholders::_1));
    scene_sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
      scene_topic_, rclcpp::SensorDataQoS(),
      std::bind(&LioOdomBridge::on_scene, this, std::placeholders::_1));

    timer_ = create_wall_timer(
      std::chrono::duration<double>(1.0 / std::max(1.0, publish_rate_hz_)),
      std::bind(&LioOdomBridge::on_timer, this));

    RCLCPP_INFO(get_logger(),
                "LIO bridge up. publish_tf=%s — %s",
                publish_tf_ ? "TRUE" : "false",
                publish_tf_
                  ? "THIS NODE OWNS map->odom. The static identity publisher must not be running."
                  : "observer only: computing the correction and publishing its size, touching no transform.");
  }

private:
  // Fit the dominant plane through the scan and record what fraction of the
  // returns lie off it. Frame-independent, so the cloud is used exactly as it
  // arrives — no transform, and nothing to go wrong if TF is not up yet.
  void on_scene(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
  {
    const int ox = field_offset(*msg, "x");
    const int oy = field_offset(*msg, "y");
    const int oz = field_offset(*msg, "z");
    if (ox < 0 || oy < 0 || oz < 0) return;

    const size_t n = static_cast<size_t>(msg->width) * msg->height;
    const size_t step = msg->point_step;
    if (n < 50 || msg->data.size() < n * step) return;

    // A real Mid-360 frame is ~20,000 points and this runs at 10 Hz on the
    // companion computer. The plane fit does not get better past a couple of
    // thousand samples, so stride rather than read them all.
    const size_t stride = std::max<size_t>(1, n / 2000);

    double sx = 0, sy = 0, sz = 0;
    size_t m = 0;
    for (size_t i = 0; i < n; i += stride) {
      const uint8_t * p = msg->data.data() + i * step;
      float x, y, z;
      std::memcpy(&x, p + ox, 4);
      std::memcpy(&y, p + oy, 4);
      std::memcpy(&z, p + oz, 4);
      if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) continue;
      sx += x; sy += y; sz += z; ++m;
    }
    if (m < 50) return;
    const double cx = sx / m, cy = sy / m, cz = sz / m;

    double cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    for (size_t i = 0; i < n; i += stride) {
      const uint8_t * p = msg->data.data() + i * step;
      float x, y, z;
      std::memcpy(&x, p + ox, 4);
      std::memcpy(&y, p + oy, 4);
      std::memcpy(&z, p + oz, 4);
      if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) continue;
      const double dx = x - cx, dy = y - cy, dz = z - cz;
      cov[0][0] += dx * dx; cov[0][1] += dx * dy; cov[0][2] += dx * dz;
      cov[1][1] += dy * dy; cov[1][2] += dy * dz;
      cov[2][2] += dz * dz;
    }
    cov[1][0] = cov[0][1]; cov[2][0] = cov[0][2]; cov[2][1] = cov[1][2];
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) cov[i][j] /= static_cast<double>(m);
    }

    const tf2::Vector3 normal = smallest_eigenvector(cov);
    size_t off = 0;
    for (size_t i = 0; i < n; i += stride) {
      const uint8_t * p = msg->data.data() + i * step;
      float x, y, z;
      std::memcpy(&x, p + ox, 4);
      std::memcpy(&y, p + oy, 4);
      std::memcpy(&z, p + oz, 4);
      if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) continue;
      const double d = normal.x() * (x - cx) + normal.y() * (y - cy)
                     + normal.z() * (z - cz);
      if (std::fabs(d) > offplane_tolerance_m_) ++off;
    }

    offplane_fraction_ = static_cast<double>(off) / static_cast<double>(m);
    last_scene_ = now();
    have_scene_ = true;

    std_msgs::msg::Float32 f;
    f.data = static_cast<float>(offplane_fraction_);
    scene_pub_->publish(f);
  }

  // True only when we have a recent scan AND it contains real structure.
  // Absence of evidence is treated as absence of structure on purpose.
  bool scene_has_structure()
  {
    if (!require_structure_) return true;
    if (!have_scene_) return false;
    if ((now() - last_scene_).seconds() > scene_timeout_sec_) return false;
    return offplane_fraction_ >= min_offplane_fraction_;
  }

  void on_lio(const nav_msgs::msg::Odometry::SharedPtr msg)
  {
    if (diverged_) return;

    // Refuse to take a correction from a scan that cannot constrain one. This
    // runs BEFORE the jump and divergence checks, because those compare LIO
    // against itself and against PX4 — neither can tell you the estimator was
    // never observable in the first place.
    if (!scene_has_structure()) {
      if (!degenerate_warned_) {
        RCLCPP_WARN(get_logger(),
                    "Scene is a single plane (%.1f%% of returns off it, need "
                    "%.1f%%). x, y and yaw are unobservable, so FAST-LIO's "
                    "answer is not being used — however confident it looks. "
                    "Holding the last correction.",
                    offplane_fraction_ * 100.0, min_offplane_fraction_ * 100.0);
        degenerate_warned_ = true;
      }
      // Keep the clock alive: the estimator IS running and current, it just
      // has nothing to see. Letting it look stale would be the wrong diagnosis.
      last_lio_ = now();
      return;
    }
    if (degenerate_warned_) {
      RCLCPP_INFO(get_logger(),
                  "Structure back in view (%.1f%% of returns off the dominant "
                  "plane). Accepting FAST-LIO corrections again.",
                  offplane_fraction_ * 100.0);
      degenerate_warned_ = false;
    }

    // PX4's pose for base_link at the SAME instant as this LIO sample.
    geometry_msgs::msg::TransformStamped px4;
    try {
      px4 = tf_buffer_->lookupTransform(
        odom_frame_, base_frame_, msg->header.stamp,
        rclcpp::Duration::from_seconds(tf_timeout_sec_));
    } catch (const tf2::TransformException & e) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
                           "No %s->%s at the LIO sample's time (%s). Holding the "
                           "last correction and skipping this sample.",
                           odom_frame_.c_str(), base_frame_.c_str(), e.what());
      return;
    }

    const tf2::Transform P = to_tf2(px4.transform);          // odom  -> base_link (PX4)
    const tf2::Transform L = to_tf2(msg->pose.pose);         // lio_odom -> lio_body
    const tf2::Transform X(tf2::Quaternion::getIdentity(),
                           tf2::Vector3(body_to_base_[0], body_to_base_[1],
                                        body_to_base_[2]));  // lio_body -> base_link

    if (!anchored_) {
      // Anchor LIO's world to the mission's. FAST-LIO starts its frame at
      // wherever the aircraft was when it initialized, with an arbitrary yaw,
      // so without this its map is rotated off the pad by the heading at
      // startup and nothing drawn in it lines up with the costmap.
      anchor_ = P * X.inverse() * L.inverse();               // map -> lio_odom
      anchored_ = true;

      geometry_msgs::msg::TransformStamped a;
      a.header.stamp = msg->header.stamp;
      a.header.frame_id = map_frame_;
      a.child_frame_id = lio_odom_frame_;
      a.transform = from_tf2(anchor_);
      // Static, and published in BOTH modes: it is what makes FAST-LIO's
      // clouds and path renderable in the mission's map frame, which is the
      // whole value of observer mode. It never conflicts with map->odom
      // because lio_odom is a separate child of map.
      static_broadcaster_->sendTransform(a);
      RCLCPP_INFO(get_logger(),
                  "Anchored %s->%s at (%.2f, %.2f, %.2f).",
                  map_frame_.c_str(), lio_odom_frame_.c_str(),
                  anchor_.getOrigin().x(), anchor_.getOrigin().y(),
                  anchor_.getOrigin().z());
    }

    // What LIO thinks base_link is, in map; and therefore what map->odom would
    // have to be for PX4's odom->base_link to agree with it.
    const tf2::Transform lio_base_in_map = anchor_ * L * X;
    const tf2::Transform target = lio_base_in_map * P.inverse();

    if (target.getOrigin().length() > max_correction_m_) {
      diverged_ = true;
      RCLCPP_ERROR(get_logger(),
                   "FAST-LIO and PX4 disagree by %.1f m, past the %.1f m limit. "
                   "DISENGAGING: map->odom is frozen at the last good correction "
                   "and will not re-engage on its own. Most likely the lidar has "
                   "no structure in view and the solution has drifted on IMU "
                   "alone. Restart the estimator on the ground.",
                   target.getOrigin().length(), max_correction_m_);
      return;
    }

    if (have_target_) {
      const double dt_m = (target.getOrigin() - target_.getOrigin()).length();
      const double dt_deg = angle_between(target.getRotation(), target_.getRotation())
                            * 180.0 / M_PI;
      if (dt_m > max_step_m_ || dt_deg > max_step_deg_) {
        RCLCPP_WARN(get_logger(),
                    "Rejected a %.2f m / %.1f deg jump in the correction "
                    "(limits %.2f m / %.1f deg). Drift does not arrive as a "
                    "step, so this is a relocalization, not a correction.",
                    dt_m, dt_deg, max_step_m_, max_step_deg_);
        return;
      }
    }

    target_ = target;
    have_target_ = true;
    last_lio_ = now();
  }

  void on_timer()
  {
    const rclcpp::Time t = now();

    // Publish from the first tick, as identity, BEFORE FAST-LIO has produced
    // anything. The static identity publisher is suppressed whenever this node
    // owns the link, so waiting for convergence would leave map->odom with no
    // publisher at all for the seconds the estimator takes to initialize —
    // during which Nav2's global costmap has no transform, and it fails to
    // configure rather than waiting. Identity is exactly what that publisher
    // would have said anyway.
    const bool stale = have_target_ && (t - last_lio_).seconds() > lio_timeout_sec_;
    if (stale && !stale_warned_) {
      RCLCPP_WARN(get_logger(),
                  "No FAST-LIO odometry for %.1f s. Holding the current "
                  "correction — it is not decayed back to identity, because "
                  "that would be the same step this node exists to avoid.",
                  (t - last_lio_).seconds());
      stale_warned_ = true;
    } else if (!stale) {
      stale_warned_ = false;
    }

    const double dt = last_publish_.nanoseconds() == 0
      ? 1.0 / std::max(1.0, publish_rate_hz_)
      : (t - last_publish_).seconds();
    last_publish_ = t;

    // Slide, never step. Also the reason a stale or diverged estimator is
    // harmless: current_ simply stops moving.
    if (have_target_ && !diverged_ && !stale) {
      current_ = slew_toward(current_, target_, dt);
    }

    if (publish_tf_) {
      geometry_msgs::msg::TransformStamped out;
      out.header.stamp = t;
      out.header.frame_id = map_frame_;
      out.child_frame_id = odom_frame_;
      out.transform = from_tf2(current_);
      tf_broadcaster_->sendTransform(out);
    }

    std_msgs::msg::Float32 n;
    n.data = static_cast<float>(current_.getOrigin().length());
    norm_pub_->publish(n);

    std_msgs::msg::Bool e;
    // "Engaged" means the correction is actually being trusted right now,
    // which requires a scene that can constrain it.
    e.data = publish_tf_ && have_target_ && !diverged_ && !stale
             && scene_has_structure();
    engaged_pub_->publish(e);
  }

  tf2::Transform slew_toward(const tf2::Transform & from, const tf2::Transform & to,
                             double dt) const
  {
    const tf2::Vector3 d = to.getOrigin() - from.getOrigin();
    const double max_m = max_slew_m_s_ * dt;
    const tf2::Vector3 origin = d.length() <= max_m
      ? to.getOrigin()
      : from.getOrigin() + d.normalized() * max_m;

    const double ang = angle_between(from.getRotation(), to.getRotation());
    const double max_rad = max_slew_deg_s_ * M_PI / 180.0 * dt;
    const tf2::Quaternion rot = (ang <= max_rad || ang < 1e-9)
      ? to.getRotation()
      : from.getRotation().slerp(to.getRotation(), max_rad / ang);

    return tf2::Transform(rot.normalized(), origin);
  }

  std::string map_frame_, odom_frame_, base_frame_, lio_odom_frame_;
  bool publish_tf_{false};
  std::vector<double> body_to_base_;
  double max_slew_m_s_{0.25}, max_slew_deg_s_{2.0};
  double max_step_m_{1.0}, max_step_deg_{5.0};
  double max_correction_m_{20.0};
  double lio_timeout_sec_{1.0}, publish_rate_hz_{20.0}, tf_timeout_sec_{0.05};
  double offplane_tolerance_m_{1.0}, min_offplane_fraction_{0.05};
  double scene_timeout_sec_{1.0}, offplane_fraction_{0.0};
  bool require_structure_{true}, have_scene_{false}, degenerate_warned_{false};
  std::string scene_topic_;
  rclcpp::Time last_scene_{0, 0, RCL_ROS_TIME};

  bool anchored_{false}, have_target_{false}, diverged_{false}, stale_warned_{false};
  tf2::Transform anchor_{tf2::Transform::getIdentity()};
  tf2::Transform target_{tf2::Transform::getIdentity()};
  tf2::Transform current_{tf2::Transform::getIdentity()};
  rclcpp::Time last_lio_{0, 0, RCL_ROS_TIME};
  rclcpp::Time last_publish_{0, 0, RCL_ROS_TIME};

  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
  std::unique_ptr<tf2_ros::StaticTransformBroadcaster> static_broadcaster_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr scene_sub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr engaged_pub_;
  rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr norm_pub_;
  rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr scene_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LioOdomBridge>());
  rclcpp::shutdown();
  return 0;
}
