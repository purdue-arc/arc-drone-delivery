#pragma once
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <Eigen/Eigen>
#include <Eigen/Geometry>
#include <array>

#include "geometry_msgs/msg/transform_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "px4_msgs/msg/vehicle_odometry.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "tf2/LinearMath/Quaternion.h"
#include "tf2/LinearMath/Matrix3x3.h"
#include "tf2_ros/transform_broadcaster.h"
#include "frame_transforms.h"


class OdomPublisherBroadcaster : public rclcpp::Node 
{
    public:
        explicit OdomPublisherBroadcaster();


    private:
        void pos_sub_callback(const std::shared_ptr<px4_msgs::msg::VehicleOdometry> msg);
        rclcpp::Subscription<px4_msgs::msg::VehicleOdometry>::SharedPtr subscription_;
        rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr publisher_;
        std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
};