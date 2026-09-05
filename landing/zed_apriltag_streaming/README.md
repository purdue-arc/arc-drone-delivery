# ZED → Jetson → Laptop AprilTag Detection Pipeline

Production deployment guide for:

- **Jetson + ZED** → Camera + H264 RTP streaming  
- **Laptop (Ubuntu)** → RTP decode + OpenCV + AprilTag + Pose  

Designed for autonomous landing and precision navigation workflows.

---

# 1. System Architecture

```
ZED Camera
     ↓
Jetson (H264 encode + RTP UDP stream)
     ↓
WiFi / Ethernet
     ↓
Laptop (OpenCV decode + AprilTag + Pose)
```

### Responsibility Split

| Machine | Responsibility |
|----------|----------------|
| Jetson | Camera driver + hardware encoding |
| Laptop | RTP decode + AprilTag + Pose |
| Drone (future) | PX4 landing control |

---

# 2. Machine Setup

## 2.1 On the Jetson (Streamer)

### Step 1 — Find IP Addresses

On Jetson:
```bash
ip a
```

On laptop:
```bash
ip a
```

Record:

```
<JETSON_IP>
<LAPTOP_IP>
```

---

### Step 2 — Start ZED RTP Stream (Jetson)

#### Option A — Using ARC Streaming Script

```bash
cd arc-drone-delivery/streaming
./start_stream.sh <LAPTOP_IP>
```

#### Option B — Manual GStreamer

> **Encoder note:** on some Jetson/NVIDIA versions the hardware
> `omxh264enc` element is broken or missing.  If you hit errors or see a
> blank stream, replace it with the software `x264enc` shown in the second
> pipeline below.  The example commands are written with `omxh264enc` for
> historical reasons but `x264enc` is the reliable default on our
> machines.

```bash
gst-launch-1.0 zedsrc ! \
video/x-raw,width=1280,height=720,framerate=15/1 ! \
omxh264enc ! rtph264pay ! \
udpsink host=<LAPTOP_IP> port=5000
```

or (software encoder fallback):
```bash
gst-launch-1.0 -v \
  zedsrc ! \
  video/x-raw,width=1280,height=720,framerate=15/1 ! \
  videoconvert ! \
  video/x-raw,format=I420 ! \
  x264enc tune=zerolatency speed-preset=ultrafast bitrate=4000 key-int-max=30 ! \
  rtph264pay config-interval=1 pt=96 mtu=1200! \
  udpsink host=<LAPTOP_IP> port=5000 sync=false async=false
```

This sends video to:

```
udp://<LAPTOP_IP>:5000
```

---

# 3. Laptop Setup (Detection + Pose)

## 3.1 Build (First Time Only)

```bash
unzip zed_apriltag_streaming_laptop_ready.zip
cd zed_apriltag_streaming

./scripts/setup_ubuntu.sh
./scripts/build.sh
```

> **Note:** if you move the repository or rebase the source tree, the build
> directory may contain an outdated `CMakeCache.txt` pointing at the old path.
> In that case either delete `build/` manually or simply rerun
> `./scripts/build.sh` (it now auto-detects a mismatched cache and removes the
> directory for you).

---

# 4. Verify Networking (Critical Step)

Before running detection:

```bash
vlc udp://@:5000
```
or,
```bash
unset SESSION_MANAGER
```
then run this in the streaming directory to run streaming
```bash
vlc ./zed.sdp
```

If video appears:
- Networking is correct
- RTP stream is valid

Close VLC before running detection.

If nothing appears:

```bash
sudo ufw allow 5000/udp
ping <JETSON_IP>
```

Recommended:
- Same subnet
- No VPN
- 5GHz WiFi or Ethernet


## Troubleshooting “stream does not open”

If `zed_apriltag` prints

```
Failed to open input.
Tried OpenCV+GStreamer pipeline:
  <pipeline string>
```
or the display window never shows any frames, the problem is almost
always a networking/stream issue. Common checks:

1. **Verify the RTP stream locally on the laptop.**
   Run the `gst-launch` command that the binary suggests:
   ```bash
   gst-launch-1.0 udpsrc port=5000 \
     caps="application/x-rtp,media=video,encoding-name=H264,payload=96" ! \
     rtph264depay ! avdec_h264 ! videoconvert ! autovideosink
   ```
   If this shows video you know the stream is reaching the laptop and
   OpenCV/GStreamer should be able to open it as well.

2. **Check that the Jetson is actually streaming.**
   On the Jetson run:
   ```bash
   gst-launch-1.0 zedsrc ! h4enc ! rtph264pay ! \
     udpsink host=<LAPTOP_IP> port=5000
   ```
   and watch `tcpdump -n -i any port 5000` on the laptop – you should see
   UDP packets arriving.

3. **Firewall/port blocking.**
   `sudo ufw status` and make sure UDP 5000 is allowed.

4. **Network path.**
   Both machines must be on the same layer‑2 network; avoid VPNs or
   routers that drop multicast/UDP.  Ping and `arp -n` should show the
   Jetson’s address.

