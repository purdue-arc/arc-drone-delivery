#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <px4_msgs/msg/trajectory_setpoint.hpp>
#include <px4_msgs/msg/offboard_control_mode.hpp>
#include <px4_msgs/msg/vehicle_command.hpp>
#include <px4_msgs/msg/vehicle_status.hpp>
#include <px4_msgs/msg/vehicle_local_position.hpp>
#include <px4_msgs/msg/vehicle_land_detected.hpp>
#include <px4_msgs/msg/battery_status.hpp>
#include <px4_msgs/msg/vehicle_global_position.hpp>
#include <px4_msgs/msg/gimbal_device_attitude_status.hpp>
#include <std_msgs/msg/float64.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/string.hpp>
#include <nav_msgs/msg/path.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <cmath>

#include "vision_landing/mission_math.hpp"

// Search levels: (altitude above home in m, gimbal tilt in rad — negative = looking down)
// Drone descends through levels until the AprilTag is found.
struct SearchLevel { float height_m; float gimbal_rad; };
static const SearchLevel LEVELS[] = {
  { 5.0f, -1.396f },   // 5 m,  80° down
  { 3.0f, -1.222f },   // 3 m,  70° down
  { 2.0f, -1.047f },   // 2 m,  60° down
  { 1.0f, -0.785f },   // 1 m,  45° down  (minimum altitude)
};
static constexpr int    N_LEVELS  = 4;
static constexpr float  YAW_RATE  = 0.01f;   // rad per 50 ms tick
static constexpr float  FULL_CIRCLE = 2.0f * static_cast<float>(M_PI);
static constexpr float  TICK_SEC  = 0.05f;

// IDLE            — passive; wait for operator start + preflight checks
// WARMUP          — stream setpoints, request OFFBOARD + arm, wait for both
// TAKEOFF         — climb to transit altitude
// TRANSIT         — fly to the delivery GPS waypoint (obstacle-aware)
// DELIVER         — hover at the address and lower the package on the winch
// SECURE_PAYLOAD  — hold and retract the winch before a non-urgent failsafe
// RETURN          — climb and fly back to the launch point
// SEARCH/GOTO_TAG — find and centre on the HOME charging pad
// LAND            — precision descent onto the charging pad
// FAILSAFE_LAND   — hand control to PX4 AUTO.LAND, go passive
// PILOT_OVERRIDE  — pilot took the vehicle; never command again
// LANDED          — terminal
//
// A delivery runs TAKEOFF -> TRANSIT -> DELIVER -> RETURN -> SEARCH ->
// GOTO_TAG -> LAND -> LANDED. The aircraft only ever lands back home, to
// charge: the customer drop is a winch lower from a hover, so no marker is
// needed at the address and the rotors stay clear of people and property.
// With no waypoint set, TRANSIT and DELIVER are skipped and the mission
// degrades to the validated search-and-land behaviour.
enum class State { IDLE, WARMUP, TAKEOFF, TRANSIT, SEARCH, GOTO_TAG, DELIVER,
                   SECURE_PAYLOAD, RETURN, LAND, FAILSAFE_LAND, PILOT_OVERRIDE,
                   LANDED };

static const char * state_name(State s)
{
  switch (s) {
    case State::IDLE:           return "IDLE";
    case State::WARMUP:         return "WARMUP";
    case State::TAKEOFF:        return "TAKEOFF";
    case State::TRANSIT:        return "TRANSIT";
    case State::SEARCH:         return "SEARCH";
    case State::GOTO_TAG:       return "GOTO_TAG";
    case State::DELIVER:        return "DELIVER";
    case State::SECURE_PAYLOAD: return "SECURE_PAYLOAD";
    case State::RETURN:         return "RETURN";
    case State::LAND:           return "LAND";
    case State::FAILSAFE_LAND:  return "FAILSAFE_LAND";
    case State::PILOT_OVERRIDE: return "PILOT_OVERRIDE";
    case State::LANDED:         return "LANDED";
  }
  return "UNKNOWN";
}

class MissionController : public rclcpp::Node
{
public:
  MissionController() : Node("mission_controller"), state_(State::IDLE)
  {
    require_start_          = declare_parameter("require_start", true);
    require_valid_position_ = declare_parameter("require_valid_position", true);
    takeoff_height_m_       = static_cast<float>(declare_parameter("takeoff_height_m", 5.0));
    accept_radius_m_        = static_cast<float>(declare_parameter("accept_radius_m", 0.5));
    descent_rate_mps_       = static_cast<float>(declare_parameter("descent_rate_mps", 0.2));
    battery_min_remaining_  = static_cast<float>(declare_parameter("battery_min_remaining", 0.25));
    takeoff_timeout_sec_    = declare_parameter("takeoff_timeout_sec", 30.0);
    goto_timeout_sec_       = declare_parameter("goto_timeout_sec", 30.0);
    max_search_time_sec_    = declare_parameter("max_search_time_sec", 180.0);
    tag_lost_abort_sec_     = declare_parameter("tag_lost_abort_sec", 2.0);
    land_commit_height_m_   = static_cast<float>(declare_parameter("land_commit_height_m", 1.0));
    max_land_retries_       = static_cast<int>(declare_parameter("max_land_retries", 2));
    // How long PX4's ground_contact / maybe_landed must hold, below the commit
    // height, before touchdown is called without a full `landed` flag.
    contact_confirm_sec_    = declare_parameter("contact_confirm_sec", 2.0);
    // Touchdown by stalled descent: we are commanding the aircraft down at
    // descent_rate_mps and it has stopped descending. Held this long, near the
    // ground, that means it is ON the ground. See the LAND state.
    descent_stall_sec_      = declare_parameter("descent_stall_sec", 3.0);
    descent_stall_vz_       = static_cast<float>(
      declare_parameter("descent_stall_vz", 0.05));
    descent_stall_height_m_ = static_cast<float>(
      declare_parameter("descent_stall_height_m", 0.30));
    // Backstop: hand the landing to PX4 if nothing else has concluded it.
    // Was 90 s, which made every landing take a minute and a half; the stalled
    // -descent handoff normally fires long before this.
    land_handoff_sec_ = declare_parameter("land_handoff_sec", 25.0);

    // ── delivery ────────────────────────────────────────────────────────────
    // Destination as GPS. NaN (the default) means "no delivery" — the mission
    // then behaves exactly like the validated search-and-land.
    delivery_lat_       = declare_parameter("delivery_lat", NAN);
    delivery_lon_       = declare_parameter("delivery_lon", NAN);
    transit_height_m_   = static_cast<float>(declare_parameter("transit_height_m", 15.0));
    transit_speed_mps_  = static_cast<float>(declare_parameter("transit_speed_mps", 4.0));
    transit_accept_m_   = static_cast<float>(declare_parameter("transit_accept_m", 2.0));
    // How close to the GPS point the aircraft must be before it starts
    // descending to winch height. Tighter than transit_accept_m so the
    // descent happens over the address, not on the way in.
    descend_radius_m_   = static_cast<float>(declare_parameter("descend_radius_m", 1.0));
    transit_timeout_sec_= declare_parameter("transit_timeout_sec", 300.0);
    // A FIXED transit timeout cannot serve both a 100 m hop and a 600 m leg.
    // At 300 s the 596 m Purdue route timed out 58 m short of the delivery
    // point, having flown 520 m perfectly well — the leg was fine, the clock
    // was wrong. The deadline is therefore derived from the distance actually
    // being flown, with transit_timeout_sec as a floor for short legs.
    //
    // The margin is large because the commanded speed is not the achieved
    // speed: the aircraft chases a setpoint a few metres ahead and PX4 decides
    // how fast to close it, which came out near 1.7 m/s for a 4 m/s command.
    // A detour around an obstacle costs more still, and climbing over one
    // costs more again — a run that had to climb from 15 m to 39 m and back
    // finished 31 m short on a 3.5x margin.
    transit_timeout_margin_ = declare_parameter("transit_timeout_margin", 5.0);
    // Hover altitude for the winch drop. The aircraft stays here and lowers
    // the package; it never lands at the delivery address.
    winch_hover_height_m_ = static_cast<float>(declare_parameter("winch_hover_height_m", 12.0));
    release_settle_sec_ = declare_parameter("release_settle_sec", 3.0);

    // ── flight envelope ─────────────────────────────────────────────────────
    // Enforced on EVERY tick of every active state, not just at preflight. A
    // preflight-only range check cannot catch a runaway setpoint, a GPS jump,
    // or a delivery point that was fine on the ground and is not fine in the
    // air. This is the companion-computer layer of the fence; it does NOT
    // replace the PX4 GF_* geofence, which keeps working when this node dies
    // — see config/px4/README.md.
    max_range_m_        = static_cast<float>(declare_parameter("max_range_m", 2000.0));
    max_altitude_m_     = static_cast<float>(declare_parameter("max_altitude_m", 40.0));

    // How long the aircraft will hold station reeling the winch back in before
    // a non-urgent failsafe descends anyway. Past this it drops the load
    // rather than landing on a dangling cable.
    winch_secure_timeout_sec_ = declare_parameter("winch_secure_timeout_sec", 25.0);

    // Obstacle avoidance. When true, TRANSIT follows the path planned by
    // Nav2 (via nav2_path_bridge) instead of a straight line.
    use_nav2_          = declare_parameter("use_nav2", false);
    path_stale_sec_    = declare_parameter("path_stale_sec", 5.0);
    path_lookahead_m_  = static_cast<float>(declare_parameter("path_lookahead_m", 4.0));

    // What to do when obstacle avoidance was requested but no plan arrives.
    //
    // A plan that goes stale mid-transit is a hiccup: the aircraft has already
    // flown part of a vetted route, so continuing on the straight line and
    // warning is the right trade. Never having received a plan at all is a
    // different thing entirely — it means the planner, the costmap or the
    // lidar is absent, and the "obstacle-avoiding" transit would be a blind
    // straight line for its whole length. That is refused by default.
    require_plan_to_transit_ =
      declare_parameter("require_plan_to_transit", true);
    plan_wait_timeout_sec_ = declare_parameter("plan_wait_timeout_sec", 30.0);

    // How far ahead of the aircraft a Nav2 goal may be placed.
    //
    // Nav2's global costmap is a ROLLING WINDOW centred on the vehicle, so it
    // only ever covers +/- half its width. Handing it the delivery point
    // directly works only while that point is inside the window; past that
    // the planner rejects the goal outright —
    //   "Goal Coordinates of (39.00, 99.01) was outside bounds"
    // — no path is ever produced, and a delivery beyond half the costmap
    // width can never be planned no matter how long the mission waits.
    //
    // So the goal is a CARROT: the delivery point when it is close enough,
    // otherwise a point this far along the bearing to it, re-issued as the
    // aircraft advances. Keep it comfortably inside half the costmap width
    // (currently 120 m, so 60 m of reach) with margin for the vehicle
    // drifting off the window centre.
    nav2_goal_max_range_m_ =
      static_cast<float>(declare_parameter("nav2_goal_max_range_m", 45.0));
    // Re-publish the carrot once the aircraft has closed this much ground.
    nav2_goal_refresh_m_ =
      static_cast<float>(declare_parameter("nav2_goal_refresh_m", 15.0));

    // ── obstacle safety ─────────────────────────────────────────────────────
    // The aircraft must not fly into anything the lidar has seen. Nav2 plans
    // around obstacles, but a plan is a statement about the past: it was clear
    // when the planner was asked. These checks re-test it continuously against
    // the live costmap, and refuse to move when it no longer holds.
    //
    // Cost threshold on Nav2's published OccupancyGrid (0..100, -1 unknown).
    // 90 catches both LETHAL and the INSCRIBED ring around it, so the aircraft
    // keeps the inflation radius rather than shaving it.
    obstacle_cost_threshold_ =
      static_cast<int>(declare_parameter("obstacle_cost_threshold", 90));
    // A costmap older than this is not evidence of anything.
    costmap_stale_sec_ = declare_parameter("costmap_stale_sec", 5.0);
    // Refuse to move at all when the costmap is missing or stale. This is the
    // "never fly into a detected obstacle" guarantee: with no costmap there is
    // nothing to check the path against, so there is no such guarantee, so the
    // aircraft holds. Set false only for a deliberate no-lidar flight.
    require_costmap_to_fly_ = declare_parameter("require_costmap_to_fly", true);
    // How far ahead the committed segment is re-checked each tick.
    lookahead_check_m_ =
      static_cast<float>(declare_parameter("lookahead_check_m", 12.0));
    // Escape from a hold that will not clear.
    //
    // Holding when the route is blocked is right, but holding FOREVER is not a
    // mission. The 2026-09-02 flight found the deadlock: the aircraft ended up
    // inside the 2D footprint of a building taller than its transit altitude,
    // so every direction including its own position read lethal, and it hovered
    // until the leg timed out. The costmap is altitude-aware now, which stops
    // that arising — and climbing is the escape that works regardless, because
    // gaining height genuinely clears a ground obstacle and an altitude-aware
    // costmap will then show it clear.
    blocked_escape_sec_ = declare_parameter("blocked_escape_sec", 10.0);
    escape_climb_m_ =
      static_cast<float>(declare_parameter("escape_climb_m", 5.0));
    // Shortest leg worth asking Nav2 to plan. Below this the move is direct
    // but still costmap-checked — see fly_guided_leg.
    min_route_m_ = static_cast<float>(declare_parameter("min_route_m", 8.0));
    // How long a destination may read as blocked before the leg is abandoned.
    target_blocked_abort_sec_ = declare_parameter("target_blocked_abort_sec", 20.0);
    // How long the best distance achieved may stagnate before giving up.
    no_progress_sec_ = declare_parameter("no_progress_sec", 90.0);

    // PX4 topic names.
    //
    // PX4 v1.16's uxrce_dds_client appends a version suffix at runtime to
    // exactly those messages that have a versioned definition in
    // msg/versioned/, and leaves every other topic bare. The resulting set is
    // MIXED, and the inconsistency below is correct, not an oversight:
    //     /fmu/out/vehicle_status_v2
    //     /fmu/out/vehicle_local_position_v1
    //     /fmu/out/battery_status_v1
    //     /fmu/out/vehicle_land_detected          (no suffix)
    //     /fmu/out/vehicle_global_position        (no suffix)
    //     /fmu/out/gimbal_device_attitude_status  (no suffix)
    //
    // dds_topics.yaml lists the BASE names and is NOT what appears on the
    // wire — reading it alone will convince you these should all be bare, and
    // the mission will then sit in preflight forever receiving nothing, which
    // is indistinguishable from a dead DDS link. Verified empirically against
    // the running firmware; check any new aircraft the same way:
    //   ros2 topic list | grep fmu/out    (or: make check-px4-topics)
    status_topic_ = declare_parameter<std::string>(
      "status_topic", "/fmu/out/vehicle_status_v2");
    local_pos_topic_ = declare_parameter<std::string>(
      "local_position_topic", "/fmu/out/vehicle_local_position_v1");
    land_detected_topic_ = declare_parameter<std::string>(
      "land_detected_topic", "/fmu/out/vehicle_land_detected");
    battery_topic_ = declare_parameter<std::string>(
      "battery_topic", "/fmu/out/battery_status_v1");
    const auto & status_topic        = status_topic_;
    const auto & local_pos_topic     = local_pos_topic_;
    const auto & land_detected_topic = land_detected_topic_;
    const auto & battery_topic       = battery_topic_;

    // SensorDataQoS (BEST_EFFORT) to match the perception publisher. A
    // RELIABLE subscription here is silently incompatible with a BEST_EFFORT
    // publisher: DDS drops the match and no detection ever arrives, so the
    // mission searches forever while perception looks healthy.
    pose_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "/landing_target_pose", rclcpp::SensorDataQoS(),
      std::bind(&MissionController::pose_callback, this, std::placeholders::_1));
    status_sub_ = create_subscription<px4_msgs::msg::VehicleStatus>(
      status_topic, rclcpp::SensorDataQoS(),
      std::bind(&MissionController::status_callback, this, std::placeholders::_1));
    local_pos_sub_ = create_subscription<px4_msgs::msg::VehicleLocalPosition>(
      local_pos_topic, rclcpp::SensorDataQoS(),
      std::bind(&MissionController::local_pos_callback, this, std::placeholders::_1));
    land_detect_sub_ = create_subscription<px4_msgs::msg::VehicleLandDetected>(
      land_detected_topic, rclcpp::SensorDataQoS(),
      std::bind(&MissionController::land_detect_callback, this, std::placeholders::_1));
    battery_sub_ = create_subscription<px4_msgs::msg::BatteryStatus>(
      battery_topic, rclcpp::SensorDataQoS(),
      std::bind(&MissionController::battery_callback, this, std::placeholders::_1));
    start_sub_ = create_subscription<std_msgs::msg::Bool>(
      "/arc/mission/start", 10,
      std::bind(&MissionController::start_callback, this, std::placeholders::_1));
    global_pos_sub_ = create_subscription<px4_msgs::msg::VehicleGlobalPosition>(
      declare_parameter<std::string>("global_position_topic",
                                     "/fmu/out/vehicle_global_position"),
      rclcpp::SensorDataQoS(),
      std::bind(&MissionController::global_pos_callback, this, std::placeholders::_1));

