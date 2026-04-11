#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <px4_msgs/msg/trajectory_setpoint.hpp>
#include <px4_msgs/msg/offboard_control_mode.hpp>
#include <px4_msgs/msg/vehicle_command.hpp>
#include <px4_msgs/msg/vehicle_status.hpp>
#include <px4_msgs/msg/vehicle_local_position.hpp>
#include <std_msgs/msg/float64.hpp>
#include <cmath>

// Search levels: (NED altitude, gimbal tilt in rad — negative = looking down)
// Drone descends through levels until the AprilTag is found.
struct SearchLevel { float alt_ned; float gimbal_rad; };
static const SearchLevel LEVELS[] = {
  { -5.0f, -1.396f },   // 5 m,  80° down
  { -3.0f, -1.222f },   // 3 m,  70° down
  { -2.0f, -1.047f },   // 2 m,  60° down
  { -1.0f, -0.785f },   // 1 m,  45° down  (minimum altitude)
};
static constexpr int    N_LEVELS    = 4;
static constexpr float  TAKEOFF_ALT = -5.0f;   // NED, 5 m above ground
static constexpr float  YAW_RATE    = 0.01f;   // rad per 50 ms tick
static constexpr float  FULL_CIRCLE = 2.0f * static_cast<float>(M_PI);

enum class State { TAKEOFF, SEARCH, GOTO_TAG, LAND, LANDED };

class MissionController : public rclcpp::Node
{
public:
  MissionController() : Node("mission_controller"), state_(State::TAKEOFF)
  {
    pose_sub_ = this->create_subscription<geometry_msgs::msg::PoseStamped>(
      "/landing_target_pose", 10,
      std::bind(&MissionController::pose_callback, this, std::placeholders::_1));

    status_sub_ = this->create_subscription<px4_msgs::msg::VehicleStatus>(
      "/fmu/out/vehicle_status", rclcpp::SensorDataQoS(),
      std::bind(&MissionController::status_callback, this, std::placeholders::_1));

    local_pos_sub_ = this->create_subscription<px4_msgs::msg::VehicleLocalPosition>(
      "/fmu/out/vehicle_local_position", rclcpp::SensorDataQoS(),
      std::bind(&MissionController::local_pos_callback, this, std::placeholders::_1));

    setpoint_pub_ = this->create_publisher<px4_msgs::msg::TrajectorySetpoint>(
      "/fmu/in/trajectory_setpoint", 10);
    offboard_pub_ = this->create_publisher<px4_msgs::msg::OffboardControlMode>(
      "/fmu/in/offboard_control_mode", 10);
    command_pub_  = this->create_publisher<px4_msgs::msg::VehicleCommand>(
      "/fmu/in/vehicle_command", 10);
    gimbal_pub_   = this->create_publisher<std_msgs::msg::Float64>(
      "/gimbal_tilt_cmd", 10);

    timer_ = this->create_wall_timer(
      std::chrono::milliseconds(50),
      std::bind(&MissionController::control_loop, this));

    tag_detected_    = false;
    tag_x_ = tag_y_ = tag_z_ = 0.0f;
    offboard_counter_ = 0;
    armed_           = false;
    pos_received_    = false;
    gps_valid_       = false;
    current_ned_x_   = current_ned_y_ = current_ned_z_ = 0.0f;
    current_heading_ = 0.0f;
    landing_ned_x_   = landing_ned_y_ = 0.0f;
    cmd_ned_x_       = cmd_ned_y_ = 0.0f;
    search_yaw_      = 0.0f;
    total_rotation_  = 0.0f;
    search_level_    = 0;
    current_gimbal_  = LEVELS[0].gimbal_rad;
    takeoff_ticks_   = 0;
    goto_ticks_      = 0;
    land_z_          = 0.0f;
    disarmed_        = false;

    RCLCPP_INFO(this->get_logger(), "Mission controller started — climbing to %.1f m", -TAKEOFF_ALT);
  }

private:
  // ── callbacks ────────────────────────────────────────────────────────────────
  void pose_callback(const geometry_msgs::msg::PoseStamped::SharedPtr msg)
  {
    tag_detected_ = true;
    tag_x_ = msg->pose.position.x;
    tag_y_ = msg->pose.position.y;
    tag_z_ = msg->pose.position.z;
  }

  void status_callback(const px4_msgs::msg::VehicleStatus::SharedPtr msg)
  {
    armed_ = (msg->arming_state == 2);
  }

