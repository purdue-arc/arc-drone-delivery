# Setup — Ubuntu 22.04

Day-zero guide: a blank Ubuntu 22.04 machine to a flying SITL mission. This
covers *getting the stack running*; for what the stack does and why, see
[CHANGELOG.md](CHANGELOG.md) (current flight status) and
[docker/README.md](docker/README.md) (architecture). New to Git, ROS 2, or
Gazebo entirely? Start with [onboarding/README.md](onboarding/README.md)
first — it's a from-scratch tutorial, not specific to this repo.

## What you end up with

The flight software (ROS 2 Jazzy) runs in Docker; PX4 SITL + Gazebo Classic
run natively on the host. They talk over ROS 2 DDS on the host network. At
the end of this guide you'll have a simulated drone arm, search, find an
AprilTag, and land on it.

Ubuntu 22.04 is the supported host — not because the flight software needs
22.04 specifically (it runs in an Ubuntu 24.04 container regardless), but
because `network_mode: host` (required for DDS discovery) and the X11 mount
for GUI tools are Linux-native behaviors that aren't reliably supported on
Docker Desktop for Mac/Windows.

## 1. Host packages

```bash
sudo apt update
sudo apt install -y git git-lfs build-essential
```

## 2. Docker Engine (not Docker Desktop)

```bash
curl -fsSL https://get.docker.com | sudo sh
sudo usermod -aG docker $USER
```

Log out and back in (or `newgrp docker`) for the group change to take
effect. Verify with `docker run hello-world`.

## 3. Clone the monorepo

```bash
git clone https://github.com/purdue-arc/arc-drone-delivery.git
cd arc-drone-delivery
```

Init this repo's submodule:

```bash
git submodule update --init --recursive
```

**Gotcha:** `.gitmodules` also lists `dd_gazebo_ws/src/px4-ros2-interface-lib`,
`px4_msgs`, and `px4_ros_com` at a path that doesn't exist in this tree — they
were left behind by the monorepo migration and have no matching gitlink, so
`git submodule update` silently skips them. That's expected: the real copies
are already vendored as plain files at
`navigation-stack/DD_Nav_WS/dd_gazebo_ws/src/`. `pointcloud_to_grid` is
currently the only package that's a real submodule.

## 4. PX4-Autopilot (not part of this repo)

`navigation-stack/PX4-Autopilot/` is a ~4 GB upstream tree and is
gitignored — you clone it yourself. Full detail (including why the exact
version matters for topic names) is in
[navigation-stack/PX4-AUTOPILOT.md](navigation-stack/PX4-AUTOPILOT.md);
short version:

```bash
cd navigation-stack
git clone https://github.com/PX4/PX4-Autopilot.git --recursive
cd PX4-Autopilot
git checkout <tag/commit recorded in PX4-AUTOPILOT.md>
git submodule update --init --recursive
make submodulesclean

# PX4's own build/SITL toolchain + Gazebo Classic installer
bash ./Tools/setup/ubuntu.sh
```

Restart your shell after `ubuntu.sh` (it edits group membership and env for
Gazebo).

## 5. Configure the Docker environment

```bash
cd ../..   # back to repo root
cp docker/.env.example docker/.env
```

Edit `docker/.env` only if your user isn't UID/GID 1000 (`id -u`, `id -g`) —
these get baked into the container user so bind-mounted files aren't owned
by root.

## 6. Build the image

```bash
make build
```

First build is ~10 minutes (pulls and builds `arc-drone:jazzy`, Ubuntu 24.04
+ ROS 2 Jazzy); cached after that.

## 7. Start PX4 SITL (separate terminal, stays running)

```bash
cd navigation-stack/PX4-Autopilot
export GAZEBO_MODEL_PATH=$GAZEBO_MODEL_PATH:$(pwd)/../gazebo_apriltag/models
PX4_SITL_WORLD=apriltag_landing PX4_HOME_ALT=5 \
  make px4_sitl gazebo-classic_typhoon_h480
```

`PX4_SITL_WORLD=apriltag_landing` loads the custom world with a landing tag;
without the `GAZEBO_MODEL_PATH` export, Gazebo can't find its models and the
world fails to load silently.

**Note:** `docker/README.md`'s quick-start snippet shows a shorter
`make px4_sitl gz_typhoon_h480` from a plain `PX4-Autopilot/` directory.
That's stale — the real path is `navigation-stack/PX4-Autopilot`, and the
`gazebo-classic_` target above (with the world/altitude vars) is what this
project's flight software actually expects. Follow `PX4-AUTOPILOT.md`, not
that snippet, if the two disagree.

## 8. Bring up the flight stack

```bash
# back in repo root, second terminal
make up-sitl
make logs SVC=mission     # watch the mission controller boot
```

## 9. Fly it

The mission controller starts **IDLE** and won't arm until told to:

```bash
make start     # arms and takes off — the drone WILL fly
```

It searches, descends through levels until it sees the AprilTag, and lands
on it. To abort mid-flight into `AUTO.LAND`:

```bash
make abort
```

`make down` tears the containers back down.

## Known gotchas

- **This is a monorepo, not a ROS workspace** — there's no top-level `src/`.
  Don't run a bare `colcon build` from the repo root; it finds nothing. The
  container builds into `/home/arc/build_ws` inside itself via
  `entrypoint.sh`. See [docker/README.md](docker/README.md) for the package
  layout.
- **PX4 topic names are a mixed bag** — some publish with a version suffix
  (`/fmu/out/vehicle_status_v2`), some don't
  (`/fmu/out/vehicle_land_detected`). If the mission sits in preflight
  forever with no telemetry, it looks exactly like a dead DDS link but is
  usually this. Verify with `ros2 topic list | grep fmu/out` inside a
  container (`make shell`).
- **`pointcloud_to_grid` is intentionally skipped** in the container build —
  it needs `pcl_ros`, which isn't in the image. This is expected, not a
  broken build.
- Host-native `build/`/`install/` directories (from building PX4's ROS
  packages outside Docker, on a different ROS distro) must not mix with the
  container's build tree — that produces a confusing
  `Package 'vision_landing' not found` crash loop. They should already carry
  `COLCON_IGNORE`; don't remove it.

## Where to go next

| Question | Doc |
|---|---|
| New to Git/ROS 2/Gazebo entirely? | [onboarding/README.md](onboarding/README.md) |
| Full architecture, hardware (Jetson/Tarot) setup | [docker/README.md](docker/README.md) |
| Which PX4 version/topics this is built against | [navigation-stack/PX4-AUTOPILOT.md](navigation-stack/PX4-AUTOPILOT.md) |
| Mission FSM behavior (search, landing, failsafes) | `navigation-stack/DD_Nav_WS/dd_gazebo_ws/src/vision_landing/README.md` |
| Current flight status, what's blocking a real flight | [CHANGELOG.md](CHANGELOG.md) |

Flight status moves fast — check `CHANGELOG.md` rather than assuming
anything here about readiness; as of writing, the full mission flies clean
in SITL but hardware validation is still in progress.
