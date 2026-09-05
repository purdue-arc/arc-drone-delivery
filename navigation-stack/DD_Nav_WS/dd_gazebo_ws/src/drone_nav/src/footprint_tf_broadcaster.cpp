#include "footprint_tf_broadcaster.hpp"


// FootprintBroadcaster::FootprintBroadcaster() : Node("footprint_broadcaster")
// {
//     tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
//     rclcpp::QoS qos_profile(10);
//     //qos_profile.reliability(RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT); //Changing the QoS Reliability setting to best effort as the publisher is best effort
//     subscription_ = this->create_subscription<sensor_msgs::msg::NavSatFix>("gps/fix", qos_profile,
//     std::bind(&FootprintBroadcaster::tf_sub_callback, this, std::placeholders::_1));
// }


// void FootprintBroadcaster::tf_sub_callback(const std::shared_ptr<sensor_msgs::msg::NavSatFix> msg) 
// {
//     geometry_msgs::msg::TransformStamped transform;

//     transform.header.stamp = msg->header.stamp;
//     transform.header.frame_id = "base_footprint";
//     transform.child_frame_id = "base_link_stabilized";
//     auto temp_var = msg->header.stamp;
//     // temp_var = temp_var.seconds();
    
//     //base_link_stabilized frame, but on the ground
//     transform.transform.translation.x = 0.0;
//     transform.transform.translation.y = 0.0;
//     transform.transform.translation.z = msg->altitude;
//     RCLCPP_INFO_STREAM(this->get_logger(), "Time is: " << temp_var.sec << " Altitude Broadcasted: " << msg->altitude << std::endl);
//     tf_broadcaster_->sendTransform(transform);
// }


// int main(int argc, char * argv[])
// {
//   rclcpp::init(argc, argv);
//   rclcpp::spin(std::make_shared<FootprintBroadcaster>());
//   rclcpp::shutdown();
//   return 0;
// }


FootprintBroadcaster::FootprintBroadcaster() : Node("footprint_broadcaster")
{
    tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
    rclcpp::QoS qos_profile(10);
    qos_profile.reliability(RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT); //Changing the QoS Reliability setting to best effort as the publisher is best effort
    subscription_ = this->create_subscription<px4_msgs::msg::SensorGps>("fmu/out/vehicle_gps_position", qos_profile,
    std::bind(&FootprintBroadcaster::tf_sub_callback, this, std::placeholders::_1));
}


void FootprintBroadcaster::tf_sub_callback(const std::shared_ptr<px4_msgs::msg::SensorGps> msg) 
{
    geometry_msgs::msg::TransformStamped transform;
    builtin_interfaces::msg::Time time;
    time.sec = int32_t(msg->timestamp / 1000000); //converting the timestamp from micro-seconds to seconds
    time.nanosec = u_int32_t(msg->timestamp * 1000); //converting the timestamp from micro-seconds to nano-seconds

    transform.header.stamp = time;
    transform.header.frame_id = "base_footprint";
    transform.child_frame_id = "base_link_stabilized";
    auto temp_var = time;

    //base_link_stabilized frame, but on the ground
    transform.transform.translation.x = 0.0;
    transform.transform.translation.y = 0.0;
    transform.transform.translation.z = msg->altitude_msl_m;
    RCLCPP_INFO_STREAM(this->get_logger(), "Time is: " << temp_var.sec << " Altitude Broadcasted: " << msg->altitude_msl_m << std::endl);
    tf_broadcaster_->sendTransform(transform);
}


int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FootprintBroadcaster>());
  rclcpp::shutdown();
  return 0;
}