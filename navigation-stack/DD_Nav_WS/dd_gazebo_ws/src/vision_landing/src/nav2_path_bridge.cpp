// nav2_path_bridge — Nav2 as a PLANNER for the delivery transit leg.
//
//   /arc/transit/goal  (geometry_msgs/PoseStamped)  in  <- mission_controller
//   /arc/transit/path  (nav_msgs/Path)              out -> mission_controller
//
// Deliberately uses Nav2's ComputePathToPose action rather than
// NavigateToPose: we want a path, not a driver. NavigateToPose would start
// Nav2's controller server, which publishes /cmd_vel and would be a second
// authority fighting mission_controller for the aircraft — the same class of
// bug as the old landing_controller. Nav2 plans; the mission flies.
//
// The goal is latched and replanned on a timer so the path keeps reflecting
// the live costmap as the lidar reveals obstacles during transit.

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <nav2_msgs/action/compute_path_to_pose.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <nav_msgs/msg/path.hpp>

using ComputePathToPose = nav2_msgs::action::ComputePathToPose;
using GoalHandle = rclcpp_action::ClientGoalHandle<ComputePathToPose>;

class Nav2PathBridge : public rclcpp::Node
{
public:
  Nav2PathBridge() : Node("nav2_path_bridge")
  {
    replan_sec_   = declare_parameter("replan_sec", 2.0);
    planner_id_   = declare_parameter<std::string>("planner_id", "GridBased");
    server_wait_s_= declare_parameter("server_wait_sec", 5.0);
    // Backstop for a planner that accepts a goal and never returns a result.
    goal_timeout_sec_ = declare_parameter("goal_timeout_sec", 10.0);

    client_ = rclcpp_action::create_client<ComputePathToPose>(
      this, "compute_path_to_pose");

    goal_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "/arc/transit/goal", 10,
      std::bind(&Nav2PathBridge::goal_callback, this, std::placeholders::_1));
    path_pub_ = create_publisher<nav_msgs::msg::Path>("/arc/transit/path", 10);

    timer_ = create_wall_timer(
      std::chrono::milliseconds(static_cast<int>(replan_sec_ * 1000)),
      std::bind(&Nav2PathBridge::replan, this));

    RCLCPP_INFO(get_logger(),
      "Nav2 path bridge up — planning with '%s' every %.1fs",
      planner_id_.c_str(), replan_sec_);
  }

private:
  void goal_callback(const geometry_msgs::msg::PoseStamped::SharedPtr msg)
  {
    goal_ = *msg;
    have_goal_ = true;
    RCLCPP_INFO(get_logger(), "Transit goal set: (%.1f, %.1f) in '%s'",
                goal_.pose.position.x, goal_.pose.position.y,
                goal_.header.frame_id.c_str());
    replan();
  }

  void replan()
  {
    if (!have_goal_) return;

    // A goal whose result callback never fires used to latch in_flight_ true
    // forever: replanning stopped, the path went stale, and the mission
    // silently reverted to straight-line for the rest of the transit with no
    // indication that the planner had stopped answering. Time it out instead.
    if (in_flight_) {
      const double waiting = (now() - goal_sent_time_).seconds();
      if (waiting < goal_timeout_sec_) return;
      RCLCPP_WARN(get_logger(),
        "Planner did not answer within %.1fs — abandoning that goal and retrying",
        goal_timeout_sec_);
      in_flight_ = false;
    }

    if (!client_->action_server_is_ready()) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
        "Nav2 planner server not available — mission will fly straight-line");
      return;
    }

    ComputePathToPose::Goal goal;
    goal.goal = goal_;
    goal.use_start = false;      // plan from the vehicle's current pose
    goal.planner_id = planner_id_;

    rclcpp_action::Client<ComputePathToPose>::SendGoalOptions opts;
    opts.result_callback = [this](const GoalHandle::WrappedResult & r) {
      in_flight_ = false;
      if (r.code != rclcpp_action::ResultCode::SUCCEEDED) {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
          "Planning failed (code %d) — mission falls back to straight-line",
          static_cast<int>(r.code));
        return;
      }
      if (r.result->path.poses.empty()) {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
          "Planner returned an empty path");
        return;
      }
      path_pub_->publish(r.result->path);
      RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 5000,
        "Published plan: %zu poses", r.result->path.poses.size());
    };
    opts.goal_response_callback = [this](GoalHandle::SharedPtr h) {
      if (!h) {
        in_flight_ = false;
        RCLCPP_WARN(get_logger(), "Planner rejected the goal");
      }
    };

    in_flight_ = true;
    goal_sent_time_ = now();
    client_->async_send_goal(goal, opts);
  }

  double replan_sec_, server_wait_s_, goal_timeout_sec_;
  rclcpp::Time goal_sent_time_{0, 0, RCL_ROS_TIME};
  std::string planner_id_;
  geometry_msgs::msg::PoseStamped goal_;
  bool have_goal_{false};
  bool in_flight_{false};

  rclcpp_action::Client<ComputePathToPose>::SharedPtr client_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr goal_sub_;
  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr path_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  rclcpp::Time now() const { return get_clock()->now(); }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Nav2PathBridge>());
  rclcpp::shutdown();
  return 0;
}