    // Measured gimbal attitude. The projection from camera frame to world used
    // to assume the gimbal was exactly where it had been commanded; the BruGi
    // board stabilises on its own IMU and can lag or sit at an offset, and the
    // error maps straight into the landing point (~0.4 m per 5 deg at 5 m).
    // When this is available and fresh, the measured pitch is used instead,
    // and detections taken while the gimbal is still slewing are discarded.
    gimbal_status_sub_ = create_subscription<px4_msgs::msg::GimbalDeviceAttitudeStatus>(
      declare_parameter<std::string>("gimbal_status_topic",
                                     "/fmu/out/gimbal_device_attitude_status"),
      rclcpp::SensorDataQoS(),
      std::bind(&MissionController::gimbal_status_callback, this, std::placeholders::_1));
    gimbal_settle_rad_ = static_cast<float>(
      declare_parameter("gimbal_settle_rad", 0.087));   // ~5 deg
    gimbal_status_stale_sec_ = declare_parameter("gimbal_status_stale_sec", 1.0);
    // Smallest commanded tilt that may be used to verify the feedback.
    // Must be well outside the gimbal's rest position.
    gimbal_verify_min_rad_ = static_cast<float>(
      declare_parameter("gimbal_verify_min_rad", 0.35));   // ~20 deg


    setpoint_pub_ = create_publisher<px4_msgs::msg::TrajectorySetpoint>(
      "/fmu/in/trajectory_setpoint", 10);
    offboard_pub_ = create_publisher<px4_msgs::msg::OffboardControlMode>(
      "/fmu/in/offboard_control_mode", 10);
    command_pub_  = create_publisher<px4_msgs::msg::VehicleCommand>(
      "/fmu/in/vehicle_command", 10);
    gimbal_pub_   = create_publisher<std_msgs::msg::Float64>(
      "/gimbal_tilt_cmd", 10);
    winch_pub_ = create_publisher<std_msgs::msg::String>(
      "/arc/winch/command", 10);
    winch_state_sub_ = create_subscription<std_msgs::msg::String>(
      "/arc/winch/state", rclcpp::QoS(1).transient_local(),
      std::bind(&MissionController::winch_state_callback, this, std::placeholders::_1));
    transit_goal_pub_ = create_publisher<geometry_msgs::msg::PoseStamped>(
      "/arc/transit/goal", rclcpp::QoS(1).transient_local());
    transit_path_sub_ = create_subscription<nav_msgs::msg::Path>(
      "/arc/transit/path", 10,
      std::bind(&MissionController::path_callback, this, std::placeholders::_1));
    // The costmap itself, so a planned path can be CHECKED rather than
    // trusted. Nav2 plans against the costmap it had when asked; between then
    // and flying it, the lidar can reveal something new. Latched to match the
    // costmap publisher.
    costmap_sub_ = create_subscription<nav_msgs::msg::OccupancyGrid>(
      declare_parameter<std::string>("costmap_topic",
                                     "/global_costmap/costmap"),
      rclcpp::QoS(1).transient_local(),
      std::bind(&MissionController::costmap_callback, this, std::placeholders::_1));

    // Mission telemetry. Latched so an operator tool that attaches mid-flight
    // immediately learns the state instead of waiting for the next tick.
    // Console logs are not telemetry: after a flight this topic, bagged
    // alongside /fmu/out/*, is the only record of WHY the controller did what
    // the PX4 log shows it doing.
    state_pub_ = create_publisher<std_msgs::msg::String>(
      "/arc/mission/state", rclcpp::QoS(1).transient_local());

    // Stamp the initial state. Without this, state_entered_ stays at the epoch
    // and the first telemetry reports a time-in-state of ~1.8 billion seconds.
    state_entered_ = now();

    timer_ = create_wall_timer(
      std::chrono::milliseconds(50),
      std::bind(&MissionController::control_loop, this));
    state_timer_ = create_wall_timer(
      std::chrono::seconds(1),
      std::bind(&MissionController::publish_state, this));

    if (require_start_) {
      RCLCPP_INFO(get_logger(),
        "Mission controller IDLE — start with: "
        "ros2 topic pub --once /arc/mission/start std_msgs/msg/Bool \"{data: true}\"");
    } else {
      RCLCPP_WARN(get_logger(), "require_start=false — will launch as soon as preflight passes");
    }
  }

