// px4_imu_bridge — px4_msgs/SensorCombined -> sensor_msgs/Imu.
//
// FAST-LIO2 needs an IMU. On the aircraft it should use the Mid-360's OWN
// IMU on /livox/imu: it is rigidly bolted to the laser, sampled by the same
// clock, and its offset from the laser origin is a datasheet constant. That is
// the configuration FAST-LIO was designed around and the one to fly.
//
// This node exists because SITL has no such sensor. Gazebo's Mid-360 stand-in
// is a ray sensor with no IMU, so simulation would have no way to run the
// estimator at all — and an estimator that cannot be run in simulation cannot
// be debugged before it is trusted with an airframe. PX4's own IMU is the only
// one in the sim, and this puts it on a ROS topic in the right frame.
//
// It is also the fallback on the aircraft if the Mid-360's IMU turns out to be
// unusable. Do not reach for that casually: see the two warnings below.
//
// FRAME. SensorCombined is FRD (x forward, y RIGHT, z DOWN) in the airframe
// body. ROS is FLU (y LEFT, z UP). The conversion is (x, -y, -z) on both the
// accelerometer and the gyro. Get this wrong and FAST-LIO still converges —
// onto a mirrored world, which looks plausible in RViz until the aircraft
// turns the wrong way around an obstacle.
//
// TIME. The stamp is ROS wall time at RECEPTION, not msg->timestamp. PX4's
// uORB timestamp is microseconds since flight-controller boot and there is no
// time synchronisation anywhere in this stack, so it cannot be converted to an
// epoch. Every other publisher here — the lidar bridge, the odometry
// broadcaster — already stamps with ROS time for the same reason, so this at
// least puts the IMU in the same clock domain as the cloud FAST-LIO pairs it
// with. It does NOT make the two well synchronised: reception-time stamping
// adds transport jitter that a real IMU driver would not have, and FAST-LIO's
// de-skew assumes lidar and IMU share a clock. Treat SITL results as evidence
// that the pipeline runs, not as evidence of the accuracy it will reach.
//
// GRAVITY. FAST-LIO estimates the gravity vector during its first ~0.1 s and
// needs the aircraft STATIONARY for it. Start the estimator on the pad before
// arming, never mid-flight.

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <px4_msgs/msg/sensor_combined.hpp>

#include <string>

class Px4ImuBridge : public rclcpp::Node
{
public:
  Px4ImuBridge() : Node("px4_imu_bridge")
  {
    // base_link, because that is where PX4's IMU effectively sits. The lidar's
    // offset from it is FAST-LIO's mapping.extrinsic_T, not this frame.
    frame_id_ = declare_parameter<std::string>("frame_id", "base_link");
    output_topic_ = declare_parameter<std::string>("output_topic", "/arc/imu");
    // FAST-LIO wants IMU an order of magnitude faster than the lidar. Below
    // this it still runs and silently gets worse, so say so out loud.
    min_rate_hz_ = declare_parameter<double>("min_rate_hz", 100.0);

    pub_ = create_publisher<sensor_msgs::msg::Imu>(output_topic_, rclcpp::QoS(200));

    rclcpp::QoS qos(rclcpp::KeepLast(200));
    qos.best_effort();  // PX4's uXRCE-DDS publisher is best-effort.
    sub_ = create_subscription<px4_msgs::msg::SensorCombined>(
      "/fmu/out/sensor_combined", qos,
      std::bind(&Px4ImuBridge::on_imu, this, std::placeholders::_1));

    RCLCPP_INFO(get_logger(),
                "/fmu/out/sensor_combined (FRD) -> '%s' (FLU, frame %s)",
                output_topic_.c_str(), frame_id_.c_str());
  }

private:
  void on_imu(const px4_msgs::msg::SensorCombined::SharedPtr msg)
  {
    sensor_msgs::msg::Imu out;
    out.header.stamp = now();
    out.header.frame_id = frame_id_;

    out.angular_velocity.x = msg->gyro_rad[0];
    out.angular_velocity.y = -msg->gyro_rad[1];
    out.angular_velocity.z = -msg->gyro_rad[2];

    out.linear_acceleration.x = msg->accelerometer_m_s2[0];
    out.linear_acceleration.y = -msg->accelerometer_m_s2[1];
    out.linear_acceleration.z = -msg->accelerometer_m_s2[2];

    // SensorCombined carries no attitude. -1 in the leading element is the
    // REP-145 signal for "this field is absent"; leaving it zero would tell a
    // consumer the identity quaternion is known perfectly. FAST-LIO ignores
    // orientation and solves for it, which is the whole point.
    out.orientation_covariance[0] = -1.0;

    pub_->publish(out);
    report_rate();
  }

  void report_rate()
  {
    const rclcpp::Time t = now();
    if (window_start_.nanoseconds() == 0) {
      window_start_ = t;
      window_count_ = 0;
      return;
    }
    ++window_count_;
    const double elapsed = (t - window_start_).seconds();
    if (elapsed < 10.0) return;

    const double hz = window_count_ / elapsed;
    if (hz < min_rate_hz_) {
      RCLCPP_WARN(get_logger(),
                  "IMU at %.0f Hz, below the %.0f Hz FAST-LIO needs. The "
                  "estimator will run and drift rather than fail. Check the "
                  "uXRCE-DDS link and that sensor_combined has no rate_limit "
                  "in dds_topics.yaml.",
                  hz, min_rate_hz_);
    } else {
      RCLCPP_INFO(get_logger(), "IMU at %.0f Hz", hz);
    }
    window_start_ = t;
    window_count_ = 0;
  }

  std::string frame_id_, output_topic_;
  double min_rate_hz_{100.0};
  rclcpp::Time window_start_{0, 0, RCL_ROS_TIME};
  uint64_t window_count_{0};
  rclcpp::Subscription<px4_msgs::msg::SensorCombined>::SharedPtr sub_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr pub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Px4ImuBridge>());
  rclcpp::shutdown();
  return 0;
}
