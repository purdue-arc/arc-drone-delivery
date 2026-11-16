#pragma once

#include "../header.hh"

namespace rs_tests {
	//No idea what the output represents
	inline rs2_quaternion quaternion_exp(rs2_vector v)
	{
		float x = v.x / 2, y = v.y / 2, z = v.z / 2, th2, th = sqrtf(th2 = x * x + y * y + z * z);
		float c = cosf(th), s = th2 < sqrtf(120 * FLT_EPSILON) ? 1 - th2 / 6 : sinf(th) / th;
		rs2_quaternion Q = { s * x, s * y, s * z, c };
		return Q;
	}
	//This is used to calculate rotation in 3D space using 4 sets of values (x, y, z, w)
	//The output is a new quaternion that describes the final rotated state
	inline rs2_quaternion quaternion_multiply(rs2_quaternion a, rs2_quaternion b)
	{
		rs2_quaternion Q = {
			a.x * b.w + a.w * b.x - a.z * b.y + a.y * b.z,
			a.y * b.w + a.z * b.x + a.w * b.y - a.x * b.z,
			a.z * b.w - a.y * b.x + a.x * b.y + a.w * b.z,
			a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
		};
		return Q;
	}

	//Predicts position given accel, vel, angular accel, and angular vel
	rs2_pose predict_pose(rs2_pose& pose, float dt_s)
	{
		rs2_pose P = pose;
		P.translation.x = dt_s * (dt_s / 2 * pose.acceleration.x + pose.velocity.x) + pose.translation.x;
		P.translation.y = dt_s * (dt_s / 2 * pose.acceleration.y + pose.velocity.y) + pose.translation.y;
		P.translation.z = dt_s * (dt_s / 2 * pose.acceleration.z + pose.velocity.z) + pose.translation.z;
		rs2_vector W = {
				dt_s * (dt_s / 2 * pose.angular_acceleration.x + pose.angular_velocity.x),
				dt_s * (dt_s / 2 * pose.angular_acceleration.y + pose.angular_velocity.y),
				dt_s * (dt_s / 2 * pose.angular_acceleration.z + pose.angular_velocity.z),
		};
		P.rotation = quaternion_multiply(quaternion_exp(W), pose.rotation);
		return P;
	}

	int realsense_test0() {
		std::string serial;
		if (!device_with_streams({ RS2_STREAM_POSE }, serial))
			return EXIT_SUCCESS;

		// creating and configuring a pipeline for rs2
		pipeline pipe;
		config config;
		config.enable_stream(RS2_STREAM_COLOR, WIDTH, HEIGHT, RS2_FORMAT_BGR8, 30);

		std::mutex mutex;
		auto callback = [&](const rs2::frame& frame)
		{
			std::lock_guard<std::mutex> lock(mutex);
			if (rs2::pose_frame fp = frame.as<rs2::pose_frame>()) {
				rs2_pose pose_data = fp.get_pose_data();
				auto now = std::chrono::system_clock::now().time_since_epoch();
				double now_ms = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
				double pose_time_ms = fp.get_timestamp();
				float dt_s = static_cast<float>(std::max(0., (now_ms - pose_time_ms) / 1000.));
				rs2_pose predicted_pose = predict_pose(pose_data, dt_s);
				std::cout << "Predicted " << std::fixed << std::setprecision(3) << dt_s * 1000 << "ms " <<
					"Confidence: " << pose_data.tracker_confidence << " T: " <<
					predicted_pose.translation.x << " " <<
					predicted_pose.translation.y << " " <<
					predicted_pose.translation.z << " (meters)   \r";
				cout << "REACHES HERE";
			}
		};

		pipe.start(config);

		// setting up viewing window
		string window_name = "RealSense";
		namedWindow(window_name, WINDOW_AUTOSIZE);

		// getting realsense data and displaying it through opencv
		frameset frames;
		while (true) {
			frames = pipe.wait_for_frames();
			frame color_frame = frames.get_color_frame();
			Mat color_image = frame_to_mat(color_frame);
			imshow(window_name, color_image);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		waitKey(0);

		return 0;
	}

	int run_rs_test() try {
		return realsense_test0();
	}
	catch (const rs2::error& e) {
		std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}