# PX4 parameters — the other half of the flight software

Everything in `navigation-stack/` and `landing/` is only half the aircraft. The
other half lives in the flight controller's parameter storage, and until this
directory existed it was not written down anywhere: the gimbal routing, the AUX
outputs the winch and hook drive, the DDS transport, and the geofence all
existed solely in the flash of whichever Pixhawk was last configured.

That is a real single point of failure. A companion computer can be reflashed
from git in minutes; a flight controller whose configuration nobody recorded
cannot be, and neither can a second airframe.

## Files

| File | What it is |
|---|---|
| `arc_delivery.params` | The parameters this software stack requires, with the values it expects. **Not a complete airframe tune** — see below. |
| `README.md` | This file. |

## What this is and is not

`arc_delivery.params` covers only the parameters the ROS 2 side depends on. It
deliberately does **not** contain PID gains, sensor calibration, ESC
calibration, or radio setup: those are per-airframe, come out of a calibration
flight, and copying them between vehicles is how you crash one.

Treat it as a checklist that must be true, not as a file to blindly load onto a
new aircraft.

## Exporting the real thing

After the aircraft is configured and flying, export the full parameter set and
commit it next to this file so the configuration is reproducible:

```bash
# In the PX4 console (via QGroundControl's MAVLink shell, or the NuttX console)
param save /fs/microsd/arc_full.params

# Or from QGroundControl: Vehicle Setup > Parameters > Tools > Save to file
```

Commit the export as `arc_full_<airframe>_<date>.params`. Do this again after
any change that affects flight behaviour, and note in the commit message what
changed and why.

## Verifying against a running aircraft

```bash
# Every parameter this stack cares about, with its live value
make shell SVC=mission
ros2 topic list | grep fmu/out        # confirm the DDS link and topic names

# In the PX4 console:
param show MNT_*
param show GF_*
param show PWM_AUX_FUNC*
param show UXRCE_DDS_*
```

## The geofence is not optional

`max_altitude_m` and `max_range_m` in the mission controller are a
*companion-side* fence. They stop the controller commanding the aircraft — they
cannot do anything if the companion computer wedges, the container dies, or the
DDS link drops, because in every one of those cases the thing that would enforce
the limit is the thing that failed.

The `GF_*` parameters are the layer that keeps working. Set both, set them
consistently, and set the PX4 values slightly wider so the companion side trips
first and lands deliberately rather than having PX4 intervene.

## Bench tests these parameters imply

Do these with **props removed**, before any flight:

1. **Gimbal sweep.** Publish `/gimbal_tilt_cmd` at `-1.396`, `-1.222`, `-1.047`,
   `-0.785` rad and confirm against a protractor that the camera points 80, 70,
   60 and 45 degrees below horizontal. If the direction is inverted, fix it with
   the AUX channel's PWM reversal, not in the mission code.
   With `/fmu/out/gimbal_device_attitude_status` flowing, the mission controller
   now uses the *measured* angle and discards detections taken mid-slew — so
   this test also confirms that feedback path is alive.
2. **Winch cycle.** With a representative load on the hook, command
   `lower` / `release` / `retract` on `/arc/winch/command` and time each phase.
   Set `lower_sec` and `retract_sec` from what you measure, at the hover
   altitude you actually intend to fly. The winch has no encoder: these timings
   are the only thing that knows how much cable is out.
3. **Hook direction.** Confirm `hook_closed_value` really holds the load and
   `hook_open_value` really releases it. Getting these backwards drops the
   package on takeoff.