  void local_pos_callback(const px4_msgs::msg::VehicleLocalPosition::SharedPtr msg)
  {
    current_ned_x_   = msg->x;
    current_ned_y_   = msg->y;
    current_ned_z_   = msg->z;
    current_heading_ = msg->heading;
    pos_received_    = true;
    // Mark GPS valid only once EKF has produced a non-trivial altitude estimate.
    // z stays 0 when barometer/EKF hasn't initialised — using that would break navigation.
    if (!gps_valid_ && std::fabs(msg->z) > 0.3f) {
      gps_valid_ = true;
      RCLCPP_INFO(this->get_logger(), "GPS valid — z=%.2f", msg->z);
    }
  }

  // ── helpers ──────────────────────────────────────────────────────────────────

  // Project camera-frame tag offset to world NED.
  // gimbal_abs: magnitude of tilt below horizontal (radians, positive).
  //
  // Camera frame (apriltag convention): x=right, y=down-in-image, z=depth.
  // When camera is pitched nose-down by θ (0=horizontal, 90=straight down):
  //   body_fwd   = tag_z * cos(θ) - tag_y * sin(θ)
  //   body_right = tag_x
  // Missing the -tag_y*sin(θ) term causes ~0.88 m overshoot at 80° tilt.
  //
  // Uses GPS position when available, commanded setpoint position otherwise.
  void camera_to_ned(float gimbal_abs, float & north, float & east) const
  {
    // Use GPS only when EKF has produced a valid (non-zero) altitude.
    // If GPS is stuck at (0,0,0) fall back to commanded position.
    float base_x = gps_valid_ ? current_ned_x_ : cmd_ned_x_;
    float base_y = gps_valid_ ? current_ned_y_ : cmd_ned_y_;
    float cos_t  = std::cos(gimbal_abs);
    float sin_t  = std::sin(gimbal_abs);
    float fwd    = tag_z_ * cos_t - tag_y_ * sin_t;
    float right  = tag_x_;
    float psi    = current_heading_;
    north = base_x + fwd * std::cos(psi) - right * std::sin(psi);
    east  = base_y + fwd * std::sin(psi) + right * std::cos(psi);
  }

  // Horizontal distance to tag in body frame (no GPS needed).
  float tag_horiz_dist(float gimbal_abs) const
  {
    float fwd   = tag_z_ * std::cos(gimbal_abs) - tag_y_ * std::sin(gimbal_abs);
    float right = tag_x_;
    return std::sqrt(fwd * fwd + right * right);
  }

  void publish_offboard_mode()
  {
    px4_msgs::msg::OffboardControlMode msg;
    msg.position  = true;
    msg.timestamp = this->get_clock()->now().nanoseconds() / 1000;
    offboard_pub_->publish(msg);
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
    msg.timestamp   = this->get_clock()->now().nanoseconds() / 1000;
    setpoint_pub_->publish(msg);
  }

  void set_gimbal(float rad)
  {
    current_gimbal_ = rad;
    std_msgs::msg::Float64 msg;
    msg.data = rad;
    gimbal_pub_->publish(msg);
  }

  void arm()
  {
    px4_msgs::msg::VehicleCommand cmd;
    cmd.command       = px4_msgs::msg::VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM;
    cmd.param1        = 1.0;
    cmd.target_system = 1;  cmd.target_component = 1;
    cmd.source_system = 1;  cmd.source_component = 1;
    cmd.from_external = true;
    cmd.timestamp     = this->get_clock()->now().nanoseconds() / 1000;
    command_pub_->publish(cmd);
  }

  void disarm()
  {
    px4_msgs::msg::VehicleCommand cmd;
    cmd.command       = px4_msgs::msg::VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM;
    cmd.param1        = 0.0;
    cmd.target_system = 1;  cmd.target_component = 1;
    cmd.source_system = 1;  cmd.source_component = 1;
    cmd.from_external = true;
    cmd.timestamp     = this->get_clock()->now().nanoseconds() / 1000;
    command_pub_->publish(cmd);
  }

  void set_offboard_mode()
  {
    px4_msgs::msg::VehicleCommand cmd;
    cmd.command       = px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_SET_MODE;
    cmd.param1        = 1.0;  cmd.param2 = 6.0;
    cmd.target_system = 1;  cmd.target_component = 1;
    cmd.source_system = 1;  cmd.source_component = 1;
    cmd.from_external = true;
    cmd.timestamp     = this->get_clock()->now().nanoseconds() / 1000;
    command_pub_->publish(cmd);
  }

