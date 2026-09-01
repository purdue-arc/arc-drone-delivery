# Vision Landing

Autonomous AprilTag search and precision landing. This is **the** mission
stack for the drone (uXRCE-DDS end-to-end — MAVROS has been dropped; the old
MAVROS FSM in `arc_landing/` is deprecated).

## Architecture — one code path, sim and hardware

```
        H.264 RTP over UDP
 SITL:  gazebo gst camera plugin  :5600  ┐
 HW:    ZED 2i on the Jetson      :5000  ┤
                                          ▼
                              zed_apriltag_node          (perception)
                                          │ /landing_target_pose
                                          ▼
                              mission_controller         (the FSM)
                                 │                    │
              /gimbal_tilt_cmd   │                    │  /fmu/in/*
                                 ▼                    ▼
                          gimbal_bridge            PX4  ── uXRCE-DDS
                                 │ VehicleCommand     │
                                 └──────► PX4 gimbal manager
                                            │
                     SITL: MAVLink ─► sim gimbal plugin
                     HW:   AUX PWM  ─► RCTimer/BruGi board
```

Nothing in the ROS graph changes between simulation and the aircraft. Only
three launch arguments differ (`udp_port`, `tag_size_m`, `calib_file`) and two
PX4 parameters (`MNT_MODE_OUT`, and the AUX output calibration).

Three nodes, launched together by `landing_pipeline.launch.py`:

| Node | Package | Role |
|---|---|---|
| `zed_apriltag_node` | `zed_apriltag_streaming` | Decodes the RTP stream, publishes `/landing_target_pose` |
| `gimbal_bridge` | `vision_landing` | `/gimbal_tilt_cmd` → PX4 gimbal manager |
| `mission_controller` | `vision_landing` | Search + precision-landing FSM |

> `apriltag_detector`, `landing_controller`, and `pose_estimator` in this
> package are **not built**. They are superseded prototypes that would each
> publish onto a topic another node already owns. See `CMakeLists.txt`.

## Mission flow

```
IDLE ─ start ─▶ WARMUP ─▶ TAKEOFF ─▶ SEARCH ─▶ GOTO_TAG ─▶ LAND ─▶ LANDED
                                        ▲  ▲_______________│
                                        │   (tag lost / timeout, ≤ max_land_retries)

with a delivery waypoint:
       TAKEOFF ─▶ TRANSIT ─▶ DELIVER ─▶ RETURN ─▶ SEARCH ─▶ GOTO_TAG ─▶ LAND

any active state, cable stowed ──(battery low / abort / timeout)──▶ FAILSAFE_LAND
any active state, cable OUT    ──(same)──▶ SECURE_PAYLOAD ──▶ FAILSAFE_LAND
any active state ──(EKF invalid / battery CRITICAL)──▶ release payload ▶ FAILSAFE_LAND
any active state ──(RC pilot takes mode, or external disarm)──▶ PILOT_OVERRIDE (terminal)
```

**SECURE_PAYLOAD.** A failsafe during a winch drop used to hand the aircraft to
PX4 `AUTO.LAND` with the package half-lowered — descending onto its own cable.
Now a non-urgent failsafe holds station, retracts, and only then lands; if the
winch has not stowed within `winch_secure_timeout_sec` the hook opens and the
load is dropped before the descent. An *urgent* failsafe (EKF invalid, battery
critical) skips straight to the release: there may be no position estimate left
to hold station with.

- **Start gate**: the node is passive until the operator publishes
  `ros2 topic pub --once /arc/mission/start std_msgs/msg/Bool "{data: true}"`.
  Publishing `{data: false}` mid-flight aborts into PX4 AUTO.LAND.
- **Search**: yaws a full circle at each level — 5 m/80°, 3 m/70°, 2 m/60°,
  1 m/45° — steering the gimbal via `/gimbal_tilt_cmd` (std_msgs/Float64,
  radians below horizontal, negative = down), which `gimbal_bridge` forwards
  to PX4's gimbal manager.
- **Pilot override**: once the RC pilot (or a PX4 failsafe) changes the
  flight mode out of OFFBOARD, the controller goes permanently passive —
  it never fights the pilot.
- All altitudes are relative to the position where the mission was started
  (home is captured at start, not assumed to be NED origin).

