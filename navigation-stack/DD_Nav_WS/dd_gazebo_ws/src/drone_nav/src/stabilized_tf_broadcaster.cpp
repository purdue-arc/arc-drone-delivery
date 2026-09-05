#include "stabilized_tf_broadcaster.hpp"


StabilizedBroadcaster::StabilizedBroadcaster() : Node("stabilized_broadcaster")
{
    tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
        
    subscription_ = this->create_subscription<sensor_msgs::msg::Imu>("demo/imu", 10,
    std::bind(&StabilizedBroadcaster::tf_sub_callback, this, std::placeholders::_1));
}


void StabilizedBroadcaster::tf_sub_callback(const std::shared_ptr<sensor_msgs::msg::Imu> msg) 
{
    geometry_msgs::msg::TransformStamped transform;

    transform.header.stamp = msg->header.stamp;
    transform.header.frame_id = "base_link_stabilized";
    transform.child_frame_id = "base_link";
    // auto temp_var = msg->header.stamp;
    
    // RCLCPP_INFO_STREAM(this->get_logger(), "Time is: " << temp_var.sec << std::endl);

    //No linear transformations
    transform.transform.translation.x = 0.0;
    transform.transform.translation.y = 0.0;
    transform.transform.translation.z = 0.0;

    tf2::Quaternion q_old(msg->orientation.x, msg->orientation.y, msg->orientation.z, msg->orientation.w);
    tf2::Matrix3x3 m(q_old);
    double roll, pitch, yaw;
    m.getRPY(roll, pitch, yaw); //getting the current roll, pitch and yaw values from the imu


    tf2::Quaternion q_new;
    q_new.setRPY(roll, pitch, 0.0); //transforming the stabilized base link to be stable in both roll and pitch, but not yaw
    transform.transform.rotation.y = q_new.y();
    transform.transform.rotation.x = q_new.x();
    transform.transform.rotation.z = q_new.z();
    transform.transform.rotation.w = q_new.w();

    tf_broadcaster_->sendTransform(transform);
}


int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<StabilizedBroadcaster>());
  rclcpp::shutdown();
  return 0;
}