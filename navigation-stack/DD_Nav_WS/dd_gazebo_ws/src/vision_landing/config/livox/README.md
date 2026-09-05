# Livox Mid-360 configuration

`MID360_config.json` is read by `livox_ros_driver2` and is where the sensor and
this computer find each other. JSON has no comments, so the explanation lives
here.

**Both IP addresses in it are placeholders and neither has been checked against
the real aircraft.** They are upstream's defaults, kept so the file is a
working shape rather than a blank. Set them during the bench bring-up.

## The two numbers that must be right

| Field | What it is | How to find it |
|---|---|---|
| `lidar_configs[0].ip` | the sensor's own address | Factory default is `192.168.1.1xx`, where `xx` is the **last two digits of the serial number** on the sensor body. A Mid-360 with serial `…4721` is `192.168.1.121`. Livox Viewer 2 also shows it. |
| every `host_net_info.*_ip` | **this machine's** address on the lidar network | `ip -4 addr show <iface>`. All five must be the same address. |

The host entries are a *bind* address, not a destination, and the two failures
look completely different — worth knowing which one you have.

**Host IP wrong** (or the interface not up): loud, and observed on a machine
with no such address —

```
bind failed
Failed to init livox lidar sdk.
[ERROR] [livox_mid360]: Init lds lidar fail!
```

**The node does not exit.** It stays in `ros2 node list` looking healthy and
publishes *nothing at all* — not an empty `/livox/points`, no topic. So "the
node is running" is not evidence of anything; check `ros2 topic list` and read
the startup log.

**Sensor IP wrong**, with a valid host IP: the bind succeeds, so expect this one
to be quiet — the driver waits for a sensor that never answers. Untested; we
have no sensor to point it at. `ping` the address first.

`log_data_ip` is deliberately empty — it enables a debug log stream we do not
want on the aircraft.

## The Jetson's network

The sensor is a wired Ethernet device on its own subnet. The companion
computer needs a static address on that subnet — `192.168.1.5/24` here —
configured to come up on boot, or the driver starts before the interface does
and binds to nothing.

The `lidar` compose service uses `network_mode: host`, so it sees the Jetson's
interfaces directly; there is no container networking to configure.

## Fields we do not use

- **`extrinsic_parameter`** stays at zero. It would let the sensor apply the
  mount offset itself, but then the offset lives in a JSON file that nothing
  else in the stack can see. The mount is a TF instead — `mount_x/y/z` and
  `mount_roll/pitch/yaw` in `launch/livox_mid360.launch.py` — so RViz, Nav2's
  raytracing and the flight-level filter all agree about where the sensor is.
  Set it in one place, not two.
- **`pcl_data_type: 1`** is the sensor's own point format (cartesian, 32-bit).
  Unrelated to the ROS message format, which is set by `xfer_format` in the
  launch file.
- **`pattern_mode: 0`** is the default non-repetitive scan pattern.

## Overriding it

The launch file takes `user_config_path`, and the compose service passes
`LIVOX_CONFIG` through to it, so an aircraft-specific file can live outside the
repo:

```bash
LIVOX_CONFIG=/etc/arc/MID360_aircraft.json docker compose --profile lidar up lidar
```

## Firmware

The Mid-360 downloads page carries the firmware and the user manual:
<https://www.livoxtech.com/mid-360/downloads>. Current release at the time of
writing is **v13.18.0244 (2025-04-11)**; it is flashed with Livox Viewer 2 over
the same Ethernet link, not by anything in this repo. Record the version the
aircraft actually runs in `config/px4/README.md` alongside the other
hardware-side settings — a driver/firmware mismatch is one of the few ways this
sensor fails in a way the software cannot diagnose.