## Key parameters

| Parameter | Default | Meaning |
|---|---|---|
| `require_start` | `true` | Wait for `/arc/mission/start` before flying |
| `require_valid_position` | `true` | Refuse to fly / failsafe-land without valid EKF position (set `false` only in SITL experiments) |
| `takeoff_height_m` | `5.0` | Climb height above start point |
| `accept_radius_m` | `0.5` | Horizontal convergence radius over the tag |
| `descent_rate_mps` | `0.2` | Landing descent rate |
| `battery_min_remaining` | `0.25` | Battery fraction that triggers failsafe land |
| `tag_lost_abort_sec` | `2.0` | Tag staleness that aborts a descent (above commit height) |
| `land_commit_height_m` | `1.0` | Below this height the landing commits even if the tag leaves the FOV |
| `max_land_retries` | `2` | Search/descent retries before failsafe land |
| `takeoff_timeout_sec` / `goto_timeout_sec` / `max_search_time_sec` | `30/30/180` | Stage timeouts |
| `max_altitude_m` | `40.0` | Companion-side altitude fence, checked every tick |
| `max_range_m` | `2000.0` | Companion-side range fence from the launch point |
| `require_plan_to_transit` | `true` | With `use_nav2`, refuse to start a transit until Nav2 has returned at least one plan |
| `winch_secure_timeout_sec` | `25.0` | How long a non-urgent failsafe reels the cable in before dropping the load and landing |
| `status_topic` / `local_position_topic` / `battery_topic` / `land_detected_topic` | unversioned `/fmu/out/*` | PX4 topic names — see below |

### The geofence parameters are not the geofence

`max_altitude_m` and `max_range_m` stop the *controller commanding*. They can do
nothing if the companion computer wedges, the container dies, or DDS drops —
in each of those cases the thing that would enforce the limit is the thing that
failed. The `GF_*` parameters in `config/px4/arc_delivery.params` are the layer
that keeps working. Set both.

### PX4 topic names

PX4 v1.16 versions message *definitions* (`msg/versioned/`), but whether the
`uxrce_dds_client` publishes them under a versioned *topic* name
(`vehicle_status_v2`) or the plain one depends on the release. The PX4 in
`navigation-stack/PX4-Autopilot` publishes the **plain** names — there is no
`_v` suffix anywhere in its `dds_topics.yaml` — so those are the defaults.

If the aircraft's Pixhawk runs a release that appends suffixes, override all
four together. A partial override leaves the controller waiting on telemetry
that never arrives, which is indistinguishable from a dead DDS link. Check with:

```bash
make check-px4-topics        # or: ros2 topic list | grep fmu/out
```

## Mission telemetry

The controller publishes its own account of itself on `/arc/mission/state`
(latched, 1 Hz and on every transition):

```
state=DELIVER t=12.4 armed=1 nav_state=14 alt=12.02 pos_valid=1 tag_age=-1.0 winch=lowering battery=0.71 failsafe=none
```

This is the piece the PX4 log cannot supply — it knows what setpoints arrived,
not why. Watch it with `make mission-state`, and fly every hardware test with
`record:=true` so it is bagged alongside `/fmu/out/*`.

## Tests

```bash
make test        # or: colcon test --packages-select vision_landing
```

`test/test_mission_math.cpp` covers the two transforms that decide where the
aircraft ends up: the camera-to-NED projection that places it over the pad, and
the GPS-to-local-NED conversion that places it over the customer. Both fail by
putting the aircraft somewhere else rather than by crashing, which makes a
flight test a very expensive way to find a sign error.

## Running the Simulation

Run each command in a separate terminal, in order:

The team standardised on **Gazebo Classic**. The camera reaches ROS 2 as an
H.264 RTP stream from PX4's `libgazebo_gst_camera_plugin.so` (UDP 5600) — the
same transport the real ZED uses. `libgazebo_ros_camera.so` is deliberately
**not** used: `gazebo_ros_pkgs` was never released for Gazebo Classic on ROS 2
Jazzy, and it is not installed on this machine.

