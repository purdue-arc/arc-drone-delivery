# px4_msgs vs the firmware in this repo

`px4_msgs` here is a vendored snapshot. Compared field-by-field against
`navigation-stack/PX4-Autopilot/msg` (and `msg/versioned/`) on 2026-09-01:

| | count |
|---|---|
| identical | 149 |
| **differ from the firmware** | **63** |
| present here but not in the firmware | 15 |

## Why this matters more than it looks

A message whose definition differs by even one field has a different
serialized size, and the uXRCE-DDS bridge then refuses the sample:

```
[RTPS_READER_HISTORY Error] Change payload size of '60' bytes is larger than
the history payload size of '55' bytes and cannot be resized.
```

The subscription stays "connected", `ros2 topic list` shows the topic, and
**every sample is silently dropped**. Nothing crashes; the data just never
arrives. This is the same class of failure as the
`payload size 168 > 167` note in `drone_nav`'s launch comments.

## What has been fixed

`GimbalDeviceAttitudeStatus` was missing three fields the firmware publishes
(`delta_yaw`, `delta_yaw_velocity`, `gimbal_device_id`) — a 5-byte gap that
dropped every gimbal attitude sample at ~5 Hz and left the mission controller
silently falling back to the commanded gimbal angle instead of the measured
one. Synced from the firmware.

## What has NOT been fixed

The other 62. **None of them are used by the mission stack** — every message
`vision_landing` and `drone_nav` subscribe to or publish was verified
identical to the firmware. They are a latent hazard for anyone who adds a new
subscription, not an active bug.

Before subscribing to any new `/fmu/` topic, check it:

```bash
diff PX4-Autopilot/msg/<Name>.msg \
     DD_Nav_WS/dd_gazebo_ws/src/px4_msgs/msg/<Name>.msg
# versioned messages live in PX4-Autopilot/msg/versioned/
```

The proper fix is to re-vendor `px4_msgs` at the release matching the
firmware, then re-run that comparison across all messages and re-test.
