# PX4-Autopilot — obtaining it, and which version

`navigation-stack/PX4-Autopilot/` is **not** committed to this repo. It is a
~4 GB upstream tree (larger with build output) and it is gitignored. This file
is what is committed in its place: how to get it, and which version the flight
software is written against.

It used to be a git submodule at `navigation-stack/DD_Nav_WS/PX4-Autopilot`.
That submodule was removed and the tree relocated, which is why a `git status`
in this repo once showed tens of thousands of pending deletions.

## Getting it

```bash
cd navigation-stack
git clone https://github.com/PX4/PX4-Autopilot.git --recursive
cd PX4-Autopilot
git checkout <the tag or commit recorded below>
git submodule update --init --recursive
make submodulesclean
```

SITL, Gazebo Classic:

```bash
export GAZEBO_MODEL_PATH=$GAZEBO_MODEL_PATH:$(pwd)/../gazebo_apriltag/models
PX4_SITL_WORLD=apriltag_landing PX4_HOME_ALT=5 \
  make px4_sitl gazebo-classic_typhoon_h480
```

## Which version — and why it matters more than it looks

The mission controller subscribes to PX4 telemetry by topic name. PX4 v1.16
introduced versioned message *definitions* (`msg/versioned/`), and depending on
the release the `uxrce_dds_client` may publish them under a versioned *topic*
name (`/fmu/out/vehicle_status_v2`) or the plain one
(`/fmu/out/vehicle_status`).

Getting this wrong is not a crash. The controller simply never receives status,
position or battery, sits in preflight forever reporting no telemetry, and the
aircraft never arms — which looks exactly like a dead DDS link.

**The version in use publishes a MIXED set**, and this is the single easiest
thing to get wrong here. The `uxrce_dds_client` appends a version suffix at
runtime to exactly those messages that have a definition in `msg/versioned/`,
and leaves the rest bare:

```
/fmu/out/vehicle_status_v2             versioned
/fmu/out/vehicle_local_position_v1     versioned
/fmu/out/battery_status_v1             versioned
/fmu/out/vehicle_land_detected         bare
/fmu/out/vehicle_global_position       bare
/fmu/out/gimbal_device_attitude_status bare
```

`src/modules/uxrce_dds_client/dds_topics.yaml` lists the **base** names and is
not what appears on the wire. Reading it alone will convince you every topic
should be unqualified; the mission will then sit in preflight forever, which
looks exactly like a dead DDS link. Verify empirically, always:

```bash
ros2 topic list | grep fmu/out
```

### Recording the version you build against

Once you have a clone, record what it is here so the next person can reproduce
it:

```bash
cd navigation-stack/PX4-Autopilot
git describe --tags --always
git rev-parse HEAD
```

| Field | Value |
|---|---|
| Tag / release | _record it here_ |
| Commit | _record it here_ |
| Publishes versioned topic names | **mixed** — see above (verified against running SITL) |
| `px4_msgs` in this repo matches its message definitions | yes (verified byte-for-byte for the messages the mission uses) |

### Checking against the actual aircraft

The Pixhawk may not run the same build as your SITL tree. Before flying:

```bash
make check-px4-topics MODE=hw
```

If the aircraft's topic names disagree with the configured ones, override all
four `PX4_*_TOPIC` variables in `docker/.env`. Override all four together — a
partial override produces the same silent stall as no override at all.

## Required PX4 parameters

See `config/px4/` for the parameters this software stack depends on (gimbal
routing, winch AUX outputs, uXRCE-DDS transport, geofence, offboard-loss
behaviour) and the bench tests they imply.
