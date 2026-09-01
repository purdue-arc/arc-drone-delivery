#include "odom_publisher_broadcaster.hpp"


OdomPublisherBroadcaster::OdomPublisherBroadcaster() : Node("odom_publisher_broadcaster")
{
    tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
    rclcpp::QoS qos_profile(10);
    qos_profile.reliability(RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT); //Changing the QoS Reliability setting to best effort as the publisher is best effort
    subscription_ = this->create_subscription<px4_msgs::msg::VehicleOdometry>("fmu/out/vehicle_odometry", qos_profile,
    std::bind(&OdomPublisherBroadcaster::pos_sub_callback, this, std::placeholders::_1));
    publisher_ = this->create_publisher<nav_msgs::msg::Odometry>("/odometry/filtered", 10);
}


void OdomPublisherBroadcaster::pos_sub_callback(const std::shared_ptr<px4_msgs::msg::VehicleOdometry> msg)
{
    geometry_msgs::msg::TransformStamped transform;
    nav_msgs::msg::Odometry odom_msg;
    // Stamp with ROS time, NOT msg->timestamp.
    //
    // PX4's uORB timestamp is microseconds since FLIGHT CONTROLLER BOOT, not
    // since the epoch, and there is no time sync in this stack to convert it.
    // Using it directly stamped the odom->base_footprint transform at ~580 s
    // (i.e. 1970) while every other publisher — the lidar bridge, the static
    // transforms — used real ROS time. Nav2's costmap then dropped every
    // point cloud with
    //   "the timestamp on the message is earlier than all the data in the
    //    transform cache"
    // and the occupancy grid stayed permanently empty.
    //
    // The old code also computed nanosec as timestamp*1000, which overflows
    // uint32 for any non-trivial uptime and produced garbage regardless.
    const builtin_interfaces::msg::Time time = this->now();

    //Setting the header and time stamps of the transform and published odometry msg
    transform.header.stamp = time;
    transform.header.frame_id = "odom";
    transform.child_frame_id = "base_footprint";
    odom_msg.header.stamp = time;
    odom_msg.header.frame_id = "odom";
    odom_msg.child_frame_id = "base_link"; //MIGHRT NEED TO CHANGE MESSAGE SO CHILD FRAME IS BASE_FOOTPRINT FOR NAV2 PURPOSES!!!!! IF CHANGE NEED TO CHANGE ODOM MSG CONTENTS AS WELL


    //Converting position, orientation and velocity as well as covariances for eachfrom px4 NED and FRD frames to ros2 ENU and FLU frames
    Eigen::Quaterniond q = px4_ros_com::frame_transforms::utils::quaternion::array_to_eigen_quat(msg->q);
    Eigen::Quaterniond enu_flu_q = px4_ros_com::frame_transforms::px4_to_ros_orientation(q); //Need to convert both world and body frames as orientation is defined relative to both
    Eigen::Vector3d position = Eigen::Vector3d(msg->position[0], msg->position[1], msg->position[2]); 
    Eigen::Vector3d enu_position = px4_ros_com::frame_transforms::ned_to_enu_local_frame(position); //Need to only convert world frame as position is only defined in reference to NED
    Eigen::Vector3d velocity = Eigen::Vector3d(msg->velocity[0], msg->velocity[1], msg->velocity[2]);
    Eigen::Vector3d enu_velocity = px4_ros_com::frame_transforms::ned_to_enu_local_frame(velocity); //Need to only convert world frame as velocity is only defined in reference to NED
    Eigen::Vector3d angular_velocity = Eigen::Vector3d(msg->angular_velocity[0], msg->angular_velocity[1], msg->angular_velocity[2]);
    Eigen::Vector3d flu_angular_velocity = px4_ros_com::frame_transforms::aircraft_to_baselink_body_frame(angular_velocity); //Need to convert body frame from FRD to FLU as angular velocity is defined relative to body frame but not world frame

    //Setting the values for new odom message
    odom_msg.pose.pose.position.x = enu_position.x();
    odom_msg.pose.pose.position.y = enu_position.y();
    odom_msg.pose.pose.position.z = enu_position.z();
    odom_msg.pose.pose.orientation.x = enu_flu_q.x();
    odom_msg.pose.pose.orientation.y = enu_flu_q.y();
    odom_msg.pose.pose.orientation.z = enu_flu_q.z();
    odom_msg.pose.pose.orientation.w = enu_flu_q.w();
    odom_msg.twist.twist.linear.x = enu_velocity.x();
    odom_msg.twist.twist.linear.y = enu_velocity.y();
    odom_msg.twist.twist.linear.z = enu_velocity.z();
    odom_msg.twist.twist.angular.x = flu_angular_velocity.x();
    odom_msg.twist.twist.angular.y = flu_angular_velocity.y();
    odom_msg.twist.twist.angular.z = flu_angular_velocity.z();
    //Only non-zero values on the diagonal as the off-diagonal values indicate their is relation between the vairables. which there is not
    odom_msg.pose.covariance = {msg->position_variance[0], 0.0, 0.0, 0.0, 0.0, 0.0,
                                0.0, msg->position_variance[1], 0.0, 0.0, 0.0, 0.0,
                                0.0, 0.0, msg->position_variance[2], 0.0, 0.0, 0.0,
                                0.0, 0.0, 0.0, msg->orientation_variance[0], 0.0, 0.0,
                                0.0, 0.0, 0.0, 0.0, msg->orientation_variance[1], 0.0,
                                0.0, 0.0, 0.0, 0.0, 0.0, msg->orientation_variance[2]}; 
    odom_msg.twist.covariance = {msg->position_variance[0], 0.0, 0.0, 0.0, 0.0, 0.0,
                                0.0, msg->position_variance[1], 0.0, 0.0, 0.0, 0.0,
                                0.0, 0.0, msg->position_variance[2], 0.0, 0.0, 0.0,
                                0.0, 0.0, 0.0, msg->orientation_variance[0], 0.0, 0.0,
                                0.0, 0.0, 0.0, 0.0, msg->orientation_variance[1], 0.0,
                                0.0, 0.0, 0.0, 0.0, 0.0, msg->orientation_variance[2]}; 

    //setting the values for the odom->basefootprint transform
    transform.transform.translation.x = enu_position.x();
    transform.transform.translation.y = enu_position.y();
    transform.transform.translation.z = 0.0; //No z movement as base_footprint

    tf2::Quaternion q_old(enu_flu_q.x(), enu_flu_q.y(), enu_flu_q.z(), enu_flu_q.w());
    tf2::Matrix3x3 m(q_old);
    double roll, pitch, yaw;
    m.getRPY(roll, pitch, yaw); //getting the current roll, pitch and yaw values from the imu


    tf2::Quaternion q_new;
    q_new.setRPY(0.0, 0.0, yaw); //transforming the stabilized base link to be stable in both roll and pitch, but not yaw
    transform.transform.rotation.y = q_new.y();
    transform.transform.rotation.x = q_new.x();
    transform.transform.rotation.z = q_new.z();
    transform.transform.rotation.w = q_new.w();


    //Publishing the ros2 compatible odom msg and odom->base_footprint transform
    publisher_->publish(odom_msg);
    tf_broadcaster_->sendTransform(transform);
}


int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<OdomPublisherBroadcaster>());
  rclcpp::shutdown();
  return 0;
}