5. **Alternate sources.**
   If you need to debug without networking you can use the webcam or a
   recorded video file (see §6) to confirm that the binary itself is
   functioning.

6. **Pipeline sanity.**
   If you’ve customised the GStreamer string, make sure it matches the
   syntax in `src/zed_gst_pipeline.cpp`.  Typos there will cause
   `cap.isOpened()` to fail silently.

Once the underlying stream is reachable, `./build/zed_apriltag --port 5000`
should open a window and start detecting tags.

---

# 5. Run AprilTag Detection (Laptop)

## 5.1 Stream Mode (Jetson → Laptop)

```bash
./build/zed_apriltag --port 5000
```

Connects to:

```
udp://@:5000
```

---

## 5.2 Pose Estimation Mode (Recommended)

```bash
./build/zed_apriltag \
  --port 5000 \
  --calib assets/your_calibration.yaml \
  --tag-size 0.162
```

### Parameters

| Argument | Description |
|----------|------------|
| `--calib` | Camera intrinsics YAML |
| `--tag-size` | Physical tag size (meters) |

### Output

- Tag ID
- Translation vector (`tvec`)
- Rotation vector (`rvec`)

Suitable for:
- Precision landing
- Drone-relative pose estimation
- SLAM integration

---

# 6. Laptop-Only Testing Mode

Useful for development without Jetson.

## 6.1 Webcam

```bash
./build/zed_apriltag --device 0
```

## 6.2 Video File

```bash
./build/zed_apriltag --video test.mp4
```

## 6.3 Pose with Webcam

```bash
./build/zed_apriltag \
  --device 0 \
  --calib assets/calibration.yaml \
  --tag-size 0.162
```

---

# 7. Clean Deployment Launch Order

1. Start stream on Jetson  
2. Confirm video with VLC on laptop  
3. Close VLC  
4. Run `zed_apriltag` on laptop  

---

# 8. Calibration Notes

For accurate pose estimation:

- Use ZED factory intrinsics or recalibrate
- Include distortion coefficients
- Measure tag size precisely (meters)

Pose error scales linearly with tag-size error.

---

# 9. Directory Structure

```
zed_apriltag_streaming/
├── CMakeLists.txt
├── include/
├── src/
│   └── main.cpp
├── assets/
│   └── calibration.yaml
├── scripts/
│   ├── setup_ubuntu.sh
│   ├── build.sh
│   ├── run_webcam.sh
│   ├── run_video.sh
│   └── run_udp.sh
└── README.md
```

---

# 10. ROS 2 node (`zed_apriltag_node`)

This package is an ament_cmake ROS 2 package. The node is the **hardware
perception source** for the vision landing mission — it decodes the ZED
stream, detects tags, and publishes the pose the mission controller needs.

### Published topics

| Topic | Type | Notes |
|---|---|---|
| `/landing_target_pose` | `geometry_msgs/PoseStamped` | Camera-frame pose of `target_tag_id`. **This is what `vision_landing/mission_controller` consumes.** Only published when pose estimation is enabled. |
| `detections` | `apriltag_msgs/AprilTagDetectionArray` | All detected tags (corners, homography) — debugging/telemetry. |
| `image_debug` | `sensor_msgs/Image` | Annotated frames; only when `publish_debug_image:=true`. |

> **Pose estimation requires intrinsics.** Set `calib_file` (or `fx`/`fy`/
> `cx`/`cy`) **and** `tag_size_m`. Without them the node logs
> `tag_size_m set but intrinsics missing — pose disabled`, never publishes
> `/landing_target_pose`, and the mission will search forever and then
> failsafe-land. Calibrate the actual ZED before flying.

### Key parameters

| Parameter | Default | Description |
|---|---|---|
| `source` | `gst` | `gst` (RTP stream), `v4l2` (local camera), or `file` |
| `gst_pipeline` | UDP H.264 on 5000 | GStreamer pipeline for `source:=gst` |
| `tag_family` | `tag36h11` | AprilTag family |
| `tag_size_m` | `0.0` | Physical tag edge length (m) — required for pose |
| `target_tag_id` | `0` | Tag id to publish as the landing target; `-1` = any |
| `calib_file` | `""` | Camera intrinsics YAML |
| `frame_id` | `camera` | Frame id stamped on published messages |

### Run

```bash
ros2 run zed_apriltag_streaming zed_apriltag_node --ros-args \
  -p tag_size_m:=0.165 \
  -p calib_file:=/path/to/zed_calibration.yaml \
  -p target_tag_id:=0 \
  -p frame_id:=camera_down
```

On the drone this runs as the `zed_apriltag` service in
`docker/docker-compose.hardware.yml`.

---

# 11. Future Integrations

- NTP time synchronization
- EKF fusion with VIO / GPS
- GPU-accelerated detection (isaac_ros_apriltag) on the Jetson

---

# End of Deployment Guide