// Gimbal bridge: /gimbal_tilt_cmd  ->  PX4 gimbal manager.
//
// The mission controller speaks one hardware-agnostic interface:
//   /gimbal_tilt_cmd (std_msgs/Float64) — tilt in radians, negative = down.
// This node translates that into PX4 VehicleCommand messages and lets PX4's
// gimbal module drive whatever is physically attached. That keeps SITL and
// the real aircraft on identical ROS code; only PX4 parameters differ:
//
//   SITL (gazebo-classic typhoon_h480 gimbal plugin, a MAVLink gimbal):
//     MNT_MODE_IN  = 4   (MAVLink gimbal protocol v2 input)
//     MNT_MODE_OUT = 1   (MAVLink gimbal protocol v1 to the sim gimbal)
//
//   Hardware (RCTimer 2-axis brushless controller / BruGi firmware, which
//   takes an RC PWM tilt command on its pitch input):
//     MNT_MODE_IN  = 4
//     MNT_MODE_OUT = 0   (AUX — PX4 emits PWM on an AUX output)
//     Wire that AUX output to the BruGi board's pitch RC input, and calibrate
//     the travel with MNT_RANGE_PITCH plus the output driver's
//     PWM_MAIN/AUX_MIN/_MAX/_DISARM values for the assigned channel.
//
// The BruGi board self-stabilises using its own IMU; PX4 only supplies the
// desired tilt, which is why an angle setpoint (not a rate) is sent.

#include <rclcpp/rclcpp.hpp>
#include <px4_msgs/msg/vehicle_command.hpp>
#include <std_msgs/msg/float64.hpp>
#include <cmath>

class GimbalBridge : public rclcpp::Node
{
public:
  GimbalBridge() : Node("gimbal_bridge")
  {
    min_tilt_rad_ = declare_parameter("min_tilt_rad", -1.5708);  // 90° down
    max_tilt_rad_ = declare_parameter("max_tilt_rad", 0.0);      // horizontal
    deadband_rad_ = declare_parameter("deadband_rad", 0.0087);   // ~0.5°
    refresh_sec_  = declare_parameter("refresh_sec", 1.0);
    gimbal_device_id_ = declare_parameter("gimbal_device_id", 0);  // 0 = all

    cmd_pub_ = create_publisher<px4_msgs::msg::VehicleCommand>(
      "/fmu/in/vehicle_command", 10);

    tilt_sub_ = create_subscription<std_msgs::msg::Float64>(
      "/gimbal_tilt_cmd", 10,
      std::bind(&GimbalBridge::tilt_callback, this, std::placeholders::_1));

    // Re-send periodically: PX4's gimbal manager treats setpoints as
    // perishable, and a gimbal powered up after us would otherwise sit at
    // its neutral angle until the mission next changed levels.
    timer_ = create_wall_timer(
      std::chrono::milliseconds(static_cast<int>(refresh_sec_ * 1000)),
      std::bind(&GimbalBridge::refresh, this));

    configure_mount();

    RCLCPP_INFO(get_logger(),
      "Gimbal bridge up — /gimbal_tilt_cmd -> PX4 gimbal manager "
      "(limits %.0f°..%.0f°)",
      min_tilt_rad_ * 180.0 / M_PI, max_tilt_rad_ * 180.0 / M_PI);
  }

private:
  void tilt_callback(const std_msgs::msg::Float64::SharedPtr msg)
  {
    double tilt = msg->data;
    if (!std::isfinite(tilt)) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
        "Ignoring non-finite tilt command");
      return;
    }
    // Clamp: a command outside the physical travel would peg the gimbal
    // against its stop and, on the real board, stall the pitch motor.
    double clamped = std::min(std::max(tilt, min_tilt_rad_), max_tilt_rad_);
    if (std::fabs(clamped - tilt) > 1e-6) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
        "Tilt %.1f° outside limits — clamped to %.1f°",
        tilt * 180.0 / M_PI, clamped * 180.0 / M_PI);
    }

    if (have_target_ && std::fabs(clamped - target_rad_) < deadband_rad_) return;
    target_rad_  = clamped;
    have_target_ = true;
    send_pitch(target_rad_);
  }

  void refresh()
  {
    if (have_target_) send_pitch(target_rad_);
  }

  // MAVLink gimbal pitch is in degrees, negative = pointing down, which
  // matches the /gimbal_tilt_cmd convention directly.
  void send_pitch(double tilt_rad)
  {
    px4_msgs::msg::VehicleCommand cmd;
    cmd.command = px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_GIMBAL_MANAGER_PITCHYAW;
    cmd.param1  = static_cast<float>(tilt_rad * 180.0 / M_PI);  // pitch, deg
    cmd.param2  = 0.0f;                                          // yaw, deg
    cmd.param3  = NAN;                                           // pitch rate
    cmd.param4  = NAN;                                           // yaw rate
    cmd.param5  = 0.0f;                                          // flags
    cmd.param7  = static_cast<float>(gimbal_device_id_);
    publish(cmd);
  }

  // Protocol v1 gimbals (the SITL plugin among them) need to be put into
  // MAVLink targeting mode before they accept angle setpoints.
  void configure_mount()
  {
    px4_msgs::msg::VehicleCommand cmd;
    cmd.command = px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_MOUNT_CONFIGURE;
    cmd.param1  = px4_msgs::msg::VehicleCommand::VEHICLE_MOUNT_MODE_MAVLINK_TARGETING;
    cmd.param2  = 0.0f;   // stabilize roll  (board does its own stabilisation)
    cmd.param3  = 0.0f;   // stabilize pitch
    cmd.param4  = 0.0f;   // stabilize yaw
    publish(cmd);
  }

  void publish(px4_msgs::msg::VehicleCommand & cmd)
  {
    cmd.target_system    = 1;  cmd.target_component = 1;
    cmd.source_system    = 1;  cmd.source_component = 1;
    cmd.from_external    = true;
    cmd.timestamp        = get_clock()->now().nanoseconds() / 1000;
    cmd_pub_->publish(cmd);
  }

  double min_tilt_rad_, max_tilt_rad_, deadband_rad_, refresh_sec_;
  int    gimbal_device_id_;
  double target_rad_{0.0};
  bool   have_target_{false};

  rclcpp::Publisher<px4_msgs::msg::VehicleCommand>::SharedPtr    cmd_pub_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr        tilt_sub_;
  rclcpp::TimerBase::SharedPtr                                   timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<GimbalBridge>());
  rclcpp::shutdown();
  return 0;
}
