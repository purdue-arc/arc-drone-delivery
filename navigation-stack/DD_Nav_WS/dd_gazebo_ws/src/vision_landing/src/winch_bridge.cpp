// winch_bridge — delivery winch on PX4 AUX outputs.
//
// The aircraft does not land to deliver: it hovers and lowers the package on
// a winch, then retracts. It only lands to charge, back at the home pad.
//
//   /arc/winch/command (std_msgs/String)  in   "lower" | "release" |
//                                              "retract" | "stow" | "abort"
//   /arc/winch/state   (std_msgs/String)  out  current state, latched
//
// Two AUX actuators, driven with VEHICLE_CMD_DO_SET_ACTUATOR exactly like the
// gimbal and payload servo, so SITL and the aircraft run identical ROS code
// and only PX4 parameters differ:
//   winch_actuator_index    spool motor: +1 pays out, -1 retracts, 0 holds
//   release_actuator_index  hook: released_value drops the package
//
// Assign both to "Peripheral via Actuator Set" outputs (PWM_AUX_FUNCn) and
// calibrate travel with PWM_AUX_MINn/_MAXn.
//
// The sequence is time-based because a winch without an encoder cannot tell
// how much cable is out. lower_sec is therefore a CALIBRATION: measure the
// spool rate on the bench and set it so the package reaches the ground from
// the hover altitude you fly. Getting it wrong either strands the package
// above ground or keeps paying out slack after it lands.
//
// SAFETY: the spool is commanded to hold (0.0) on every terminal state and
// on abort, so a dropped command stream cannot leave the winch running.

//
// OPTIONAL SENSOR: if a stowed limit switch is fitted, publish it as
// std_msgs/Bool on /arc/winch/limit_stowed and set use_limit_switch:=true.
// The timer then stops being the sole authority on whether the cable is in —
// a retract that finishes its timer with the switch still open reports
// "retract_failed" instead of "stowed", and the mission's failsafe path treats
// that as a payload still deployed. Without the switch the behaviour is
// unchanged: open-loop timing, and nothing can contradict it.

#include <rclcpp/rclcpp.hpp>
#include <px4_msgs/msg/vehicle_command.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_msgs/msg/bool.hpp>
#include <cmath>
#include <string>

class WinchBridge : public rclcpp::Node
{
public:
  WinchBridge() : Node("winch_bridge")
  {
    winch_index_   = static_cast<int>(declare_parameter("winch_actuator_index", 2));
    release_index_ = static_cast<int>(declare_parameter("release_actuator_index", 1));
    pay_out_value_ = declare_parameter("pay_out_value", 1.0);
    retract_value_ = declare_parameter("retract_value", -1.0);
    hold_value_    = declare_parameter("hold_value", 0.0);
    hook_closed_   = declare_parameter("hook_closed_value", -1.0);
    hook_open_     = declare_parameter("hook_open_value", 1.0);
    lower_sec_     = declare_parameter("lower_sec", 12.0);
    release_sec_   = declare_parameter("release_sec", 2.0);
    retract_sec_   = declare_parameter("retract_sec", 14.0);
    use_limit_switch_ = declare_parameter("use_limit_switch", false);

    cmd_pub_ = create_publisher<px4_msgs::msg::VehicleCommand>(
      "/fmu/in/vehicle_command", 10);
    state_pub_ = create_publisher<std_msgs::msg::String>(
      "/arc/winch/state", rclcpp::QoS(1).transient_local());
    cmd_sub_ = create_subscription<std_msgs::msg::String>(
      "/arc/winch/command", 10,
      std::bind(&WinchBridge::command_callback, this, std::placeholders::_1));
    limit_sub_ = create_subscription<std_msgs::msg::Bool>(
      "/arc/winch/limit_stowed", rclcpp::QoS(1),
      [this](const std_msgs::msg::Bool::SharedPtr m) {
        limit_stowed_ = m->data;
        limit_seen_ = true;
      });

    timer_ = create_wall_timer(std::chrono::milliseconds(100),
                               std::bind(&WinchBridge::tick, this));

    set_state("stowed");
    hold_spool();
    set_hook(false);
    RCLCPP_INFO(get_logger(),
      "Winch bridge up — spool=actuator %d, hook=actuator %d "
      "(lower %.1fs, retract %.1fs)",
      winch_index_, release_index_, lower_sec_, retract_sec_);
  }

private:
  void command_callback(const std_msgs::msg::String::SharedPtr msg)
  {
    const std::string c = msg->data;
    if (c == "lower")        start("lowering", lower_sec_);
    else if (c == "release") start("releasing", release_sec_);
    else if (c == "retract") start("retracting", retract_sec_);
    else if (c == "stow")    { hold_spool(); set_hook(false); set_state("stowed"); }
    else if (c == "abort")   {
      // Stop the spool where it is and keep the hook shut: an abort must not
      // drop the package.
      hold_spool(); set_hook(false); set_state("aborted");
      RCLCPP_WARN(get_logger(), "Winch ABORT — spool held, hook closed");
    } else {
      RCLCPP_WARN(get_logger(), "Unknown winch command '%s'", c.c_str());
    }
  }

