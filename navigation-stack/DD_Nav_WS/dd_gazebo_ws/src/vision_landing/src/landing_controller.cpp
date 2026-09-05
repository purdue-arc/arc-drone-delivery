#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <px4_msgs/msg/trajectory_setpoint.hpp>

class LandingController : public rclcpp::Node
{
public:
  LandingController() : Node("landing_controller")
  {
    sub_ = this->create_subscription<geometry_msgs::msg::PoseStamped>(
      "/landing_target_pose", 10,
      std::bind(&LandingController::pose_callback, this, std::placeholders::_1));

    pub_ = this->create_publisher<px4_msgs::msg::TrajectorySetpoint>(
      "/fmu/in/trajectory_setpoint", 10);

    RCLCPP_INFO(this->get_logger(), "Landing controller started");
  }

private:
  void pose_callback(const geometry_msgs::msg::PoseStamped::SharedPtr msg)
  {
    px4_msgs::msg::TrajectorySetpoint setpoint;
    setpoint.position[0] = msg->pose.position.x;
    setpoint.position[1] = msg->pose.position.y;
    setpoint.position[2] = msg->pose.position.z - 0.5;
    setpoint.yaw = 0.0;
    setpoint.timestamp = this->get_clock()->now().nanoseconds() / 1000;

    pub_->publish(setpoint);
    RCLCPP_INFO(this->get_logger(), "Publishing setpoint: x=%.2f y=%.2f z=%.2f",
      setpoint.position[0], setpoint.position[1], setpoint.position[2]);
  }

  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr sub_;
  rclcpp::Publisher<px4_msgs::msg::TrajectorySetpoint>::SharedPtr pub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LandingController>());
  rclcpp::shutdown();
  return 0;
}