  // ── main loop ────────────────────────────────────────────────────────────────
  void control_loop()
  {
    publish_offboard_mode();
    set_gimbal(current_gimbal_);

    // Warmup: stream setpoints for 5 s before arming (100 ticks × 50 ms)
    if (offboard_counter_ < 100) {
      publish_setpoint(0.0f, 0.0f, TAKEOFF_ALT, 0.0f);
      offboard_counter_++;
      if (offboard_counter_ == 50) { set_offboard_mode(); arm(); }
      return;
    }

    switch (state_) {

      // ── TAKEOFF: climb to 5 m ─────────────────────────────────────────────
      case State::TAKEOFF: {
        publish_setpoint(0.0f, 0.0f, TAKEOFF_ALT, 0.0f);
        takeoff_ticks_++;

        // GPS: z <= -4.7 means >=4.7 m above ground.  Fallback after 20 s.
        bool alt_ok  = pos_received_ && (current_ned_z_ <= TAKEOFF_ALT + 0.3f);
        bool timeout = (takeoff_ticks_ >= 400);

        if (alt_ok || timeout) {
          if (timeout && !alt_ok)
            RCLCPP_WARN(this->get_logger(),
              "TAKEOFF timeout — GPS z=%.2f, proceeding anyway", current_ned_z_);
          search_level_   = 0;
          search_yaw_     = current_heading_;
          total_rotation_ = 0.0f;
          current_gimbal_ = LEVELS[0].gimbal_rad;
          state_          = State::SEARCH;
          RCLCPP_INFO(this->get_logger(),
            "TAKEOFF done (%.2f m) — SEARCH level 0: alt=%.1f m, gimbal=%.0f°",
            -current_ned_z_, -LEVELS[0].alt_ned,
            LEVELS[0].gimbal_rad * 180.0f / static_cast<float>(M_PI));
        } else {
          RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
            "TAKEOFF: %.2f m / %.2f m  [armed=%d GPS=%d tick=%d]",
            -current_ned_z_, -TAKEOFF_ALT, (int)armed_, (int)pos_received_, takeoff_ticks_);
        }
        break;
      }

      // ── SEARCH: rotate at current level; step down if full circle misses ──
      case State::SEARCH: {
        float tgt_alt    = LEVELS[search_level_].alt_ned;
        float tgt_gimbal = LEVELS[search_level_].gimbal_rad;

        set_gimbal(tgt_gimbal);
        search_yaw_     += YAW_RATE;
        total_rotation_ += YAW_RATE;
        if (search_yaw_ > static_cast<float>(M_PI))
          search_yaw_ -= 2.0f * static_cast<float>(M_PI);

        publish_setpoint(0.0f, 0.0f, tgt_alt, search_yaw_);

        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
          "SEARCH lvl%d: alt=%.2f m  gimbal=%.0f°  scanned=%.0f°",
          search_level_, -current_ned_z_,
          tgt_gimbal * 180.0f / static_cast<float>(M_PI),
          total_rotation_ * 180.0f / static_cast<float>(M_PI));

        if (tag_detected_) {
          camera_to_ned(std::fabs(tgt_gimbal), landing_ned_x_, landing_ned_y_);
          tag_detected_ = false;
          goto_ticks_   = 0;
          state_        = State::GOTO_TAG;
          RCLCPP_INFO(this->get_logger(),
            "Tag found! lvl%d  scan=%.0f°  → NED target (%.2f, %.2f)",
            search_level_,
            total_rotation_ * 180.0f / static_cast<float>(M_PI),
            landing_ned_x_, landing_ned_y_);
          break;
        }

        if (total_rotation_ >= FULL_CIRCLE) {
          total_rotation_ = 0.0f;
          if (search_level_ < N_LEVELS - 1) {
            search_level_++;
            RCLCPP_WARN(this->get_logger(),
              "Full scan — no tag.  Dropping to level %d (%.1f m, %.0f°)",
              search_level_,
              -LEVELS[search_level_].alt_ned,
              LEVELS[search_level_].gimbal_rad * 180.0f / static_cast<float>(M_PI));
          } else {
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
              "No tag at any level — repeating 1 m scan");
          }
        }
        break;
      }

      // ── GOTO_TAG: fly horizontally to tag at search altitude ──────────────
      case State::GOTO_TAG: {
        float tgt_alt    = LEVELS[search_level_].alt_ned;
        float tgt_gimbal = LEVELS[search_level_].gimbal_rad;

        // Refine target if we see the tag again
        if (tag_detected_) {
          camera_to_ned(std::fabs(tgt_gimbal), landing_ned_x_, landing_ned_y_);
          tag_detected_ = false;
        }

        float yaw_to_tag = std::atan2(
          landing_ned_y_ - current_ned_y_,
          landing_ned_x_ - current_ned_x_);

        publish_setpoint(landing_ned_x_, landing_ned_y_, tgt_alt, yaw_to_tag);
        goto_ticks_++;

        // Compute dist: GPS-based when available; camera horizontal range otherwise.
        // Without GPS: dist = sqrt(fwd²+right²) naturally converges to 0 as drone centres over tag.
        float dist;
        if (gps_valid_) {
          float dx = current_ned_x_ - landing_ned_x_;
          float dy = current_ned_y_ - landing_ned_y_;
          dist = std::sqrt(dx * dx + dy * dy);
        } else {
          dist = tag_horiz_dist(std::fabs(tgt_gimbal));
        }

        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 500,
          "GOTO_TAG: dist=%.2f m  target=(%.2f, %.2f)  cmd=(%.2f, %.2f)",
          dist, landing_ned_x_, landing_ned_y_, cmd_ned_x_, cmd_ned_y_);

        // Transition when within 0.5 m OR 30 s timeout
        bool close   = (dist < 0.5f);
        bool timeout = (goto_ticks_ >= 600);

        if (close || timeout) {
          if (timeout && !close)
            RCLCPP_WARN(this->get_logger(),
              "GOTO_TAG timeout (GPS dist=%.2f m) — starting descent", dist);
          land_z_ = tgt_alt;
          state_  = State::LAND;
          RCLCPP_INFO(this->get_logger(), "Over tag — beginning descent");
        }
        break;
      }

      // ── LAND: descend onto AprilTag ───────────────────────────────────────
      case State::LAND: {
        if (tag_detected_) {
          camera_to_ned(std::fabs(LEVELS[search_level_].gimbal_rad),
            landing_ned_x_, landing_ned_y_);
          tag_detected_ = false;
        }

        float yaw_to_tag = std::atan2(
          landing_ned_y_ - current_ned_y_,
          landing_ned_x_ - current_ned_x_);

        land_z_ += 0.01f;   // 0.2 m/s descent
        if (land_z_ > 0.0f) land_z_ = 0.0f;

        publish_setpoint(landing_ned_x_, landing_ned_y_, land_z_, yaw_to_tag);
        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 500,
          "LAND: alt=%.3f m", -current_ned_z_);

        if (land_z_ >= -0.05f) {
          state_    = State::LANDED;
          disarmed_ = false;
          RCLCPP_INFO(this->get_logger(), "LANDED on AprilTag!");
        }
        break;
      }

      // ── LANDED ────────────────────────────────────────────────────────────
      case State::LANDED: {
        publish_setpoint(landing_ned_x_, landing_ned_y_, 0.0f, current_heading_);
        if (!disarmed_) {
          disarm();
          disarmed_ = true;
          RCLCPP_INFO(this->get_logger(), "Disarmed — propellers stopped");
        }
        break;
      }
    }
  }

  // ── members ──────────────────────────────────────────────────────────────────
  State  state_;
  bool   tag_detected_;
  float  tag_x_, tag_y_, tag_z_;
  int    offboard_counter_;
  bool   armed_;
  bool   pos_received_;
  bool   gps_valid_;       // true once EKF z moves off zero (barometer initialised)
  float  current_ned_x_, current_ned_y_, current_ned_z_;
  float  current_heading_;
  float  landing_ned_x_, landing_ned_y_;
  float  cmd_ned_x_, cmd_ned_y_;       // last commanded horizontal setpoint
  float  search_yaw_;
  float  total_rotation_;
  int    search_level_;
  float  current_gimbal_;
  int    takeoff_ticks_;
  int    goto_ticks_;
  float  land_z_;
  bool   disarmed_;

  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr      pose_sub_;
  rclcpp::Subscription<px4_msgs::msg::VehicleStatus>::SharedPtr         status_sub_;
  rclcpp::Subscription<px4_msgs::msg::VehicleLocalPosition>::SharedPtr  local_pos_sub_;
  rclcpp::Publisher<px4_msgs::msg::TrajectorySetpoint>::SharedPtr       setpoint_pub_;
  rclcpp::Publisher<px4_msgs::msg::OffboardControlMode>::SharedPtr      offboard_pub_;
  rclcpp::Publisher<px4_msgs::msg::VehicleCommand>::SharedPtr           command_pub_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr                  gimbal_pub_;
  rclcpp::TimerBase::SharedPtr                                          timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MissionController>());
  rclcpp::shutdown();
  return 0;
}