  void start(const std::string & phase, double seconds)
  {
    phase_ = phase;
    phase_end_ = now() + rclcpp::Duration::from_seconds(seconds);
    running_ = true;
    if (phase == "lowering")        { set_hook(false); spool(pay_out_value_); }
    else if (phase == "retracting") { set_hook(false); spool(retract_value_); }
    else if (phase == "releasing")  { hold_spool(); set_hook(true); }
    set_state(phase);
    RCLCPP_INFO(get_logger(), "Winch %s for %.1fs", phase.c_str(), seconds);
  }

  void tick()
  {
    if (!running_) { reassert(); return; }
    if (now() < phase_end_) { reassert(); return; }

    running_ = false;
    hold_spool();
    if (phase_ == "lowering")        set_state("lowered");
    else if (phase_ == "releasing")  set_state("released");
    else if (phase_ == "retracting") {
      set_hook(false);
      // The timer says the spool has run long enough. If a limit switch is
      // fitted, it gets the final say: reporting "stowed" with cable still out
      // is what puts a cable under the rotors on the next descent.
      if (use_limit_switch_ && limit_seen_ && !limit_stowed_) {
        set_state("retract_failed");
        RCLCPP_ERROR(get_logger(),
          "Retract timer finished but the stowed limit switch is still open — "
          "cable is NOT in. Reporting retract_failed.");
        return;
      }
      if (use_limit_switch_ && !limit_seen_) {
        RCLCPP_WARN(get_logger(),
          "use_limit_switch is true but nothing has published "
          "/arc/winch/limit_stowed — falling back to the timer alone");
      }
      set_state("stowed");
    }
    RCLCPP_INFO(get_logger(), "Winch -> %s", state_.c_str());
  }

  // Re-send the current commands: PX4 actuator setpoints are perishable, and
  // a winch that stops mid-pay-out because a message was lost is worse than
  // one that keeps its commanded state.
  void reassert()
  {
    send(winch_index_, last_spool_);
    send(release_index_, hook_open_now_ ? hook_open_ : hook_closed_);
  }

  void spool(double v)  { last_spool_ = v; send(winch_index_, v); }
  void hold_spool()     { spool(hold_value_); }
  void set_hook(bool open)
  {
    hook_open_now_ = open;
    send(release_index_, open ? hook_open_ : hook_closed_);
  }

  void set_state(const std::string & s)
  {
    state_ = s;
    std_msgs::msg::String m;
    m.data = s;
    state_pub_->publish(m);
  }

  void send(int index, double value)
  {
    if (index < 1 || index > 6) return;
    px4_msgs::msg::VehicleCommand cmd;
    cmd.command = px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_SET_ACTUATOR;
    // Untouched channels must be NaN, not 0 — 0 is a valid mid-travel command
    // and would drive every other peripheral servo to centre.
    cmd.param1 = cmd.param2 = cmd.param3 = NAN;
    cmd.param4 = cmd.param5 = cmd.param6 = NAN;
    const float v = static_cast<float>(value);
    switch (index) {
      case 1: cmd.param1 = v; break;
      case 2: cmd.param2 = v; break;
      case 3: cmd.param3 = v; break;
      case 4: cmd.param4 = v; break;
      case 5: cmd.param5 = v; break;
      case 6: cmd.param6 = v; break;
      default: return;
    }
    cmd.param7 = 0.0f;
    cmd.target_system = 1;  cmd.target_component = 1;
    cmd.source_system = 1;  cmd.source_component = 1;
    cmd.from_external = true;
    cmd.timestamp = get_clock()->now().nanoseconds() / 1000;
    cmd_pub_->publish(cmd);
  }

  rclcpp::Time now() const { return get_clock()->now(); }

  int    winch_index_, release_index_;
  double pay_out_value_, retract_value_, hold_value_;
  double hook_closed_, hook_open_;
  double lower_sec_, release_sec_, retract_sec_;
  bool   use_limit_switch_{false};
  bool   limit_seen_{false};
  bool   limit_stowed_{false};
  double last_spool_{0.0};
  bool   hook_open_now_{false};
  bool   running_{false};
  std::string phase_, state_;
  rclcpp::Time phase_end_{0, 0, RCL_ROS_TIME};

  rclcpp::Publisher<px4_msgs::msg::VehicleCommand>::SharedPtr cmd_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr         state_pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr      cmd_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr        limit_sub_;
  rclcpp::TimerBase::SharedPtr                                timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<WinchBridge>());
  rclcpp::shutdown();
  return 0;
}
