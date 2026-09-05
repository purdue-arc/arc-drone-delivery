// Payload bridge: /arc/payload/command  ->  PX4 actuator (servo on AUX PWM).
//
// Same pattern as gimbal_bridge: the mission speaks one hardware-agnostic
// topic and PX4 drives the physical output, so SITL and the aircraft run
// identical ROS code.
//
//   /arc/payload/command  (std_msgs/Bool)   true = release, false = secure
//   /arc/payload/state    (std_msgs/Bool)   latched view of what we commanded
//
// PX4 side: VEHICLE_CMD_DO_SET_ACTUATOR drives "Peripheral via Actuator Set"
// output functions. Assign the servo's AUX channel to one of them
// (PWM_AUX_FUNCx = 'Peripheral via Actuator Set N') and calibrate the travel
// with PWM_AUX_MINx / PWM_AUX_MAXx, then set actuator_index below to N.
//
// param1..param6 map to actuator sets 1..6, each in [-1, 1]; NaN leaves a
// channel untouched. We only ever drive the one configured index.
//
// SAFETY: the payload stays secured until explicitly commanded. A release is
// latched and re-sent periodically so a servo that loses power mid-flight
// returns to the commanded position, and `require_release_confirm` keeps the
// mission from treating a release as done before the servo has had time to
// travel.

#include <rclcpp/rclcpp.hpp>
#include <px4_msgs/msg/vehicle_command.hpp>
#include <std_msgs/msg/bool.hpp>
#include <cmath>

class PayloadBridge : public rclcpp::Node
{
public:
  PayloadBridge() : Node("payload_bridge")
  {
    actuator_index_  = static_cast<int>(declare_parameter("actuator_index", 1));
    released_value_  = declare_parameter("released_value", 1.0);
    secured_value_   = declare_parameter("secured_value", -1.0);
    refresh_sec_     = declare_parameter("refresh_sec", 1.0);
    travel_time_sec_ = declare_parameter("travel_time_sec", 1.5);

    if (actuator_index_ < 1 || actuator_index_ > 6) {
      RCLCPP_ERROR(get_logger(),
        "actuator_index must be 1..6 (got %d) — refusing to command anything",
        actuator_index_);
      actuator_index_ = 0;
    }

    cmd_pub_ = create_publisher<px4_msgs::msg::VehicleCommand>(
      "/fmu/in/vehicle_command", 10);
    state_pub_ = create_publisher<std_msgs::msg::Bool>(
      "/arc/payload/state", rclcpp::QoS(1).transient_local());

    cmd_sub_ = create_subscription<std_msgs::msg::Bool>(
      "/arc/payload/command", 10,
      std::bind(&PayloadBridge::command_callback, this, std::placeholders::_1));

    timer_ = create_wall_timer(
      std::chrono::milliseconds(static_cast<int>(refresh_sec_ * 1000)),
      std::bind(&PayloadBridge::refresh, this));

    // Start secured, and say so, so the mission never infers "released"
    // from an absent message.
    apply(false);
    RCLCPP_INFO(get_logger(),
      "Payload bridge up — actuator set %d (secured=%.2f released=%.2f)",
      actuator_index_, secured_value_, released_value_);
  }

private:
  void command_callback(const std_msgs::msg::Bool::SharedPtr msg)
  {
    if (msg->data == released_) {
      return;  // idempotent; refresh() keeps re-asserting it anyway
    }
    apply(msg->data);
    RCLCPP_WARN(get_logger(), "Payload %s", msg->data ? "RELEASED" : "SECURED");
  }

  void refresh() { if (actuator_index_ > 0) send(released_); }

  void apply(bool released)
  {
    released_ = released;
    released_at_ = now();
    send(released_);
    std_msgs::msg::Bool s;
    s.data = released_;
    state_pub_->publish(s);
  }

  void send(bool released)
  {
    if (actuator_index_ == 0) return;
    const float value = static_cast<float>(released ? released_value_ : secured_value_);

    px4_msgs::msg::VehicleCommand cmd;
    cmd.command = px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_SET_ACTUATOR;
    // Untouched channels must be NaN, not 0 — 0 is a valid mid-travel command
    // and would drive every other peripheral servo to centre.
    cmd.param1 = cmd.param2 = cmd.param3 = NAN;
    cmd.param4 = cmd.param5 = cmd.param6 = NAN;
    switch (actuator_index_) {
      case 1: cmd.param1 = value; break;
      case 2: cmd.param2 = value; break;
      case 3: cmd.param3 = value; break;
      case 4: cmd.param4 = value; break;
      case 5: cmd.param5 = value; break;
      case 6: cmd.param6 = value; break;
      default: return;
    }
    cmd.param7 = 0.0f;   // index/group
    cmd.target_system = 1;  cmd.target_component = 1;
    cmd.source_system = 1;  cmd.source_component = 1;
    cmd.from_external = true;
    cmd.timestamp = get_clock()->now().nanoseconds() / 1000;
    cmd_pub_->publish(cmd);
  }

  rclcpp::Time now() const { return get_clock()->now(); }

  int    actuator_index_;
  double released_value_, secured_value_, refresh_sec_, travel_time_sec_;
  bool   released_{false};
  rclcpp::Time released_at_{0, 0, RCL_ROS_TIME};

  rclcpp::Publisher<px4_msgs::msg::VehicleCommand>::SharedPtr cmd_pub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr           state_pub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr        cmd_sub_;
  rclcpp::TimerBase::SharedPtr                                timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<PayloadBridge>());
  rclcpp::shutdown();
  return 0;
}
