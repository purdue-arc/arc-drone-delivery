#pragma once
#include <functional>
#include <memory>
#include <sstream>
#include <string>

#include "geometry_msgs/msg/transform_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "tf2/LinearMath/Quaternion.h"
#include "tf2/LinearMath/Matrix3x3.h"
#include "tf2_ros/transform_broadcaster.h"
#include "sensor_msgs/msg/imu.hpp"

/**
 * @brief Creating a transform broadcaster which broadcasts the transform from base_link_stabilized->base_link
 * by zeroing out the roll and pitch so there is a frame that is straight up and down(aids in navigation algorithms).
 * This also means the node creates the base_link_stabilized frame as it does not exist prior to this node
 * publishing the transform of which it is the parent frame.
 * 
 */
class StabilizedBroadcaster : public rclcpp::Node 
{
    public: 
        explicit StabilizedBroadcaster();


    private:
        void tf_sub_callback(const std::shared_ptr<sensor_msgs::msg::Imu> msg);
        rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr subscription_;
        std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
};