private:
  // ── callbacks ────────────────────────────────────────────────────────────────
  void pose_callback(const geometry_msgs::msg::PoseStamped::SharedPtr msg)
  {
    tag_detected_  = true;
    tag_ever_seen_ = true;
    tag_x_ = msg->pose.position.x;
    tag_y_ = msg->pose.position.y;
    tag_z_ = msg->pose.position.z;
    last_tag_time_ = now();
  }

  void status_callback(const px4_msgs::msg::VehicleStatus::SharedPtr msg)
  {
    armed_           = (msg->arming_state == px4_msgs::msg::VehicleStatus::ARMING_STATE_ARMED);
    nav_state_       = msg->nav_state;
    status_received_ = true;
    if (nav_state_ == px4_msgs::msg::VehicleStatus::NAVIGATION_STATE_OFFBOARD)
      offboard_seen_ = true;
  }

  void local_pos_callback(const px4_msgs::msg::VehicleLocalPosition::SharedPtr msg)
  {
    current_ned_x_   = msg->x;
    current_ned_y_   = msg->y;
    current_ned_z_   = msg->z;
    current_heading_ = msg->heading;
    current_vz_      = msg->vz;
    // Measured clearance to the ground, when a range sensor is actually
    // providing it. This is the only height figure in the system that does not
    // drift: height_above_home() is an EKF estimate referenced to wherever the
    // aircraft took off, and barometric drift over a long mission moves it.
    dist_bottom_       = msg->dist_bottom;
    dist_bottom_valid_ = msg->dist_bottom_valid &&
      (msg->dist_bottom_sensor_bitfield &
       px4_msgs::msg::VehicleLocalPosition::DIST_BOTTOM_SENSOR_RANGE) != 0;
    pos_valid_       = msg->xy_valid && msg->z_valid;
    pos_received_    = true;
    last_pos_time_   = now();
  }

  void land_detect_callback(const px4_msgs::msg::VehicleLandDetected::SharedPtr msg)
  {
    landed_detected_  = msg->landed;
    // PX4 stages its land detection: ground_contact -> maybe_landed -> landed.
    // The last stage requires the thrust setpoint to fall away, which does not
    // happen while we are still holding an OFFBOARD setpoint — see the note in
    // the LAND state. The earlier stages do trip, so they are kept as a
    // secondary touchdown signal.
    ground_contact_   = msg->ground_contact;
    maybe_landed_     = msg->maybe_landed;
    if (!(ground_contact_ || maybe_landed_)) contact_since_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
    else if (contact_since_.nanoseconds() == 0) contact_since_ = now();
  }

  void battery_callback(const px4_msgs::msg::BatteryStatus::SharedPtr msg)
  {
    battery_warning_   = msg->warning;
    battery_remaining_ = msg->remaining;
    battery_received_  = true;
  }

  void global_pos_callback(const px4_msgs::msg::VehicleGlobalPosition::SharedPtr msg)
  {
    if (!msg->lat_lon_valid) return;
    current_lat_    = msg->lat;
    current_lon_    = msg->lon;
    global_valid_   = true;
  }

  void winch_state_callback(const std_msgs::msg::String::SharedPtr msg)
  {
    winch_state_ = msg->data;
  }

  void gimbal_status_callback(
    const px4_msgs::msg::GimbalDeviceAttitudeStatus::SharedPtr msg)
  {
    // PX4 reports the gimbal's own attitude; pitch below horizontal is
    // negative, matching the /gimbal_tilt_cmd convention.
    gimbal_measured_rad_ = vision_landing::quat_pitch(msg->q.data());
    gimbal_status_time_  = now();
    gimbal_status_seen_  = true;

    // TRUST, BUT VERIFY.
    //
    // A gimbal that reports an attitude is not necessarily a gimbal that
    // reports OUR attitude. The gazebo-classic SITL gimbal publishes this
    // topic while ignoring the pitch command entirely: commanded -45 deg,
    // reported +5.7 deg, forever. Treating that as truth is worse than having
    // no feedback at all — it rejects every detection as "taken mid-slew" and
    // the aircraft searches until it gives up.
    //
    // So the feedback has to earn its authority: until the measured angle has
    // matched a commanded angle at least once, it is reported but not acted
    // on, and the mission behaves exactly as it did before this was added.
    // Verify only against a command that is meaningfully tilted. A level
    // command (0 rad) is matched by a gimbal that is simply stuck level —
    // which is exactly the SITL failure this guard exists for, so allowing
    // 0 deg to certify the feedback certifies precisely the broken case.
    if (!gimbal_feedback_trusted_ &&
        std::fabs(current_gimbal_) > gimbal_verify_min_rad_ &&
        std::fabs(gimbal_measured_rad_ - current_gimbal_) <= gimbal_settle_rad_) {
      gimbal_feedback_trusted_ = true;
      RCLCPP_INFO(get_logger(),
        "Gimbal feedback verified (measured %.0f deg matches commanded %.0f deg) "
        "— using the measured angle from here",
        gimbal_measured_rad_ * 180.0f / static_cast<float>(M_PI),
        current_gimbal_ * 180.0f / static_cast<float>(M_PI));
    }
  }

  // The tilt angle to project detections with: the measured one once the
  // feedback has proved it tracks, the commanded one otherwise.
  float effective_gimbal_rad() const
  {
    if (gimbal_feedback_trusted_ && gimbal_feedback_fresh()) return gimbal_measured_rad_;
    return current_gimbal_;
  }

  bool gimbal_feedback_fresh() const
  {
    return gimbal_status_seen_ &&
           (now() - gimbal_status_time_).seconds() < gimbal_status_stale_sec_;
  }

  // True once the gimbal has actually reached the commanded angle. Detections
  // taken mid-slew project through an angle the camera was not at, which puts
  // the landing target metres from the tag.
  //
  // Only ever gates anything once the feedback has been verified against a
  // command (see gimbal_status_callback). With no feedback, or feedback that
  // has never tracked, detections are accepted — stalling the mission on a
  // gimbal we cannot read is the worse failure.
  bool gimbal_settled() const
  {
    if (!gimbal_feedback_trusted_ || !gimbal_feedback_fresh()) return true;
    return std::fabs(gimbal_measured_rad_ - current_gimbal_) <= gimbal_settle_rad_;
  }

  // Consume a detection if one has arrived AND it can be trusted. Centralised
  // so SEARCH, GOTO_TAG and LAND cannot drift apart on the gate.
  bool take_detection(float & north, float & east)
  {
    if (!tag_detected_) return false;
    tag_detected_ = false;
    if (!gimbal_settled()) {
      RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000,
        "Discarding detection: gimbal at %.0f deg, commanded %.0f deg (still slewing)",
        gimbal_measured_rad_ * 180.0f / static_cast<float>(M_PI),
        current_gimbal_ * 180.0f / static_cast<float>(M_PI));
      return false;
    }
    camera_to_ned(std::fabs(effective_gimbal_rad()), north, east);
    return true;
  }

  void publish_state()
  {
    std_msgs::msg::String msg;
    char buf[512];
    std::snprintf(buf, sizeof(buf),
      "state=%s t=%.1f armed=%d nav_state=%u alt=%.2f pos_valid=%d "
      "tag_age=%.1f winch=%s battery=%.2f failsafe=%s",
      state_name(state_), time_in_state(), static_cast<int>(armed_), nav_state_,
      height_above_home(), static_cast<int>(pos_valid_),
      tag_ever_seen_ ? (now() - last_tag_time_).seconds() : -1.0,
      winch_state_.empty() ? "unknown" : winch_state_.c_str(),
      battery_remaining_,
      failsafe_reason_.empty() ? "none" : failsafe_reason_.c_str());
    msg.data = buf;
    state_pub_->publish(msg);
  }

  void costmap_callback(const nav_msgs::msg::OccupancyGrid::SharedPtr msg)
  {
    costmap_ = *msg;
    last_costmap_time_ = now();
  }

  bool costmap_fresh() const
  {
    return !costmap_.data.empty() &&
           (now() - last_costmap_time_).seconds() < costmap_stale_sec_;
  }

  // Cost at a point given in mission NED. Returns -1 for "outside the map or
  // unknown", which is NOT treated as an obstacle: the aircraft must not fly
  // into what it HAS detected, and unobserved space is not a detection.
  int cost_at(float north, float east) const
  {
    if (costmap_.data.empty()) return -1;
    // Costmap is ENU in "map": x = east, y = north.
    const double res = costmap_.info.resolution;
    if (res <= 0.0) return -1;
    const int cx = static_cast<int>(
      std::floor((east  - costmap_.info.origin.position.x) / res));
    const int cy = static_cast<int>(
      std::floor((north - costmap_.info.origin.position.y) / res));
    if (cx < 0 || cy < 0 ||
        cx >= static_cast<int>(costmap_.info.width) ||
        cy >= static_cast<int>(costmap_.info.height)) return -1;
    return costmap_.data[cy * costmap_.info.width + cx];
  }

  bool point_blocked(float north, float east) const
  {
    const int c = cost_at(north, east);
    return c >= obstacle_cost_threshold_;
  }

  // Walk a straight segment at costmap resolution. Sampling coarser than the
  // cell size can step straight over a wall one cell thick.
  bool segment_blocked(float n0, float e0, float n1, float e1,
                       float * hit_n = nullptr, float * hit_e = nullptr) const
  {
    const double res = costmap_.info.resolution > 0.0 ? costmap_.info.resolution : 0.2;
    const float len = std::hypot(n1 - n0, e1 - e0);
    const int steps = std::max(1, static_cast<int>(std::ceil(len / (res * 0.5))));
    for (int i = 0; i <= steps; ++i) {
      const float f = static_cast<float>(i) / steps;
      const float n = n0 + (n1 - n0) * f;
      const float e = e0 + (e1 - e0) * f;
      if (point_blocked(n, e)) {
        if (hit_n) *hit_n = n;
        if (hit_e) *hit_e = e;
        return true;
      }
    }
    return false;
  }

  // Is the planned route still clear, from where we are to `horizon` metres
  // along it? Checking the whole path every tick is wasted work — the far end
  // will be re-planned several times before we reach it — but the part we are
  // about to fly must be clear right now.
  bool path_ahead_clear(float horizon, float * hit_n, float * hit_e) const
  {
    if (path_.poses.empty()) return true;
    size_t best = 0;
    double best_d = 1e18;
    for (size_t i = 0; i < path_.poses.size(); ++i) {
      const double pn = path_.poses[i].pose.position.y;
      const double pe = path_.poses[i].pose.position.x;
      const double d = std::hypot(pn - current_ned_x_, pe - current_ned_y_);
      if (d < best_d) { best_d = d; best = i; }
    }
    float travelled = 0.0f;
    for (size_t i = best; i + 1 < path_.poses.size() && travelled < horizon; ++i) {
      const float n0 = static_cast<float>(path_.poses[i].pose.position.y);
      const float e0 = static_cast<float>(path_.poses[i].pose.position.x);
      const float n1 = static_cast<float>(path_.poses[i + 1].pose.position.y);
      const float e1 = static_cast<float>(path_.poses[i + 1].pose.position.x);
      if (segment_blocked(n0, e0, n1, e1, hit_n, hit_e)) return false;
      travelled += std::hypot(n1 - n0, e1 - e0);
    }
    return true;
  }

  void path_callback(const nav_msgs::msg::Path::SharedPtr msg)
  {
    if (msg->poses.empty()) return;
    path_ = *msg;
    last_path_time_ = now();
  }

  void start_callback(const std_msgs::msg::Bool::SharedPtr msg)
  {
    if (msg->data) {
      start_requested_ = true;
    } else if (mission_active()) {
      // Operator abort: {data: false} mid-flight forces a failsafe land.
      RCLCPP_WARN(get_logger(), "Operator abort received — failsafe landing");
      enter_failsafe_land("operator_abort");
    }
  }

  // ── helpers ──────────────────────────────────────────────────────────────────

  bool mission_active() const
  {
    return state_ == State::TAKEOFF  || state_ == State::TRANSIT ||
           state_ == State::SEARCH   || state_ == State::GOTO_TAG ||
           state_ == State::DELIVER  || state_ == State::RETURN ||
           state_ == State::LAND     || state_ == State::SECURE_PAYLOAD;
  }

  // True while this run still has a package to deliver.
  bool delivering() const { return have_delivery_waypoint() && !delivered_; }

  void begin_search()
  {
    search_level_   = 0;
    search_yaw_     = current_heading_;
    total_rotation_ = 0.0f;
    search_ticks_   = 0;
    current_gimbal_ = LEVELS[0].gimbal_rad;
    // Search over wherever we are now — the delivery point after a transit,
    // or the launch point on a plain search-and-land. Holding home_ here
    // would fly the aircraft all the way back mid-delivery.
    search_x_ = current_ned_x_;
    search_y_ = current_ned_y_;
    transition(State::SEARCH);
  }

  double time_in_state() const { return (now() - state_entered_).seconds(); }

  void transition(State s)
  {
    if (s != state_) {
      RCLCPP_INFO(get_logger(), "STATE %s -> %s", state_name(state_), state_name(s));
    }
    state_ = s;
    state_entered_ = now();
    publish_state();   // don't make an operator wait up to a second for this
  }

  // Project camera-frame tag offset to world NED. The maths lives in
  // vision_landing/mission_math.hpp so it can be unit tested; see
  // test/test_mission_math.cpp.
  //
  // gimbal_abs: magnitude of tilt below horizontal (radians, positive).
  // Uses EKF position when valid, commanded setpoint position otherwise.
  void camera_to_ned(float gimbal_abs, float & north, float & east) const
  {
    const float base_x = pos_valid_ ? current_ned_x_ : cmd_ned_x_;
    const float base_y = pos_valid_ ? current_ned_y_ : cmd_ned_y_;
    const auto offset =
      vision_landing::camera_offset_body(tag_x_, tag_y_, tag_z_, gimbal_abs);
    vision_landing::body_offset_to_ned(base_x, base_y, offset, current_heading_,
                                       north, east);
  }

  // Horizontal distance to tag in body frame (no EKF position needed).
  float tag_horiz_dist(float gimbal_abs) const
  {
    return vision_landing::body_offset_range(
      vision_landing::camera_offset_body(tag_x_, tag_y_, tag_z_, gimbal_abs));
  }

  float level_alt_ned(int level) const { return home_z_ - LEVELS[level].height_m; }
  float height_above_home() const      { return home_z_ - current_ned_z_; }

  bool have_delivery_waypoint() const
  {
    return std::isfinite(delivery_lat_) && std::isfinite(delivery_lon_);
  }

  // Convert the delivery lat/lon into the local NED frame, anchored on the
  // vehicle's global+local correspondence captured at mission start. See
  // vision_landing/mission_math.hpp.
  void delivery_target_ned(float & north, float & east) const
  {
    vision_landing::gps_to_local_ned(home_lat_, home_lon_, home_x_, home_y_,
                                     delivery_lat_, delivery_lon_, north, east);
  }

  // Horizontal distance from the launch point, in metres.
  float range_from_home() const
  {
    const float dx = current_ned_x_ - home_x_;
    const float dy = current_ned_y_ - home_y_;
    return std::sqrt(dx * dx + dy * dy);
  }

  void set_winch(const std::string & cmd)
  {
    std_msgs::msg::String msg;
    msg.data = cmd;
    winch_pub_->publish(msg);
  }

  // Hand Nav2 a goal to plan toward. Latched, so nav2_path_bridge picks it up
  // even if it starts after us.
  void publish_transit_goal(float nx, float ny)
  {
    geometry_msgs::msg::PoseStamped g;
    g.header.stamp = now();
    // Nav2's global costmap is in "map"; drone_nav's TF chain ties map->odom
    // ->base_link, and the mission's local NED shares that origin.
    g.header.frame_id = "map";
    // NED (north, east) -> ENU (x=east, y=north), which is what ROS/Nav2 use.
    g.pose.position.x = ny;
    g.pose.position.y = nx;
    g.pose.position.z = 0.0;
    g.pose.orientation.w = 1.0;
    transit_goal_pub_->publish(g);
  }

  bool path_fresh() const
  {
    return use_nav2_ && !path_.poses.empty() &&
           (now() - last_path_time_).seconds() < path_stale_sec_;
  }

  // Pick a carrot on the planned path: the first pose at least
  // path_lookahead_m_ ahead of the vehicle, so the aircraft cuts corners
  // smoothly instead of chasing every waypoint.
  // Returns NED coordinates.
  bool path_carrot(float & nx, float & ny) const
  {
    if (path_.poses.empty()) return false;
    // Nav2 poses are ENU; convert back to NED (north = y_enu, east = x_enu).
    size_t best = 0;
    double best_d = 1e18;
    for (size_t i = 0; i < path_.poses.size(); ++i) {
      const double pn = path_.poses[i].pose.position.y;
      const double pe = path_.poses[i].pose.position.x;
      const double d = std::hypot(pn - current_ned_x_, pe - current_ned_y_);
      if (d < best_d) { best_d = d; best = i; }
    }
    for (size_t i = best; i < path_.poses.size(); ++i) {
      const double pn = path_.poses[i].pose.position.y;
      const double pe = path_.poses[i].pose.position.x;
      if (std::hypot(pn - current_ned_x_, pe - current_ned_y_) >= path_lookahead_m_) {
        nx = static_cast<float>(pn);
        ny = static_cast<float>(pe);
        return true;
      }
    }
    // Near the end of the path — aim at its last pose.
    nx = static_cast<float>(path_.poses.back().pose.position.y);
    ny = static_cast<float>(path_.poses.back().pose.position.x);
    return true;
  }

  // One guided leg, used by BOTH the outbound transit and the return.
  //
  // The return used to be a plain straight line home while the outbound leg
  // routed around obstacles — so exactly half the flight was unguarded, over
  // the same ground, in the same world. Both legs now go through here.
  //
  // Returns true if the aircraft was commanded to move. False means it is
  // holding position on purpose, and the caller must not command anything
  // else: the reason it is holding is that moving is not known to be safe.
  bool fly_guided_leg(float tgt_n, float tgt_e, float tgt_z, const char * leg,
                      const float * yaw_override = nullptr)
  {
    const float dx = tgt_n - current_ned_x_;
    const float dy = tgt_e - current_ned_y_;
    const float dist = std::max(std::hypot(dx, dy), 1e-3f);

    // Hold in place, and if the hold does not clear, climb out of it.
    const auto hold = [&](const char * why) {
      if (blocked_since_.nanoseconds() == 0) blocked_since_ = now();
      const double held = (now() - blocked_since_).seconds();

      float hold_z = tgt_z;
      if (held > blocked_escape_sec_) {
        // Climb, capped by the companion-side altitude fence. Height is the
        // one direction that is reliably clearer than where we are: the
        // obstacle is on the ground and we are above it.
        const float want = height_above_home() + escape_climb_m_;
        const float capped = std::min(want, max_altitude_m_ - 1.0f);
        hold_z = home_z_ - capped;
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 3000,
          "%s: blocked %.0f s — climbing to %.1f m to get above it (%s)",
          leg, held, capped, why);
      } else {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
          "%s: HOLDING — %s", leg, why);
      }
      publish_setpoint(current_ned_x_, current_ned_y_, hold_z, current_heading_);
    };

    // A global planner cannot usefully route a two-metre nudge, and demanding
    // one at the moment of landing would add a failure mode exactly where the
    // mission can least afford it. Below min_route_m the aircraft moves
    // directly — but the move is still CHECKED against the costmap, so the
    // "never fly into a detected obstacle" guarantee holds over the whole
    // mission, not only over the legs long enough to plan.
    const bool route_worthwhile = dist > min_route_m_;

    if (use_nav2_ && route_worthwhile) {
      // 1. Keep the planner aimed, clamped into its rolling window.
      const float moved = std::hypot(current_ned_x_ - last_goal_pub_x_,
                                     current_ned_y_ - last_goal_pub_y_);
      if (!transit_goal_sent_ || moved > nav2_goal_refresh_m_) {
        float gx = tgt_n, gy = tgt_e;
        if (dist > nav2_goal_max_range_m_) {
          gx = current_ned_x_ + (dx / dist) * nav2_goal_max_range_m_;
          gy = current_ned_y_ + (dy / dist) * nav2_goal_max_range_m_;
        }
        publish_transit_goal(gx, gy);
        transit_goal_sent_ = true;
        last_goal_pub_x_ = current_ned_x_;
        last_goal_pub_y_ = current_ned_y_;
      }

      // 2. Is the DESTINATION itself reachable?
      //
      //    A goal inside a building is not a route-planning problem, it is an
      //    input error, and it does not fail cleanly: Nav2 cannot plan to a
      //    lethal cell, so it returns nothing or a partial path, the safety
      //    check refuses it, the aircraft backs off, replans, approaches, and
      //    repeats until the leg times out. Measured: a delivery point set
      //    inside the Morgan J. Burke Aquatic Center produced exactly that,
      //    oscillating between 8 and 58 m from the goal for the whole leg.
      //
      //    Say so plainly instead. It is unfixable in the air, so there is no
      //    point burning the deadline discovering that repeatedly.
      if (costmap_fresh() && point_blocked(tgt_n, tgt_e)) {
        if (target_blocked_since_.nanoseconds() == 0) target_blocked_since_ = now();
        const double held = (now() - target_blocked_since_).seconds();
        if (held > target_blocked_abort_sec_) {
          RCLCPP_ERROR(get_logger(),
            "%s: the destination NED (%.1f, %.1f) is itself inside an obstacle. "
            "No route can exist to it. Check delivery_lat/delivery_lon — a "
            "delivery point must be in open ground, not on a building.",
            leg, tgt_n, tgt_e);
          enter_failsafe_land("destination_unreachable");
          return false;
        }
        char why[200];
        std::snprintf(why, sizeof(why),
          "destination itself is inside an obstacle (%.0f/%.0f s before abort)",
          held, target_blocked_abort_sec_);
        hold(why);
        return false;
      }
      target_blocked_since_ = rclcpp::Time(0, 0, RCL_ROS_TIME);

      // 3. No costmap means nothing to check a route against, so there is no
      //    obstacle guarantee to be had. Hold rather than guess.
      if (require_costmap_to_fly_ && !costmap_fresh()) {
        hold("no fresh costmap — cannot confirm the route is clear");
        return false;
      }

      // 4. A plan must exist. Never having had one means the planner, the
      //    costmap or the lidar is absent, and a straight line would be flown
      //    blind for the whole leg.
      float aim_n = tgt_n, aim_e = tgt_e;
      const bool planned = path_fresh() && path_carrot(aim_n, aim_e);
      if (planned) ever_planned_ = true;
      if (!planned) {
        if (require_plan_to_transit_) {
          hold("waiting for a Nav2 route");
          return false;
        }
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
          "%s: no fresh Nav2 plan — flying straight line (obstacles NOT avoided)",
          leg);
      }

      // 5. The plan was clear when it was made. Re-check it against the
      //    costmap as it is NOW, out to the lookahead. This is what makes
      //    "never fly into a detected obstacle" true rather than hoped for.
      float hn = 0.0f, he = 0.0f;
      if (planned && !path_ahead_clear(lookahead_check_m_, &hn, &he)) {
        transit_goal_sent_ = false;      // force an immediate replan
        char why[160];
        std::snprintf(why, sizeof(why),
          "planned route is blocked at NED (%.1f, %.1f) — replanning", hn, he);
        hold(why);
        return false;
      }

      // 6. And the specific step about to be commanded.
      float sn = current_ned_x_ + (aim_n - current_ned_x_);
      float se = current_ned_y_ + (aim_e - current_ned_y_);
      if (segment_blocked(current_ned_x_, current_ned_y_, sn, se, &hn, &he)) {
        transit_goal_sent_ = false;
        char why[160];
        std::snprintf(why, sizeof(why),
          "next step crosses an obstacle at NED (%.1f, %.1f)", hn, he);
        hold(why);
        return false;
      }

      const float adx = aim_n - current_ned_x_;
      const float ady = aim_e - current_ned_y_;
      const float adist = std::max(std::hypot(adx, ady), 1e-3f);
      // Progress watchdog. Detouring means the distance to the goal legitimately
      // grows for a while, so distance alone cannot judge progress — but the
      // BEST distance achieved so far should keep improving. If it has not for
      // no_progress_sec, the aircraft is circling something it cannot get past
      // and will otherwise do so until the deadline.
      if (dist < best_dist_ - 2.0f) { best_dist_ = dist; best_dist_at_ = now(); }
      else if (best_dist_at_.nanoseconds() != 0 &&
               (now() - best_dist_at_).seconds() > no_progress_sec_) {
        RCLCPP_ERROR(get_logger(),
          "%s: no progress for %.0f s — closest approach still %.0f m. The "
          "route around this obstacle is not being found; giving up rather "
          "than circling until the deadline.", leg, no_progress_sec_, best_dist_);
        enter_failsafe_land("no_progress");
        return false;
      }

      const float carrot = std::min(transit_speed_mps_ * TICK_SEC * 20.0f, adist);
      publish_setpoint(current_ned_x_ + (adx / adist) * carrot,
                       current_ned_y_ + (ady / adist) * carrot,
                       tgt_z,
                       yaw_override ? *yaw_override : std::atan2(ady, adx));
      blocked_since_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
      return true;
    }

    // Short leg, or use_nav2 deliberately disabled.
    const float carrot = std::min(transit_speed_mps_ * TICK_SEC * 20.0f, dist);
    const float sn = current_ned_x_ + (dx / dist) * carrot;
    const float se = current_ned_y_ + (dy / dist) * carrot;

    if (use_nav2_ && costmap_fresh()) {
      float hn = 0.0f, he = 0.0f;
      if (segment_blocked(current_ned_x_, current_ned_y_, sn, se, &hn, &he)) {
        char why[160];
        std::snprintf(why, sizeof(why),
          "short move crosses an obstacle at NED (%.1f, %.1f)", hn, he);
        hold(why);
        return false;
      }
    }

    publish_setpoint(sn, se, tgt_z,
                     yaw_override ? *yaw_override : std::atan2(dy, dx));
    blocked_since_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
    return true;
  }

  void publish_offboard_mode(bool velocity = false)
  {
    px4_msgs::msg::OffboardControlMode msg;
    msg.position  = true;
    msg.velocity  = velocity;
    msg.timestamp = get_clock()->now().nanoseconds() / 1000;
    offboard_pub_->publish(msg);
  }

  // Touchdown setpoint: hold x/y, but command a DOWNWARD VELOCITY with the
  // z position left NaN.
  //
  // A position setpoint pushed below ground keeps PX4 commanding thrust into
  // the ground, so its land detector — which waits for thrust to fall off —
  // never trips, and every landing fell through to the AUTO.LAND failsafe.
  // Descending on velocity lets the thrust drop naturally on contact, which
  // is what the detector is looking for.
  void publish_land_setpoint(float x, float y, float vz, float yaw)
  {
    cmd_ned_x_ = x;
    cmd_ned_y_ = y;
    px4_msgs::msg::TrajectorySetpoint msg;
    msg.position[0] = x;
    msg.position[1] = y;
    msg.position[2] = NAN;
    msg.velocity[0] = NAN;
    msg.velocity[1] = NAN;
    msg.velocity[2] = vz;      // NED: positive is down
    msg.yaw         = yaw;
    msg.timestamp   = get_clock()->now().nanoseconds() / 1000;
    setpoint_pub_->publish(msg);
  }

  void publish_setpoint(float x, float y, float z, float yaw)
  {
    cmd_ned_x_ = x;
    cmd_ned_y_ = y;
    px4_msgs::msg::TrajectorySetpoint msg;
    msg.position[0] = x;
    msg.position[1] = y;
    msg.position[2] = z;
    msg.yaw         = yaw;
    msg.timestamp   = get_clock()->now().nanoseconds() / 1000;
    setpoint_pub_->publish(msg);
  }

  void set_gimbal(float rad)
  {
    current_gimbal_ = rad;
    std_msgs::msg::Float64 msg;
    msg.data = rad;
    gimbal_pub_->publish(msg);
  }

  void send_command(uint16_t command, float param1 = 0.0f, float param2 = 0.0f)
  {
    px4_msgs::msg::VehicleCommand cmd;
    cmd.command       = command;
    cmd.param1        = param1;
    cmd.param2        = param2;
    cmd.target_system = 1;  cmd.target_component = 1;
    cmd.source_system = 1;  cmd.source_component = 1;
    cmd.from_external = true;
    cmd.timestamp     = get_clock()->now().nanoseconds() / 1000;
    command_pub_->publish(cmd);
  }

  void arm()    { send_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM, 1.0f); }
  void disarm() { send_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM, 0.0f); }
  void set_offboard_mode() { send_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_SET_MODE, 1.0f, 6.0f); }

  // True while the winch is anywhere other than fully stowed — i.e. while
  // there is cable, or a package, hanging under the aircraft.
  bool payload_deployed() const
  {
    return !winch_state_.empty() && winch_state_ != "stowed";
  }

  // Land the aircraft, dealing with the winch first.
  //
  // Landing with the winch part-way out puts a cable and a package under the
  // rotor disc, which is worse than any outcome the failsafe was trying to
  // avoid. Two paths:
  //
  //   urgent   (EKF invalid, battery critical, disarm imminent) — there is no
  //            time and possibly no position estimate to hold. Open the hook
  //            immediately: a dropped parcel from hover beats a cable in the
  //            props. Then descend.
  //   ordinary (operator abort, low battery, a stage timeout) — hold station
  //            in SECURE_PAYLOAD and reel the cable in first, with
  //            winch_secure_timeout_sec as the backstop before it drops the
  //            load and lands anyway.
  void enter_failsafe_land(const std::string & reason, bool urgent = false)
  {
    failsafe_reason_ = reason;

    if (payload_deployed() && !urgent && state_ != State::SECURE_PAYLOAD) {
      set_winch("retract");
      secure_ticks_ = 0;
      // Freeze the hold position now: current_ned_* keeps moving, and a
      // setpoint that chases it would let the aircraft drift on the wind for
      // as long as the retract takes.
      secure_x_ = current_ned_x_;
      secure_y_ = current_ned_y_;
      secure_z_ = current_ned_z_;
      transition(State::SECURE_PAYLOAD);
      RCLCPP_WARN(get_logger(),
        "%s — winch reports '%s'; retracting before landing (max %.0f s)",
        reason.c_str(), winch_state_.c_str(), winch_secure_timeout_sec_);
      return;
    }

    if (payload_deployed()) {
      RCLCPP_ERROR(get_logger(),
        "%s — no time to stow (winch '%s'): releasing payload before descent",
        reason.c_str(), winch_state_.c_str());
      set_winch("release");
    }

    send_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_NAV_LAND);
    nav_land_sent_ = 1;
    transition(State::FAILSAFE_LAND);
    RCLCPP_WARN(get_logger(), "FAILSAFE_LAND (%s) — handing vehicle to PX4 AUTO.LAND",
                reason.c_str());
  }

  // Finish the landing under PX4's own AUTO.LAND rather than under mission
  // control. This is a NORMAL completion path, not a failsafe: PX4's land
  // detector works perfectly well once we stop streaming setpoints at it, and
  // handing the last metre to it is strictly safer than guessing.
  void finish_under_auto_land(const char * why)
  {
    send_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_NAV_LAND);
    nav_land_sent_ = 1;
    transition(State::FAILSAFE_LAND);
    RCLCPP_INFO(get_logger(),
      "Handing the last metre to PX4 AUTO.LAND (%s). This is the normal "
      "finish when no range sensor is fitted.", why);
  }

  // ── safety checks run every tick while the mission is active ────────────────
  // Returns false when the mission must stop commanding.
  bool safety_ok()
  {
    // Pilot override / external mode change: once we've been in OFFBOARD, any
    // other nav state means someone (RC pilot or a PX4 failsafe) took over.
    // Never fight them — go passive permanently.
    if (offboard_seen_ &&
        nav_state_ != px4_msgs::msg::VehicleStatus::NAVIGATION_STATE_OFFBOARD) {
      RCLCPP_WARN(get_logger(),
        "Mode changed externally (nav_state=%u) — PILOT_OVERRIDE, going passive", nav_state_);
      transition(State::PILOT_OVERRIDE);
      return false;
    }

    // Disarmed mid-mission (kill switch, PX4 failsafe): nothing left to command.
    if (!armed_) {
      RCLCPP_WARN(get_logger(), "Vehicle disarmed externally — going passive");
      transition(State::PILOT_OVERRIDE);
      return false;
    }

    // Stale or invalid EKF position: we cannot navigate — let PX4 land.
    // Urgent: without a position estimate we cannot hold station to reel the
    // winch in, so the payload is released rather than carried down.
    if (require_valid_position_) {
      bool stale = (now() - last_pos_time_).seconds() > 1.0;
      if (stale || !pos_valid_) {
        RCLCPP_ERROR(get_logger(), "EKF position %s — failsafe landing",
                     stale ? "stale" : "invalid");
        enter_failsafe_land(stale ? "ekf_position_stale" : "ekf_position_invalid",
                            /*urgent=*/true);
        return false;
      }
    }

    // Battery: critical warning or below reserve → land now. A CRITICAL
    // warning is urgent (PX4 is about to act on its own); merely crossing our
    // own reserve threshold leaves time to stow the winch first.
    if (battery_received_) {
      bool critical = battery_warning_ >= px4_msgs::msg::BatteryStatus::WARNING_CRITICAL;
      bool low      = battery_remaining_ >= 0.0f && battery_remaining_ < battery_min_remaining_;
      if (critical || low) {
        RCLCPP_ERROR(get_logger(),
          "Battery failsafe (warning=%u remaining=%.0f%%) — landing",
          battery_warning_, battery_remaining_ * 100.0f);
        enter_failsafe_land(critical ? "battery_critical" : "battery_low", critical);
        return false;
      }
    }

    // Flight envelope. Checked every tick, in the air, where a preflight-only
    // range check cannot help: a runaway setpoint or a GPS jump would
    // otherwise fly the aircraft away with nothing on this side objecting.
    if (pos_valid_) {
      const float range = range_from_home();
      if (range > max_range_m_) {
        RCLCPP_ERROR(get_logger(),
          "Geofence: %.0f m from launch, limit %.0f m — landing", range, max_range_m_);
        enter_failsafe_land("geofence_range");
        return false;
      }
      const float alt = height_above_home();
      if (alt > max_altitude_m_) {
        RCLCPP_ERROR(get_logger(),
          "Geofence: %.1f m above launch, limit %.1f m — landing", alt, max_altitude_m_);
        enter_failsafe_land("geofence_altitude");
        return false;
      }
    }
    return true;
  }

  // Throttled: with require_start=false this runs every tick until it passes.
  bool preflight_ok()
  {
    if (!status_received_ || !pos_received_) {
      // Name the topics. The commonest cause of this message is a PX4 release
      // whose uxrce_dds_client publishes versioned topic names (…_v1 / …_v2)
      // while these parameters point at the plain ones, or the reverse — and
      // that is indistinguishable from a dead DDS link unless the wait says
      // what it is waiting on.
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
        "Preflight: no PX4 telemetry yet (status=%d on '%s', position=%d on '%s'). "
        "Compare with: ros2 topic list | grep fmu/out",
        (int)status_received_, status_topic_.c_str(),
        (int)pos_received_, local_pos_topic_.c_str());
      return false;
    }
    if (armed_) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
        "Preflight: vehicle already armed — refusing to start");
      return false;
    }
    if (require_valid_position_ && !pos_valid_) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
        "Preflight: EKF position not valid yet");
      return false;
    }
    // Don't take off on a pack that would trip the in-flight battery failsafe
    // on the first tick — refuse on the ground instead.
    if (battery_received_ && battery_remaining_ >= 0.0f &&
        battery_remaining_ < battery_min_remaining_) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
        "Preflight: battery at %.0f%%, below the %.0f%% minimum — refusing to start",
        battery_remaining_ * 100.0f, battery_min_remaining_ * 100.0f);
      return false;
    }

    // Delivery-specific gates. A bad waypoint is the one failure that would
    // fly the aircraft somewhere it should never go, so check it hard.
    if (have_delivery_waypoint()) {
      if (!global_valid_) {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
          "Preflight: delivery requested but no valid GPS fix yet");
        return false;
      }
      if (delivery_lat_ < -90.0 || delivery_lat_ > 90.0 ||
          delivery_lon_ < -180.0 || delivery_lon_ > 180.0) {
        RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 5000,
          "Preflight: delivery coordinate (%.6f, %.6f) is not a valid lat/lon",
          delivery_lat_, delivery_lon_);
        return false;
      }
      // home_lat_/lon_ are not set until start; use the live fix for the
      // check, through the same transform the in-flight fence uses so the two
      // cannot disagree about where the boundary is.
      const float range = vision_landing::gps_range_m(
        current_lat_, current_lon_, delivery_lat_, delivery_lon_);
      if (range > max_range_m_) {
        RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 5000,
          "Preflight: delivery point is %.0f m away, beyond max_range_m %.0f",
          range, max_range_m_);
        return false;
      }
      if (winch_state_.empty()) {
        // A delivery with no winch_bridge running would fly out, hover, and
        // sit at the address forever waiting for a state that never comes.
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
          "Preflight: delivery requested but no winch on /arc/winch/state — "
          "is winch_bridge running? (delivery.launch.py starts it; "
          "landing_pipeline.launch.py does not)");
        return false;
      }
      if (winch_state_ != "stowed") {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
          "Preflight: winch reports '%s' — stow it before a delivery",
          winch_state_.c_str());
        return false;
      }
    }

    // Envelope sanity. Catching this on the ground beats discovering in the
    // air that the fence is below the altitude the mission wants to fly.
    const float highest = std::max(takeoff_height_m_,
                                   have_delivery_waypoint() ? transit_height_m_ : 0.0f);
    if (highest > max_altitude_m_) {
      RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 5000,
        "Preflight: mission needs %.1f m but max_altitude_m is %.1f m — "
        "the geofence would trip on the way up", highest, max_altitude_m_);
      return false;
    }
    return true;
  }

  // ── main loop ────────────────────────────────────────────────────────────────
  void control_loop()
  {
    switch (state_) {

      // ── IDLE: fully passive until the operator starts the mission ─────────
      case State::IDLE: {
        if (!require_start_) start_requested_ = true;
        if (!start_requested_) break;
        start_requested_ = false;
        if (!preflight_ok()) break;

        // Everything from here on is relative to where we started.
        home_x_       = current_ned_x_;
        home_y_       = current_ned_y_;
        home_z_       = current_ned_z_;
        home_heading_ = current_heading_;
        home_lat_     = current_lat_;
        home_lon_     = current_lon_;
        warmup_ticks_ = 0;
        delivered_    = false;
        transition(State::WARMUP);
        RCLCPP_INFO(get_logger(),
          "Mission start — home NED (%.2f, %.2f, %.2f), heading %.0f°",
          home_x_, home_y_, home_z_, home_heading_ * 180.0f / static_cast<float>(M_PI));

        if (have_delivery_waypoint()) {
          delivery_target_ned(delivery_ned_x_, delivery_ned_y_);
          const float dx = delivery_ned_x_ - home_x_;
          const float dy = delivery_ned_y_ - home_y_;
          delivery_range_m_ = std::sqrt(dx * dx + dy * dy);
          transit_deadline_sec_ = std::max(
            transit_timeout_sec_,
            static_cast<double>(delivery_range_m_) /
              std::max(0.5f, transit_speed_mps_) * transit_timeout_margin_);
          RCLCPP_INFO(get_logger(),
            "Delivery to (%.7f, %.7f) — NED (%.1f, %.1f), %.0f m away "
            "(allowing %.0f s per leg)",
            delivery_lat_, delivery_lon_, delivery_ned_x_, delivery_ned_y_,
            delivery_range_m_, transit_deadline_sec_);
        } else {
          RCLCPP_INFO(get_logger(),
            "No delivery waypoint — search-and-land at the launch point");
        }
        break;
      }

      // ── WARMUP: stream setpoints, then request OFFBOARD + arm ─────────────
      case State::WARMUP: {
        publish_offboard_mode();
        set_gimbal(LEVELS[0].gimbal_rad);
        publish_setpoint(home_x_, home_y_, home_z_ - takeoff_height_m_, home_heading_);
        warmup_ticks_++;

        // 2 s of setpoints first (PX4 rejects OFFBOARD without a stream),
        // then re-request mode + arm once a second until both stick.
        if (warmup_ticks_ >= 40 && warmup_ticks_ % 20 == 0) {
          bool in_offboard =
            nav_state_ == px4_msgs::msg::VehicleStatus::NAVIGATION_STATE_OFFBOARD;
          if (!in_offboard) set_offboard_mode();
          if (!armed_)      arm();
          if (in_offboard && armed_) {
            takeoff_ticks_ = 0;
            transition(State::TAKEOFF);
            RCLCPP_INFO(get_logger(), "Armed in OFFBOARD — TAKEOFF to %.1f m", takeoff_height_m_);
            break;
          }
        }
        if (warmup_ticks_ >= 300) {   // 15 s
          RCLCPP_ERROR(get_logger(),
            "Could not arm in OFFBOARD (armed=%d nav_state=%u) — back to IDLE",
            (int)armed_, nav_state_);
          disarm();
          offboard_seen_ = false;
          transition(State::IDLE);
        }
        break;
      }

      // ── TAKEOFF: climb to takeoff height above home ───────────────────────
      case State::TAKEOFF: {
        if (!safety_ok()) break;
        publish_offboard_mode();
        // Pre-aim the gimbal at the first search level while climbing.
        set_gimbal(LEVELS[0].gimbal_rad);
        // Deliveries climb to transit altitude before setting off.
        const float climb_to = delivering() ? transit_height_m_ : takeoff_height_m_;
        publish_setpoint(home_x_, home_y_, home_z_ - climb_to, home_heading_);
        takeoff_ticks_++;

        if (height_above_home() >= climb_to - 0.3f) {
          if (delivering()) {
            transit_ticks_ = 0;
            transit_goal_sent_ = false;
            best_dist_ = 1e9f;
            best_dist_at_ = now();
            target_blocked_since_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
            transition(State::TRANSIT);
            RCLCPP_INFO(get_logger(),
              "TAKEOFF done (%.2f m) — TRANSIT to delivery point, %.0f m",
              height_above_home(), delivery_range_m_);
          } else {
            begin_search();
            RCLCPP_INFO(get_logger(),
              "TAKEOFF done (%.2f m) — SEARCH level 0: alt=%.1f m, gimbal=%.0f°",
              height_above_home(), LEVELS[0].height_m,
              LEVELS[0].gimbal_rad * 180.0f / static_cast<float>(M_PI));
          }
        } else if (takeoff_ticks_ * TICK_SEC > takeoff_timeout_sec_) {
          RCLCPP_ERROR(get_logger(),
            "TAKEOFF timeout at %.2f m — failsafe landing", height_above_home());
          enter_failsafe_land("takeoff_timeout");
        } else {
          RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 1000,
            "TAKEOFF: %.2f / %.2f m", height_above_home(), takeoff_height_m_);
        }
        break;
      }

      // ── TRANSIT: fly to the delivery waypoint at transit altitude ─────────
      // Setpoints are stepped along the path at transit_speed_mps rather than
      // commanding the far waypoint directly: PX4 would otherwise accelerate
      // to its own limits, and a stepped setpoint is also the natural place
      // for the Nav2 planner to substitute an obstacle-free path.
      case State::TRANSIT: {
        if (!safety_ok()) break;
        publish_offboard_mode();
        set_gimbal(LEVELS[0].gimbal_rad);
        transit_ticks_++;

        const float tgt_z = home_z_ - transit_height_m_;
        float dx = delivery_ned_x_ - current_ned_x_;
        float dy = delivery_ned_y_ - current_ned_y_;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < transit_accept_m_) {
          // Winch delivery: hover and lower. No landing, and no tag search —
          // the customer address has no marker; GPS positioning is what the
          // cable tolerance is for.
          settle_ticks_ = 0;
          winch_started_ = false;
          transit_goal_sent_ = false;
          transition(State::DELIVER);
          RCLCPP_INFO(get_logger(),
            "Arrived at delivery point (%.2f m) — descending to winch altitude",
            dist);
          break;
        }
        if (transit_ticks_ * TICK_SEC > transit_deadline_sec_) {
          RCLCPP_ERROR(get_logger(),
            "TRANSIT timeout after %.0f s, %.0f m short — failsafe landing",
            transit_deadline_sec_, dist);
          enter_failsafe_land("transit_timeout");
          break;
        }

        // Everything about steering this leg — keeping the planner aimed,
        // demanding a route, and re-checking that route against the live
        // costmap — lives in fly_guided_leg, so the outbound and return legs
        // cannot drift apart in how carefully they fly.
        const bool moving =
          fly_guided_leg(delivery_ned_x_, delivery_ned_y_, tgt_z, "TRANSIT");

        if (!moving) {
          // Held for a reason fly_guided_leg has already logged. The only
          // thing to add is giving up if it never clears.
          if (transit_ticks_ * TICK_SEC > plan_wait_timeout_sec_ && !ever_planned_) {
            RCLCPP_ERROR(get_logger(),
              "No Nav2 route after %.0f s — refusing to transit unguarded. "
              "Check the lidar is publishing and the costmap is filling; set "
              "require_plan_to_transit:=false to fly the straight line anyway.",
              plan_wait_timeout_sec_);
            enter_failsafe_land("no_obstacle_plan");
          }
          break;
        }

        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000,
          "TRANSIT: %.0f m to go, alt=%.1f m", dist, height_above_home());
        break;
      }

      // ── SEARCH: rotate at current level; step down if full circle misses ──
      case State::SEARCH: {
        if (!safety_ok()) break;
        publish_offboard_mode();

        float tgt_alt    = level_alt_ned(search_level_);
        float tgt_gimbal = LEVELS[search_level_].gimbal_rad;

        set_gimbal(tgt_gimbal);
        search_yaw_     += YAW_RATE;
        total_rotation_ += YAW_RATE;
        if (search_yaw_ > static_cast<float>(M_PI))
          search_yaw_ -= FULL_CIRCLE;

        publish_setpoint(search_x_, search_y_, tgt_alt, search_yaw_);
        search_ticks_++;

        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000,
          "SEARCH lvl%d: alt=%.2f m  gimbal=%.0f°  scanned=%.0f°",
          search_level_, height_above_home(),
          tgt_gimbal * 180.0f / static_cast<float>(M_PI),
          total_rotation_ * 180.0f / static_cast<float>(M_PI));

        if (take_detection(landing_ned_x_, landing_ned_y_)) {
          goto_ticks_   = 0;
          goto_yaw_     = current_heading_;
          transition(State::GOTO_TAG);
          RCLCPP_INFO(get_logger(),
            "Tag found! lvl%d  scan=%.0f°  → NED target (%.2f, %.2f)",
            search_level_, total_rotation_ * 180.0f / static_cast<float>(M_PI),
            landing_ned_x_, landing_ned_y_);
          break;
        }

        if (search_ticks_ * TICK_SEC > max_search_time_sec_) {
          RCLCPP_ERROR(get_logger(),
            "No tag after %.0f s of searching — failsafe landing", max_search_time_sec_);
          enter_failsafe_land("search_exhausted");
          break;
        }

        if (total_rotation_ >= FULL_CIRCLE) {
          total_rotation_ = 0.0f;
          if (search_level_ < N_LEVELS - 1) {
            search_level_++;
            RCLCPP_WARN(get_logger(),
              "Full scan — no tag.  Dropping to level %d (%.1f m, %.0f°)",
              search_level_, LEVELS[search_level_].height_m,
              LEVELS[search_level_].gimbal_rad * 180.0f / static_cast<float>(M_PI));
          } else {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
              "No tag at any level — repeating %.0f m scan", LEVELS[N_LEVELS - 1].height_m);
          }
        }
        break;
      }

      // ── GOTO_TAG: fly horizontally to tag at search altitude ──────────────
      case State::GOTO_TAG: {
        if (!safety_ok()) break;
        publish_offboard_mode();

        float tgt_alt    = level_alt_ned(search_level_);
        float tgt_gimbal = LEVELS[search_level_].gimbal_rad;
        set_gimbal(tgt_gimbal);

        // Refine target if we see the tag again
        take_detection(landing_ned_x_, landing_ned_y_);

        // Distance: EKF-based when valid; camera horizontal range otherwise
        // (sqrt(fwd²+right²) naturally converges to 0 as drone centres over tag).
        float dist;
        if (pos_valid_) {
          float dx = current_ned_x_ - landing_ned_x_;
          float dy = current_ned_y_ - landing_ned_y_;
          dist = std::sqrt(dx * dx + dy * dy);
        } else {
          dist = tag_horiz_dist(std::fabs(tgt_gimbal));
        }

        // Only steer yaw while far away — atan2 gets noisy right over the tag.
        if (dist > 1.0f) {
          goto_yaw_ = std::atan2(landing_ned_y_ - current_ned_y_,
                                 landing_ned_x_ - current_ned_x_);
        }
        // The run in to the pad is lateral flight and is guarded like the
        // rest of the mission. Beyond min_route_m Nav2 routes it; inside that
        // it is a direct move, still checked against the costmap. Holding
        // here is safe: the tag is not going anywhere, and goto_timeout_sec
        // already bounds how long this may take.
        if (!fly_guided_leg(landing_ned_x_, landing_ned_y_, tgt_alt,
                            "GOTO_TAG", &goto_yaw_)) {
          goto_ticks_++;
          break;
        }
        goto_ticks_++;

        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 500,
          "GOTO_TAG: dist=%.2f m  target=(%.2f, %.2f)", dist, landing_ned_x_, landing_ned_y_);

        if (dist < accept_radius_m_) {
          // GOTO_TAG only ever serves the home charging pad now. The delivery
          // drop is a winch lower from a GPS hover with no marker involved,
          // so a tag centred here always means "land to charge".
          land_z_ = tgt_alt;
          // Converging on the tag means the previous attempt succeeded, so the
          // retry budget starts fresh. Carrying the count forward meant a
          // mission that recovered twice hit the failsafe on its first real
          // problem, well before max_land_retries suggested.
          land_retries_ = 0;
          transition(State::LAND);
          RCLCPP_INFO(get_logger(), "Over tag — beginning descent");
        } else if (goto_ticks_ * TICK_SEC > goto_timeout_sec_) {
          land_retries_++;
          if (land_retries_ > max_land_retries_) {
            RCLCPP_ERROR(get_logger(), "GOTO_TAG timeout, retries exhausted — failsafe landing");
            enter_failsafe_land("goto_retries_exhausted");
          } else {
            RCLCPP_WARN(get_logger(),
              "GOTO_TAG timeout (dist=%.2f m) — back to SEARCH (retry %d/%d)",
              dist, land_retries_, max_land_retries_);
            total_rotation_ = 0.0f;
            transition(State::SEARCH);
          }
        }
        break;
      }

      // ── DELIVER: hold over the pad at release height and drop the payload ──
      case State::DELIVER: {
        if (!safety_ok()) break;
        publish_offboard_mode();
        // Gimbal level: nothing to look at, and a down-tilted camera is not
        // used for a winch drop.
        set_gimbal(0.0f);

        const float dx = delivery_ned_x_ - current_ned_x_;
        const float dy = delivery_ned_y_ - current_ned_y_;
        const float horiz = std::sqrt(dx * dx + dy * dy);
        const bool on_station = horiz < descend_radius_m_;

        // Hold TRANSIT altitude until the aircraft is genuinely over the GPS
        // point, and only then descend to winch height. Descending while still
        // closing the last stretch drops the aircraft on a diagonal, which
        // sheds obstacle clearance exactly where it is least wanted — over the
        // delivery address. It never goes to the ground here either: the
        // package comes down on the cable, rotors stay well clear.
        const float target_h = on_station ? winch_hover_height_m_
                                          : transit_height_m_;
        // Closing the last stretch to the address is lateral flight like any
        // other, so it goes through the same guarded path. Once on station
        // this is a pure hold and the guard is a no-op.
        if (!on_station) {
          if (!fly_guided_leg(delivery_ned_x_, delivery_ned_y_,
                              home_z_ - target_h, "DELIVER",
                              &current_heading_)) break;
        } else {
          publish_setpoint(delivery_ned_x_, delivery_ned_y_,
                           home_z_ - target_h, current_heading_);
        }

        const bool at_height =
          std::fabs(height_above_home() - winch_hover_height_m_) < 0.5f;

        if (!on_station) {
          RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 1000,
            "DELIVER: holding %.1f m, closing last %.1f m to the point",
            height_above_home(), horiz);
          settle_ticks_ = 0;
          break;
        }
        if (!at_height) {
          RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 1000,
            "DELIVER: over the point — descending %.1f -> %.1f m",
            height_above_home(), winch_hover_height_m_);
          settle_ticks_ = 0;
          break;
        }

        // Settle before paying out: a swinging cable started from a moving
        // airframe is how packages end up in trees.
        settle_ticks_++;
        if (settle_ticks_ * TICK_SEC < release_settle_sec_) break;

        if (!winch_started_) {
          winch_started_ = true;
          set_winch("lower");
          RCLCPP_WARN(get_logger(),
            "On station at %.1f m — lowering package", height_above_home());
          break;
        }

        // Drive the winch sequence from its own reported state, so each phase
        // is only started once the previous one actually finished.
        if (winch_state_ == "lowered") {
          set_winch("release");
        } else if (winch_state_ == "released") {
          set_winch("retract");
        } else if (winch_state_ == "stowed" && winch_started_) {
          delivered_ = true;
          transit_ticks_ = 0;
          // Force the planner to re-aim at home; otherwise it would keep
          // planning to the delivery point we are standing on.
          transit_goal_sent_ = false;
          ever_planned_ = false;
          best_dist_ = 1e9f;
          best_dist_at_ = now();
          target_blocked_since_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
          transition(State::RETURN);
          RCLCPP_INFO(get_logger(),
            "Package delivered and winch stowed — RETURN to launch");
        } else {
          RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000,
            "DELIVER: winch %s", winch_state_.c_str());
        }
        break;
      }

      // ── SECURE_PAYLOAD: hold station and stow the winch, then land ────────
      // Entered only from a non-urgent failsafe with cable out. The aircraft
      // holds exactly where it is — it is NOT trying to finish the mission,
      // only to get the package off the hook path before descending.
      case State::SECURE_PAYLOAD: {
        // Deliberately not safety_ok(): that would re-enter the failsafe and
        // loop. The two conditions that must still be able to interrupt are a
        // pilot taking over and the vehicle disarming.
        if (offboard_seen_ &&
            nav_state_ != px4_msgs::msg::VehicleStatus::NAVIGATION_STATE_OFFBOARD) {
          RCLCPP_WARN(get_logger(),
            "Mode changed externally during payload secure — PILOT_OVERRIDE");
          transition(State::PILOT_OVERRIDE);
          break;
        }
        if (!armed_) { transition(State::PILOT_OVERRIDE); break; }

        publish_offboard_mode();
        set_gimbal(0.0f);
        publish_setpoint(secure_x_, secure_y_, secure_z_, current_heading_);
        secure_ticks_++;

        if (!payload_deployed()) {
          RCLCPP_INFO(get_logger(), "Winch stowed — proceeding to failsafe landing");
          send_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_NAV_LAND);
          nav_land_sent_ = 1;
          transition(State::FAILSAFE_LAND);
          break;
        }
        if (secure_ticks_ * TICK_SEC > winch_secure_timeout_sec_) {
          RCLCPP_ERROR(get_logger(),
            "Winch still '%s' after %.0f s — releasing payload and landing",
            winch_state_.c_str(), winch_secure_timeout_sec_);
          set_winch("release");
          send_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_NAV_LAND);
          nav_land_sent_ = 1;
          transition(State::FAILSAFE_LAND);
          break;
        }
        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000,
          "SECURE_PAYLOAD: winch %s, %.0f s of %.0f",
          winch_state_.c_str(), secure_ticks_ * TICK_SEC, winch_secure_timeout_sec_);
        break;
      }

      // ── RETURN: climb back to transit altitude and fly home ───────────────
      case State::RETURN: {
        if (!safety_ok()) break;
        publish_offboard_mode();
        set_gimbal(LEVELS[0].gimbal_rad);
        transit_ticks_++;

        const float tgt_z = home_z_ - transit_height_m_;
        float dx = home_x_ - current_ned_x_;
        float dy = home_y_ - current_ned_y_;
        const float dist = std::sqrt(dx * dx + dy * dy);

        // Climb clear before translating, so the return leg does not clip
        // whatever is next to the delivery pad.
        if (height_above_home() < transit_height_m_ - 1.0f) {
          publish_setpoint(current_ned_x_, current_ned_y_, tgt_z, current_heading_);
          RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000,
            "RETURN: climbing %.1f/%.1f m", height_above_home(), transit_height_m_);
          break;
        }

        if (dist < transit_accept_m_) {
          begin_search();
          RCLCPP_INFO(get_logger(), "Home reached (%.2f m) — landing", dist);
          break;
        }
        if (transit_ticks_ * TICK_SEC > transit_deadline_sec_) {
          RCLCPP_ERROR(get_logger(),
            "RETURN timeout after %.0f s, %.0f m short — failsafe landing",
            transit_deadline_sec_, dist);
          enter_failsafe_land("return_timeout");
          break;
        }

        // The return is flown exactly as carefully as the outbound leg. It
        // used to be a bare straight line home — the same obstacles, the same
        // altitude, no route and no checking — which meant half of every
        // delivery was unguarded.
        if (!fly_guided_leg(home_x_, home_y_, tgt_z, "RETURN")) break;

        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000,
          "RETURN: %.0f m to go, alt=%.1f m", dist, height_above_home());
        break;
      }

      // ── LAND: descend onto AprilTag, tracking it on the way down ──────────
      case State::LAND: {
        if (!safety_ok()) break;
        publish_offboard_mode();
        set_gimbal(LEVELS[search_level_].gimbal_rad);

        take_detection(landing_ned_x_, landing_ned_y_);

        // Above the commit height, losing the tag aborts the descent — climb
        // back up and re-search.  Below it the tag leaves the camera FOV
        // naturally, so we commit to touchdown on the last known position.
        bool tag_fresh = (now() - last_tag_time_).seconds() < tag_lost_abort_sec_;
        if (!tag_fresh && height_above_home() > land_commit_height_m_) {
          land_retries_++;
          if (land_retries_ > max_land_retries_) {
            RCLCPP_ERROR(get_logger(), "Tag lost in LAND, retries exhausted — failsafe landing");
            enter_failsafe_land("land_tag_lost_retries_exhausted");
          } else {
            RCLCPP_WARN(get_logger(),
              "Tag lost during descent — climbing back to SEARCH (retry %d/%d)",
              land_retries_, max_land_retries_);
            total_rotation_ = 0.0f;
            search_yaw_     = current_heading_;
            transition(State::SEARCH);
          }
          break;
        }

        // Above the commit height, track the tag with position setpoints so
        // the descent stays centred. Below it, switch to a velocity descent
        // so PX4 can actually detect touchdown (see publish_land_setpoint).
        if (height_above_home() > land_commit_height_m_) {
          land_z_ += descent_rate_mps_ * TICK_SEC;
          publish_setpoint(landing_ned_x_, landing_ned_y_, land_z_, current_heading_);
        } else {
          publish_offboard_mode(true);
          publish_land_setpoint(landing_ned_x_, landing_ned_y_,
                                descent_rate_mps_, current_heading_);
        }
        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 500,
          "LAND: alt=%.2f m  landed=%d", height_above_home(), (int)landed_detected_);

        // Touchdown, in order of confidence.
        //
        // PX4's own `landed` flag is the authority, but it will not trip while
        // this node is still streaming an OFFBOARD setpoint: the position
        // controller keeps thrust up to hold x/y, and PX4's last land-detection
        // stage waits for thrust to fall away. Measured in SITL, the aircraft
        // sat on the pad at 0.03 m with landed=false for the full 90 s
        // timeout, and every landing completed under AUTO.LAND instead of
        // under mission control.
        //
        // So below the commit height we also accept PX4's EARLIER stages,
        // ground_contact / maybe_landed, held for contact_confirm_sec. Those
        // do trip, and combined with "below commit height and no longer
        // descending" they are a sound touchdown call. The 90 s failsafe stays
        // as the backstop.
        const bool contact_held =
          contact_since_.nanoseconds() != 0 &&
          (now() - contact_since_).seconds() > contact_confirm_sec_;

        // Touchdown by stalled descent.
        //
        // This is the signal that actually works. While this node streams an
        // OFFBOARD setpoint, PX4 suppresses EVERY stage of its land detector —
        // measured in SITL, the aircraft sat on the pad at 0.03 m with
        // landed, maybe_landed AND ground_contact all false for the full 90 s
        // timeout, and only reported ground contact once the mission stopped
        // commanding. So waiting for PX4 to tell us we have landed is waiting
        // for something that cannot happen until we stop asking.
        //
        // What we can see for ourselves: we are commanding a descent at
        // descent_rate_mps, we are close to the pad, and we are not going
        // down. That is the ground.
        const bool descending_commanded = height_above_home() <= land_commit_height_m_;
        const bool not_moving_down =
          descending_commanded &&
          height_above_home() < descent_stall_height_m_ &&
          std::fabs(current_vz_) < descent_stall_vz_;
        if (!not_moving_down) {
          descent_stalled_since_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
        } else if (descent_stalled_since_.nanoseconds() == 0) {
          descent_stalled_since_ = now();
        }
        const bool descent_stalled =
          descent_stalled_since_.nanoseconds() != 0 &&
          (now() - descent_stalled_since_).seconds() > descent_stall_sec_;

        // WHY A STALLED DESCENT NO LONGER DISARMS
        //
        // The height in "we are close to the ground" is height_above_home() —
        // an EKF estimate referenced to the takeoff point, which drifts with
        // barometric pressure over a long mission. An earlier version of this
        // code cut the motors on that estimate alone. If it drifted ~2 m low
        // and the aircraft held steady for 3 s during a descent, the motors
        // would stop while it was still 2 m up.
        //
        // The two outcomes are not symmetric:
        //
        //   disarm too early  -> the aircraft falls. Damage, possibly worse.
        //   hand off too early -> PX4 AUTO.LAND flies it down from wherever it
        //                         actually is. A controlled descent.
        //
        // So a stalled descent hands off; it never disarms. Disarming directly
        // is reserved for signals that cannot be wrong about being on the
        // ground: PX4's own land detector, or a real range sensor.
        const bool measured = dist_bottom_valid_;
        const float clearance = measured ? dist_bottom_ : height_above_home();

        if (landed_detected_) {
          // PX4 says it is down. Authoritative.
          disarm();
          transition(State::LANDED);
          RCLCPP_INFO(get_logger(), "Touchdown confirmed by PX4 — disarmed. Mission complete.");
        } else if (contact_held && clearance < land_commit_height_m_) {
          // PX4's earlier land-detection stages, held. Also PX4's own judgement.
          disarm();
          transition(State::LANDED);
          RCLCPP_INFO(get_logger(),
            "Touchdown on ground contact (%.2f m%s, held %.1f s) — disarmed. "
            "Mission complete.", clearance, measured ? ", measured" : "",
            (now() - contact_since_).seconds());
        } else if (descent_stalled && measured &&
                   dist_bottom_ < descent_stall_height_m_) {
          // Stalled descent AND a range sensor agreeing we are centimetres off
          // the ground. A measurement, not an estimate, so disarming is sound.
          disarm();
          transition(State::LANDED);
          RCLCPP_INFO(get_logger(),
            "Touchdown: descent stalled with the range sensor reading %.2f m — "
            "disarmed. Mission complete.", dist_bottom_);
        } else if (descent_stalled) {
          // Stalled, but the only height we have is an estimate that can drift.
          // Hand the last metre to PX4 rather than bet the airframe on it.
          finish_under_auto_land("descent stalled, no range sensor to confirm height");
        } else if (time_in_state() > land_handoff_sec_) {
          finish_under_auto_land("no touchdown detected in time");
        }
        break;
      }

      // ── FAILSAFE_LAND: PX4 AUTO.LAND owns the vehicle; we stay passive ────
      case State::FAILSAFE_LAND: {
        // Re-send NAV_LAND a few times in case the first was lost; never spam,
        // and never fight a pilot who takes over during the failsafe.
        bool in_auto_land =
          nav_state_ == px4_msgs::msg::VehicleStatus::NAVIGATION_STATE_AUTO_LAND;
        if (!in_auto_land && armed_ && nav_land_sent_ < 5 &&
            time_in_state() > nav_land_sent_ * 1.0) {
          send_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_NAV_LAND);
          nav_land_sent_++;
        }
        if (landed_detected_ || !armed_) {
          transition(State::LANDED);
          RCLCPP_INFO(get_logger(), "Failsafe landing complete");
        }
        break;
      }

      // ── PILOT_OVERRIDE: terminal, fully passive ───────────────────────────
      case State::PILOT_OVERRIDE: {
        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 10000,
          "PILOT_OVERRIDE — mission controller passive (restart node to fly again)");
        break;
      }

      // ── LANDED: terminal ──────────────────────────────────────────────────
      case State::LANDED: {
        if (armed_) disarm();
        break;
      }
    }
  }

  rclcpp::Time now() const { return get_clock()->now(); }

  // ── members ──────────────────────────────────────────────────────────────────
  State  state_;
  rclcpp::Time state_entered_{0, 0, RCL_ROS_TIME};

  // parameters
  bool   require_start_, require_valid_position_;
  float  takeoff_height_m_, accept_radius_m_, descent_rate_mps_;
  float  battery_min_remaining_, land_commit_height_m_;
  double takeoff_timeout_sec_, goto_timeout_sec_, max_search_time_sec_, tag_lost_abort_sec_;
  int    max_land_retries_;
  // delivery
  double delivery_lat_, delivery_lon_;
  float  transit_height_m_, transit_speed_mps_, transit_accept_m_;
  float  descend_radius_m_;
  double transit_timeout_sec_, release_settle_sec_;
  double transit_timeout_margin_, transit_deadline_sec_ = 300.0;
  float  winch_hover_height_m_, max_range_m_;
  float  max_altitude_m_;
  double winch_secure_timeout_sec_;
  bool   require_plan_to_transit_;
  double plan_wait_timeout_sec_;
  int    obstacle_cost_threshold_;
  double costmap_stale_sec_;
  bool   require_costmap_to_fly_;
  float  lookahead_check_m_;
  double blocked_escape_sec_;
  float  escape_climb_m_, min_route_m_;
  double target_blocked_abort_sec_, no_progress_sec_;
  rclcpp::Time target_blocked_since_{0, 0, RCL_ROS_TIME};
  float  best_dist_ = 1e9f;
  rclcpp::Time best_dist_at_{0, 0, RCL_ROS_TIME};
  rclcpp::Time blocked_since_{0, 0, RCL_ROS_TIME};
  nav_msgs::msg::OccupancyGrid costmap_;
  rclcpp::Time last_costmap_time_{0, 0, RCL_ROS_TIME};
  float  nav2_goal_max_range_m_, nav2_goal_refresh_m_;
  float  gimbal_settle_rad_, gimbal_verify_min_rad_;
  double gimbal_status_stale_sec_;
  std::string status_topic_, local_pos_topic_, land_detected_topic_, battery_topic_;
  // NOTE: there is deliberately no `return_home` parameter. It used to be
  // declared and never read, which is worse than not having it — an operator
  // could set return_home:=false and watch the aircraft fly home anyway. The
  // return leg is unconditional: this airframe lands only on its charging pad,
  // so "deliver and stay" is not a mode it has.
  bool   use_nav2_;
  double path_stale_sec_;
  float  path_lookahead_m_;

  // telemetry
  bool   armed_ = false, status_received_ = false, offboard_seen_ = false;
  uint8_t nav_state_ = 0;
  bool   pos_received_ = false, pos_valid_ = false;
  float  current_ned_x_ = 0, current_ned_y_ = 0, current_ned_z_ = 0, current_heading_ = 0;
  rclcpp::Time last_pos_time_{0, 0, RCL_ROS_TIME};
  bool   landed_detected_ = false;
  bool   ground_contact_ = false, maybe_landed_ = false;
  float  current_vz_ = 0.0f;
  rclcpp::Time descent_stalled_since_{0, 0, RCL_ROS_TIME};
  rclcpp::Time contact_since_{0, 0, RCL_ROS_TIME};
  double contact_confirm_sec_, descent_stall_sec_;
  float  descent_stall_vz_, descent_stall_height_m_;
  double land_handoff_sec_;
  float  dist_bottom_ = 0.0f;
  bool   dist_bottom_valid_ = false;
  bool   battery_received_ = false;
  uint8_t battery_warning_ = 0;
  float  battery_remaining_ = -1.0f;

  // tag
  bool   tag_detected_ = false;
  bool   tag_ever_seen_ = false;
  float  tag_x_ = 0, tag_y_ = 0, tag_z_ = 0;
  rclcpp::Time last_tag_time_{0, 0, RCL_ROS_TIME};

  // gimbal feedback
  bool   gimbal_status_seen_ = false;
  bool   gimbal_feedback_trusted_ = false;
  float  gimbal_measured_rad_ = 0.0f;
  rclcpp::Time gimbal_status_time_{0, 0, RCL_ROS_TIME};

  // mission
  bool   start_requested_ = false;
  float  home_x_ = 0, home_y_ = 0, home_z_ = 0, home_heading_ = 0;
  float  landing_ned_x_ = 0, landing_ned_y_ = 0;
  float  cmd_ned_x_ = 0, cmd_ned_y_ = 0;   // last commanded horizontal setpoint
  float  search_yaw_ = 0, total_rotation_ = 0, goto_yaw_ = 0;
  float  search_x_ = 0, search_y_ = 0;   // where SEARCH orbits (home or delivery)
  int    search_level_ = 0;
  float  current_gimbal_ = LEVELS[0].gimbal_rad;
  int    warmup_ticks_ = 0, takeoff_ticks_ = 0, search_ticks_ = 0, goto_ticks_ = 0;
  float  land_z_ = 0;
  int    land_retries_ = 0, nav_land_sent_ = 0;

  // delivery runtime
  double current_lat_ = 0.0, current_lon_ = 0.0;
  double home_lat_ = 0.0, home_lon_ = 0.0;
  bool   global_valid_ = false;
  float  delivery_ned_x_ = 0, delivery_ned_y_ = 0, delivery_range_m_ = 0;
  bool   delivered_ = false, winch_started_ = false;
  std::string winch_state_;
  int    transit_ticks_ = 0, settle_ticks_ = 0;
  bool   transit_goal_sent_ = false;
  nav_msgs::msg::Path path_;
  rclcpp::Time last_path_time_{0, 0, RCL_ROS_TIME};
  bool   ever_planned_ = false;
  float  last_goal_pub_x_ = 0, last_goal_pub_y_ = 0;

  // failsafe / payload securing
  std::string failsafe_reason_;
  int    secure_ticks_ = 0;
  float  secure_x_ = 0, secure_y_ = 0, secure_z_ = 0;

  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr      pose_sub_;
  rclcpp::Subscription<px4_msgs::msg::VehicleStatus>::SharedPtr         status_sub_;
  rclcpp::Subscription<px4_msgs::msg::VehicleLocalPosition>::SharedPtr  local_pos_sub_;
  rclcpp::Subscription<px4_msgs::msg::VehicleLandDetected>::SharedPtr   land_detect_sub_;
  rclcpp::Subscription<px4_msgs::msg::BatteryStatus>::SharedPtr         battery_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr                  start_sub_;
  rclcpp::Subscription<px4_msgs::msg::VehicleGlobalPosition>::SharedPtr global_pos_sub_;
  rclcpp::Subscription<px4_msgs::msg::GimbalDeviceAttitudeStatus>::SharedPtr gimbal_status_sub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr                   state_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr                   winch_pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr                winch_state_sub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr         transit_goal_pub_;
  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr                  transit_path_sub_;
  rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr         costmap_sub_;
  rclcpp::Publisher<px4_msgs::msg::TrajectorySetpoint>::SharedPtr       setpoint_pub_;
  rclcpp::Publisher<px4_msgs::msg::OffboardControlMode>::SharedPtr      offboard_pub_;
  rclcpp::Publisher<px4_msgs::msg::VehicleCommand>::SharedPtr           command_pub_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr                  gimbal_pub_;
  rclcpp::TimerBase::SharedPtr                                          timer_;
  rclcpp::TimerBase::SharedPtr                                          state_timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MissionController>());
  rclcpp::shutdown();
  return 0;
}
