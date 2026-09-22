# Setup — Ubuntu 22.04 / 24.04

Day-zero guide: a blank Ubuntu machine to a flying SITL mission. This covers
*getting the stack running*; for what the stack does and why, see
[CHANGELOG.md](CHANGELOG.md) (current flight status) and
[docker/README.md](docker/README.md) (architecture). New to Git, ROS 2, or
Gazebo entirely? Start with [onboarding/README.md](onboarding/README.md)
first — it's a from-scratch tutorial, not specific to this repo.

The numbered steps below are written for **22.04**, the fully-validated host
— someone has taken a blank 22.04 machine through every step, including
flying the mission. **24.04** works too, but with one real gap: see
[Setup on Ubuntu 24.04](#setup-on-ubuntu-2404) before you start Step 4.

## What you end up with

The flight software (ROS 2 Jazzy) runs in Docker; PX4 SITL + Gazebo Classic
run natively on the host. They talk over ROS 2 DDS on the host network. At
the end of this guide you'll have a simulated drone arm, search, find an
AprilTag, and land on it.

A Linux host (not Mac/Windows) is required — not because the flight software
needs it specifically (it runs in an Ubuntu 24.04 container regardless of
host distro), but because `network_mode: host` (required for DDS discovery)
and the X11 mount for GUI tools are Linux-native behaviors that aren't
reliably supported on Docker Desktop for Mac/Windows. Within Linux, 22.04 and
24.04 both work for that part; the one place the two diverge is **Gazebo
Classic packaging**, covered below.

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

## Setup on Ubuntu 24.04

Steps 1, 2, 3, 5, 6, 8 and 9 above are unchanged on 24.04 — cloning the repo,
Docker Engine, and building/running the `arc-drone:jazzy` container don't
care about host distro. **Step 4 (PX4-Autopilot) and Step 7 (starting SITL)
are where 24.04 diverges**, because of Gazebo, not PX4 itself.

### The blocker: Gazebo Classic has no package for 24.04 (noble)

This project's SITL setup uses **Gazebo Classic** (`gazebo11`,
`make px4_sitl gazebo-classic_typhoon_h480`), not the newer Gazebo (`gz`)
line. Gazebo Classic was only ever packaged by Ubuntu/OSRF through **22.04
(jammy)** — there is no `gazebo11` package for 24.04. PX4 itself confirms
this: `Tools/setup/ubuntu.sh` in current PX4-Autopilot checkouts installs
**Gazebo Harmonic** on 22.04+ (it no longer installs Classic on any distro),
and PX4's own docs describe reinstalling Gazebo Classic as a 22.04-only
procedure.

Since this repo's custom world (`apriltag_landing`) and models
(`navigation-stack/PX4-Autopilot/../gazebo_apriltag/models`) are built for
Classic, not Harmonic, `bash ./Tools/setup/ubuntu.sh` and the
`gazebo-classic_typhoon_h480` make target in Step 4/7 do not work out of the
box on a 24.04 host — `apt install gazebo11` has no candidate to install.

### What to do about it

Pick one, depending on why you're on 24.04:

- **You just want to fly the mission in SITL (most new members):** do Step 4
  and Step 7 (the native PX4/Gazebo pieces) from a 22.04 VM (e.g.
  [Multipass](https://multipass.run/) or VirtualBox) or a 22.04 machine, and
  run everything else — Docker build, `make up-sitl`, `make start` — from
  there too. It's simplest to keep the whole guide on one 22.04 environment
  rather than split native Gazebo onto a VM and Docker onto the 24.04 host;
  splitting them means bridging ROS 2 DDS discovery across two machines/VMs,
  which is real extra work `network_mode: host` doesn't solve for you.
- **You mainly need the Docker flight stack (not native SITL)** — e.g.
  working on the mission controller, ROS 2 nodes, or anything that runs
  inside `arc-drone:jazzy` — 24.04 works unmodified. Skip Step 4/7 and come
  back to them later if you need full SITL.
- **You want to build `gazebo11` from source on 24.04 anyway:** it's
  possible in principle but unverified by this team and not documented here
  — expect to hand-resolve dependency versions. Search
  [PX4/PX4-Autopilot#20834](https://github.com/PX4/PX4-Autopilot/issues/20834)
  and the [Gazebo/Open Robotics Discourse](https://discourse.openrobotics.org/)
  for the current state before sinking time into it.

If the project migrates the sim to Gazebo Harmonic in the future (removing
this gap entirely), that'll show up as a `CHANGELOG.md` entry — check there
before assuming this section is still accurate.

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
