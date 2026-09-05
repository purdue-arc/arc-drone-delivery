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
// #include "sensor_msgs/msg/nav_sat_fix.hpp"
#include "px4_msgs/msg/sensor_gps.hpp"



/**
 * @brief Creating a transform broadcaster which broadcasts the transform from base_footprint->base_link_stabilized
 * by zeroing out the z-axis variable of the coordinate frame which allows 2D navigation algorithms to control the drone's movements.
 * This also means the node creates the base_footprint frame as it does not exist prior to this node
 * publishing the transform of which it is the parent frame.
 * 
 */
class FootprintBroadcaster : public rclcpp::Node 
{
    public: 
        explicit FootprintBroadcaster();


    private:
        // void tf_sub_callback(const std::shared_ptr<sensor_msgs::msg::NavSatFix> msg);
        // rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr subscription_;
        // std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
        void tf_sub_callback(const std::shared_ptr<px4_msgs::msg::SensorGps> msg);
        rclcpp::Subscription<px4_msgs::msg::SensorGps>::SharedPtr subscription_;
        std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
};