**Terminal 1 — PX4 SITL + Gazebo Classic**
```bash
cd /home/raghav/Documents/arc-drone-delivery/navigation-stack/PX4-Autopilot
export GAZEBO_MODEL_PATH=$GAZEBO_MODEL_PATH:/home/raghav/Documents/arc-drone-delivery/navigation-stack/gazebo_apriltag/models
PX4_SITL_WORLD=apriltag_landing PX4_HOME_ALT=5 make px4_sitl gazebo-classic_typhoon_h480
```

**Terminal 2 — uXRCE DDS Agent (PX4 ↔ ROS2 bridge)**
```bash
MicroXRCEAgent udp4 -p 8888
```

Then set the gimbal parameters once, in the PX4 console (`pxh>`):
```
param set MNT_MODE_IN 4
param set MNT_MODE_OUT 1
```

**Terminal 3 — the whole mission stack** (perception + gimbal + FSM)
```bash
cd /home/raghav/Documents/arc-drone-delivery/navigation-stack/DD_Nav_WS
source install/setup.bash
ros2 launch vision_landing landing_pipeline.launch.py
```
The defaults are the SITL values (UDP 5600, 0.5 m tag, and the 640×360
hfov 2.0 camera's intrinsics fx=fy=205.47, cx=320, cy=180).

> Older revisions of this guide launched `drone_nav master.launch.py` here.
> That package (Nav2/SLAM bring-up) is **deleted from the working tree** —
> it survives only in git history, and the landing mission does not need it.

**Terminal 4 — start the mission** (the controller idles until told to go)
```bash
ros2 topic pub --once /arc/mission/start std_msgs/msg/Bool "{data: true}"
```

To abort at any time: `ros2 topic pub --once /arc/mission/start std_msgs/msg/Bool "{data: false}"`
(forces PX4 AUTO.LAND), or take over with the RC transmitter — the
controller detects the mode change and goes passive.

## Required PX4 parameters

The gimbal is driven through PX4, so these must be set on the flight
controller (reboot required):

| Parameter | SITL | Hardware (RCTimer/BruGi) | Meaning |
|---|---|---|---|
| `MNT_MODE_IN` | `4` | `4` | Accept MAVLink gimbal v2 setpoints (what `gimbal_bridge` sends) |
| `MNT_MODE_OUT` | `2` | `0` | Sim: MAVLink v2 to the gazebo gimbal device. Hardware: `AUX` → PWM |
| `MNT_DO_STAB` | `0` | `0` | PX4 does not stabilise; the gimbal's own IMU does |
| `MNT_RANGE_PITCH` | — | set to the board's travel | Maps commanded degrees onto PWM range |

In SITL the typhoon_h480 airframe already sets `MNT_MODE_IN=4` / `MNT_MODE_OUT=2`,
so no parameter changes are needed. Verify with `px4-param show MNT_MODE_OUT`.

On hardware, wire the assigned AUX output to the BruGi board's **pitch RC
input** and calibrate that channel's `PWM_AUX_MIN` / `_MAX` so a −90°…0°
command sweeps the gimbal from straight-down to level. The BruGi board
self-stabilises on its own IMU, so PX4 only supplies the desired angle.

**Bench-test this with the props removed** before flying: publish
`/gimbal_tilt_cmd` at −1.396, −1.222, −1.047, −0.785 rad and confirm the
camera physically points at 80°, 70°, 60°, 45° below horizontal. If the
direction is inverted, flip the sign via the AUX channel's PWM reversal
rather than changing the mission code.

## Running on hardware

See `docker/README.md`. `make up-hw` runs this same launch file on the Jetson
against the real Pixhawk over UART, overriding `udp_port` (5000),
`tag_size_m`, and `calib_file`.

**The ZED 2i calibration is mandatory.** Without it `zed_apriltag_node` logs
`pose disabled` and never publishes `/landing_target_pose`. Generate it from
the camera's factory calibration:

```bash
landing/zed_apriltag_streaming/scripts/zed_calib_to_yaml.py \
  --conf /usr/local/zed/settings/SN<serial>.conf \
  --side left --resolution HD720 -o zed2i_hd720.yaml
```

Then set `CALIB_FILE` in `docker/.env` to its path inside the container. The
resolution must match what the Jetson actually streams — the node rescales
and warns if it does not, but calibrating at the streamed resolution is
better.
