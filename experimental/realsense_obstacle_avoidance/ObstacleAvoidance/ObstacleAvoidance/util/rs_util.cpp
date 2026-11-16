// header file
/*#include "../header.hh"


// checks if given stream is supported
bool device_with_streams(std::vector <rs2_stream> stream_requests, std::string& out_serial) {
    context current_context;
    auto devices = current_context.query_devices();
    vector <rs2_stream> unavailable_streams = stream_requests;
    for (auto dev : devices) {
        map<rs2_stream, bool> found_streams;
        for (auto& type : stream_requests) {
            found_streams[type] = false;
            for (auto& sensor : dev.query_sensors()) {
                for (auto& profile : sensor.get_stream_profiles()) {
                    if (profile.stream_type() == type) {
                        found_streams[type] = true;
                        unavailable_streams.erase(std::remove(unavailable_streams.begin(), unavailable_streams.end(), type), unavailable_streams.end());
                        if (dev.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
                            out_serial = dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
                    }
                }
            }
        }

        for (auto& stream : found_streams) {
            if (!stream.second) {
                return false;
            }
        }
        return true;
    }
}

// converts a realsense frame to opencv viewable "mat"
Mat frame_to_mat(frame frame) {
    Mat color_image(Size(WIDTH, HEIGHT), CV_8UC3, (void*)frame.get_data(), Mat::AUTO_STEP);
    return color_image;
}

// Checks if file exists
inline bool file_exists(const char* name) {
    if (FILE* file = fopen(name, "r")) {
        fclose(file);
        return true;
    }
    else {
        return false;
    }
}*/