# Software Change Log

**What this is.** One running record of every significant change to the flight
software, written so that anyone on the team can read it — you should not need
to know ROS or PX4 to understand what changed on the aircraft and what it means
for you.

**Who it's for.** Everyone. Software people get the detail. Hardware and
airframe people get a **Hardware impact** section in every entry saying what,
if anything, you now have to wire, set, measure or bench-test.

**Where the truth lives.** This file, in the repo, is canonical. If a summary
elsewhere disagrees with this file, this file wins.

There is a glossary at the bottom. Any term in `code font` that looks like
jargon is probably in it.

---

## How to add an entry

Add one **at the top of the Entries section** whenever you do something in the
"major" column:

| Major — write an entry | Minor — no entry needed |
|---|---|
| Changes what the aircraft does in the air | Refactoring with no behaviour change |
| Adds, removes or renames a parameter, topic or launch argument | Comments, formatting, typo fixes |
| Changes anything the hardware team must set or wire | Changes only to test code |
| Changes how the software is built, deployed or launched | Log message wording |
| Fixes a bug that could have caused a bad flight | Documentation-only edits |
| Adds or removes a dependency | |

Copy this template:

```markdown
## YYYY-MM-DD — Short title

**In one sentence:** what changed, in plain language.

### What changed
- Grouped bullets. Name files and parameters.

### Why
The problem this solves, and what went wrong without it.

### Hardware impact
What the hardware team must do. Write "None." if genuinely none —
do not leave it blank.

### How it was verified
What was actually run and what the result was. If it was not tested,
say "Not verified" and why.

### Risks still open
Anything this leaves unfinished or newly risky.
```

**Rule of thumb for the "How it was verified" section:** "it compiles" is not
verification. "The mission flew it in SITL" is. "Nobody has run this" is an
acceptable and useful answer — write it down rather than leave it implied.

### Optional: a reminder hook

`scripts/hooks/pre-commit-changelog` warns (does not block) if you commit
changes to flight source without touching this file. Enable it with:

```bash
ln -sf ../../scripts/hooks/pre-commit-changelog .git/hooks/pre-commit
chmod +x scripts/hooks/pre-commit-changelog
```

Bypass for a genuinely minor commit with `git commit --no-verify`.

---

## Current status

**Can we fly this on the aircraft? No — not yet.** The software runs the
complete mission in simulation. Nothing in it has ever run on real hardware.

| Subsystem | State |
|---|---|
| Mission logic (the flight sequence) | Works end to end in simulation |
| Build and deployment | Works from a clean checkout |
| Link to the flight controller | Works in simulation; unverified against the actual Pixhawk |
| Camera / AprilTag detection | Works in simulation; needs the real camera calibrated |
| Obstacle avoidance | Works in simulation; the Mid-360 driver is now vendored and wired, but **has never seen a sensor** |
| Position (where the aircraft thinks it is) | The flight controller's GPS estimate, trusted completely. FAST-LIO2 is now in the stack as a second opinion but is **off by default and has never been run** |
| Winch | Runs the sequence in simulation; **never actuated in real life** |
| Gimbal | Commanded correctly; **never swept against a protractor** |

### Blocking a first flight

| # | What | Who |
|---|---|---|
| 1 | Fly the rewritten DELIVER sequence in simulation — the phase machine, the timeout, and the `retract_failed` path have never run | Software |
| 2 | Bench-test the winch with a load and measure the real timings | Hardware |
| 3 | Verify gimbal tilt direction and travel, props off, against a protractor | Hardware |
| 4 | Generate the ZED camera calibration file | Software/Hardware |
| 5 | Put the Mid-360 on the network and measure its mount — the driver is in, the two IP addresses and the mount transform are placeholders | Hardware |
| 6 | Apply and export the PX4 parameters, including a geofence for the test site | Hardware |
| 7 | Exercise a failsafe *during* a winch drop in simulation before trusting it with a real cable | Software |

The mid-air disarm risk that headed this list is **closed** — see the
2026-09-02 (late night) entry. A stalled descent now hands the last metre to
PX4 AUTO.LAND instead of disarming.

---

# Entries

## 2026-09-03 (later) — FAST-LIO2: the aircraft can now check where it thinks it is

**In one sentence:** FAST-LIO2, a lidar-inertial SLAM system, is now part of
the stack — it works out the aircraft's position from the lidar and the IMU
instead of taking the flight controller's word for it, and it is switched off
by default until we have flown it enough to know whether to believe it.

### What changed

**The estimator itself.**

- Vendored `fast_lio` (hku-mars FAST_LIO, `ROS2` branch, commit `a4743b0`)
  into `navigation-stack/DD_Nav_WS/dd_gazebo_ws/src/fast_lio`, alongside the
  Livox driver. `VENDORED.md` in that directory records the pin, the bundled
  `ikd-Tree`, and the five local changes made to get it building on ROS 2
  Jazzy. Four of the five are build-system fixes; the fifth makes its
  transform frames configurable, because this stack allows exactly one
  publisher per transform link and FAST-LIO's was hardcoded.

**Three new nodes in `vision_landing`.**

- `livox_pc2_to_custom` — converts `/livox/points` into the Livox message
  format FAST-LIO reads, on a new topic `/livox/lidar_custom`. This is a
  *branch* off the lidar cloud, not a link in it: `/livox/points` and
  everything feeding the obstacle costmap are byte-for-byte unchanged.
- `px4_imu_bridge` — publishes the flight controller's IMU as `/arc/imu`,
  converted from PX4's axis convention to ROS's. Used in simulation only; on
  the aircraft FAST-LIO uses the Mid-360's own built-in IMU.
- `lio_odom_bridge` — takes FAST-LIO's answer and compares it with the flight
  controller's, publishing the difference on `/arc/lio/correction_norm_m`.
  When enabled it also applies that difference to the `map` → `odom`
  transform, slowly and with limits (see below).

**New launch arguments on `delivery.launch.py`.**

- `lio:=true` — run the estimator. It publishes its answer and its
  disagreement with PX4, and moves nothing.
- `lio_tf:=true` — additionally let it correct the map. Requires `lio:=true`.
- `lio_config:=mid360_aircraft.yaml` — switch from the simulation
  configuration to the aircraft one. This single argument also selects the
  matching IMU source and sensor offsets, so they cannot disagree.

Parameters live in `vision_landing/config/fast_lio/`. Both files annotate
every value that differs from upstream and why.

### Why

Everything in the stack that draws a map — the obstacle costmap, both existing
SLAM views — currently trusts the flight controller's GPS-based position
completely. The two `drone_slam` nodes are not exceptions: they read the
position from PX4 and paint the lidar onto it, so they can show you a sensor
that is aimed wrong, but they can never show you a *position* that is wrong,
because they assume it.

That matters because GPS is least accurate exactly where this aircraft is
being asked to fly: low, near large buildings. If the position drifts, the
obstacle map drifts with it, and the check that stops the aircraft flying into
a building is evaluated against a map that is in the wrong place. Nothing in
the stack could previously detect this.

FAST-LIO2 is an independent second opinion. It fuses the raw 3D lidar points
with the IMU at every scan and solves for the aircraft's motion from the
geometry it sees. It is the first thing here capable of disagreeing with the
flight controller — and `/arc/lio/correction_norm_m` is that disagreement, in
metres, recorded in every flight bag.

**Why it does not simply take over.** The obvious move is to make FAST-LIO the
aircraft's position source. That was rejected. The flight controller flies on
its own estimate and will keep doing so regardless of what ROS believes, so a
second estimator does not replace it — it only adds a second opinion about
where the map goes. FAST-LIO therefore adjusts the *map*, and the flight
control loop is untouched. If the estimator dies mid-flight, the map stops
being adjusted and the stack behaves exactly as it does today.

**Why the correction is rate limited.** An occupancy map remembers where it
put things. Move the map suddenly and every obstacle recorded earlier jumps
relative to the aircraft at once — buildings smear, cleared space is re-marked,
and the "am I about to hit something" check runs against a map that just
teleported. So the correction is allowed to slide at 0.25 m/s and 2°/s, and a
correction that arrives as a *jump* is refused outright rather than followed
slowly. This is the same lesson as the setpoint rate limits earlier today: an
aircraft handed a step answers it violently, and the fix is to limit the
reference, not to soften the safety check.

Beyond 20 m of disagreement the bridge disengages permanently, freezes the map
where it was, and says so loudly. It does not re-engage on its own.

### Hardware impact

**One thing changed, and it is the mount.** The simulated Mid-360 is now
**inverted** — still 0.15 m below the airframe, but rolled 180° so it looks
*down* instead of up. The sensor's field of view is −7° to +52° about its own
plane, which mounted the obvious way up points the cone at the sky.

**The aircraft must be built to match.** Bolted the "right way up" under the
airframe, the real Mid-360 cannot see the ground in cruise either — at 15 m a
ground return inside its 40 m range needs 20.6° of depression and it has 7°.
That is survivable for "do not hit a building taller than me" and useless for
anything else. Mount it inverted.

The two numbers that must agree — `mount_z` and `mount_roll` in
`livox_mid360.launch.py`, and the sensor pose in `typhoon_h480.sdf.jinja` —
are cross-referenced in both files. Nothing checks them automatically, and a
mismatch puts every obstacle in the wrong place without complaining.

The rest of the Mid-360 work already on the list is unchanged: get the sensor
on the network, and measure the mount.

Two notes for when that happens:

1. **The mount measurement now matters more than it did.** The 0.15 m
   "under_drone" offset is still a number copied from the simulation model and
   never measured. It previously affected only where obstacles were drawn, to
   under one map cell. It now also feeds the estimator's sensor geometry. A
   wrong value does not fail — it biases the answer quietly. The number appears
   in three files, and they are cross-referenced to each other.
2. **The Mid-360's built-in IMU is now used.** No wiring or configuration
   change — it is already published by the driver on `/livox/imu`. It just
   means the sensor's IMU has to actually work, which nothing previously
   depended on.

### How it was verified

**Brought up in SITL on the pad. The plumbing works; the estimator does not,
and the reason is the simulated sensor rather than the code.**

Builds:

- `fast_lio` builds clean on ROS 2 Jazzy inside `arc-drone:jazzy`. Getting
  there took three build failures, all recorded in `VENDORED.md` — the
  significant one being that upstream pins C++14, which Jazzy's ROS libraries
  reject with an error that points at ROS rather than at the flag.
- Adding it required **no change to the Docker image**. The one dependency it
  appeared to need (`pcl_ros`) turned out to be unused by the source.

**Second run, full delivery flown, inverted mount + scene gate in place.** The
mission flew the complete 596 m out-and-back: transit at 15 m, 57 obstacle
replans, winch drop at Krach Lawn, return leg. `lio_tf` was false throughout,
so none of the below touched the aircraft.

- **Inverting the sensor transformed obstacle detection.** `flight_level_filter`
  went from keeping **0 of ~1540** returns to **3886 of 9713**, and the global
  costmap from **0 obstacle points to ~3685**. The cloud roughly sextupled,
  from ~1540 points to ~9400. Avoidance did not regress; it substantially
  improved, because the aircraft can now see anything that is not above it.
- **The scene gate discriminates.** On the open pad it read 0.0% of returns
  off the dominant plane and refused FAST-LIO outright. In transit it read
  0.45–0.98 (median 0.81 over 1102 scans) and opened. The metric was
  re-derived independently from a raw cloud and reproduced (80.7%).
- **FAST-LIO itself still failed.** It diverged to 42.0 m, the tripwire
  disengaged it, and it ended the flight emitting "No Effective Points!" — its
  map had drifted so far that incoming scans no longer associate with it. The
  published correction stayed at 0.00 m for all 5839 samples of the transit:
  the guards held everything at identity, which is safe and is also nothing.
- **It used 1.84 GB of RSS** by the end of the round trip. That alone
  disqualifies the current configuration from the companion computer.

The most likely cause is not yet established, but the strongest candidate is
the IMU. `px4_imu_bridge` measured **44 Hz**, against the 100 Hz FAST-LIO
wants — roughly four IMU samples per lidar frame, where the algorithm assumes
tens. FAST-LIO also processed only 5 of the 10 scans per second it was offered.

First run, aircraft stationary on the pad, upright mount, no gate:

- The whole chain connects. `livox_pc2_to_custom` converts ~1540 points per
  frame, `px4_imu_bridge` publishes, FAST-LIO initialises and produces
  odometry at 5 Hz and registered clouds at 4.5 Hz. All four RViz windows open,
  including the new FAST-LIO view.
- `lio_odom_bridge` anchored `map` → `lio_odom` at (−0.04, 0.00, 0.04) — i.e.
  the frame arithmetic lands on the identity when the aircraft is on the pad,
  which is the right answer.
- **The estimator then drifted 118 m in about 40 seconds without the aircraft
  moving.** FAST-LIO placed it at (−84, +83, 1.1) m from a pad it never left.
- **Every guard did its job.** Jump rejection refused seven relocalisation-sized
  steps; the divergence tripwire fired at 20.1 m, disengaged permanently, froze
  the correction at 7.0 m, and printed the correct diagnosis — "the lidar has
  no structure in view and the solution has drifted on IMU alone". Because the
  default is observer mode, no transform moved and the mission was unaffected.

**Why it drifted, and it is not the algorithm.** The simulated Mid-360 in
`typhoon_h480.sdf` has the sensor's real vertical field of view, −7° to +52° —
mostly *upward* — and it is mounted under the airframe pointing that way. The
consequences are geometric and were measured, not guessed:

- On the pad only **4 of its 32 vertical rings point downward at all**, so the
  entire scene is a flat ring of ground at 2.4 m radius. That constrains
  height, roll and pitch, and constrains position and heading not at all. 1440
  of 11520 rays return, which matches the ~1540 observed.
- **In transit at 15 m it is worse.** A ground return inside the sensor's 40 m
  range needs 20.6° of depression; the sensor has 7°, which puts the ground
  122 m away. In cruise it sees nothing except structures taller than the
  aircraft within 40 m.

That is the open-ground degeneracy the design notes predicted before any of it
ran — confirmed on the first attempt and worse than expected. It is what
motivated inverting the mount.

**Where this leaves FAST-LIO: not working.** Two real defects were found and
fixed, and the estimator still does not converge. What has been earned is the
scaffolding — the mount is right, the gate works, the guards are proven under
two genuine divergences, and the whole thing is measurable from a bag. What has
not been earned is a single second of trustworthy lidar-inertial odometry.

Still unverified: that the estimator converges at all in this stack, at any IMU
rate; that it holds over a delivery; that it fits in the companion computer's
memory.

### Risks still open

- **`lio_tf:=true` has still never been run**, and on this evidence must not be
  until the sensor question below is settled. The default is `lio:=false`, so
  the mission is bit-for-bit unchanged unless someone asks for it.
- **The blocking question is which way the Mid-360 actually points**, and it is
  a hardware question, not a simulation one. The simulated mount reproduces the
  sensor's true −7°/+52° field of view pointing upward from under the airframe.
  If the real aircraft is mounted the same way, then the real aircraft also
  cannot see the ground in cruise — which is fine for "do not hit a building
  taller than me", which is all the costmap has ever been asked to do, and
  fatally insufficient for a lidar-inertial estimator.

  Inverting the sensor (roll 180°, giving −52°/+7°) would put ground returns
  11.7 m away at transit height and give FAST-LIO a plane plus structures to
  solve on. **That change has not been made**: it alters what the obstacle
  costmap sees, and obstacle avoidance is the one part of this stack that
  currently works. It needs a decision and a re-verification of avoidance, not
  a quiet edit.
- **The guards are proven, the estimator is not.** Jump rejection, the scene
  gate, the divergence tripwire and observer mode all behaved correctly under
  two real divergences (118 m parked, 42 m in flight). That is the backstop
  working; it is not evidence that the thing it is backstopping works.
- **1.84 GB of RSS after one round trip.** The ikd-Tree map grows with distance
  flown and nothing prunes it. Even a converging estimator would need this
  bounded before it goes near the companion computer.
- **The IMU rate is the next thing to chase.** 44 Hz from
  `/fmu/out/sensor_combined` against the 100 Hz FAST-LIO expects, and FAST-LIO
  consuming only 5 of 10 scans per second. Neither is understood yet, and
  either could be sufficient to explain the divergence.
- **The two guards fight each other after open ground, and nothing fixes that
  yet.** When the scene gate reopens, the first correction legitimately arrives
  as a step — it is the drift accumulated while the gate was shut. The jump
  filter rejected exactly that during this flight (1.50 m), which means the
  correction can never recover after a degenerate stretch, and on this route
  that is most of it. The fix is to re-seed on gate reopen and let the slew
  limiter walk the map across; it is NOT in this commit, because it has never
  been flown.
- **The gate has never been seen to close at altitude.** It closed on the pad
  and stayed open for the whole transit, because this route has structure in
  view essentially throughout. Its behaviour over genuinely open ground at
  15 m — the case it exists for — is still untested.
- **Simulation cannot test the hardest part.** The simulated lidar captures a
  whole frame in one instant, so there is no motion blur to remove. The real
  Mid-360 sweeps for 100 ms, which at transit speed is 0.4 m of travel per
  scan, and correcting for that is most of what makes this work on a moving
  aircraft. That code path will first run on the real sensor.
- **Not wired into the Docker Compose services.** Running it currently means a
  manual `ros2 launch`. Deliberate: the compose files describe how the aircraft
  flies, and this has not earned a place there yet.
- **`lio_odom_bridge` assumes the lidar is mounted square** — no roll, pitch or
  yaw in the mount. That is true today and matches the launch file defaults,
  but a tilted mount would silently invalidate its sensor-offset arithmetic.

## 2026-09-03 — Why the aircraft kept falling out of the sky, and three gaps closed

**In one sentence:** four attempted flights on the long route all failed, and
the flight log says why — Nav2 alternated between two ways round a large
building, the mission followed each new plan instantly, and the resulting 180°
command reversals every second and a half drove the airframe into a tumble;
that is now fixed, along with an obstacle map that was smaller than the routes
we fly, a safety flag with no operator control, and a simulation that never
tested the lidar's mount.

### The map was smaller than the mission

The global costmap was 900 × 900 m centred on the pad — 450 m of reach. The
documented IM Gold → Krach Lawn delivery is at NED **north −96, east +588**, so
**139 m outside it**. With the new preflight check that refuses a delivery
outside the mapped area, that route stopped being flyable at all; without the
check it would have flown with the last 139 m unguarded and nothing logged.

Neither is acceptable, so the map is now **1400 × 1400 m at 0.75 m**, origin
(−700, −700). The size is derived rather than picked:

```
MAX_RANGE_M (docker/.env, the hardware fence)  500 m
detour / approach margin                     + 200 m
                                             = 700 m radius
```

**The costmap radius must be the largest of the operational numbers**, because
it is the only one that carries an obstacle guarantee. `docker/.env` now says
so next to the value, and names the third number (`max_range_m`'s 2000 m launch
default, the outer companion geofence) so all three are visible in one place.

**The resolution is 0.75 m, and that was not the first guess.** 1400 m at the
previous 0.5 m is 2800 × 2800 = 7.84 M cells. It was tried, flown, and this
stack cannot afford it — two independent failures, both measured in SITL:

- `planner_server` ran its loop at **2.4–7 Hz** against a desired 20, so a
  replan landed roughly every 6 s. With `path_stale_sec` at 5 s, every plan
  spent part of its life "stale" and the mission intermittently fell back to
  holding for a route it already had.
- Worse, and intermittent: on one start the **Nav2 lifecycle manager stalled at
  "Configuring planner_server"**. The configure took long enough that the
  `change_state` response was lost — `failed to send response … client will not
  receive response` — so the costmap was never activated. The mission then sat
  in preflight reporting that no costmap had arrived. It had not, and never
  would.

0.75 m over the same 1400 m is 1867 × 1867 = **3.48 M cells**, within a few
percent of the 900 m × 0.5 m map this stack is known to run, while keeping the
full 700 m of reach. **Coarsen, don't shrink** — shrinking silently gives back
range. 0.75 m cells are ample for a 0.6 m airframe going *around* buildings
tens of metres across at 15 m rather than threading gaps.

`inflation_radius` on the global costmap goes 0.65 m → **1.5 m** to match. At
0.65 m it would have been under a single 0.75 m cell, which is no inflation at
all — the planner would have been free to graze a building corner. 1.5 m is
also simply the clearance a 0.6 m airframe should be keeping.

Two related settings follow from the same measurement. `path_stale_sec` goes
5 s → **15 s**, which is not a weakening of the obstacle guarantee: the plan is
re-tested against the live costmap on every 20 Hz tick, so "stale" means
"computed a while ago", not "unchecked". And `expected_planner_frequency` goes
20 → 1.0, so Nav2's rate warning means something again instead of printing on
every cycle.

### The guarantee had no switch

`require_costmap_to_fly` became load-bearing in the previous entry — it now
gates preflight, not just in-flight movement — but it was a bare node parameter
with no way to set it from a launch command or the environment. It is now a
declared launch argument on both `delivery.launch.py` and
`landing_pipeline.launch.py`, and `REQUIRE_COSTMAP_TO_FLY` in `docker/.env`,
alongside `require_plan_to_transit` which was already plumbed that way.

Its description says what turning it off actually costs, because that is the
only reason anyone would look it up.

### The simulator now tests the mount

`gazebo_scan_bridge` stamped its cloud `base_link`. The real driver stamps
`livox_frame` and relies on a `base_link → livox_frame` transform to place the
returns. So the simulation folded the 0.15 m mount offset silently away — an
error smaller than one costmap cell, and therefore harmless — but it also meant
**nothing in simulation ever looked up that transform.** The mount is the
number most likely to be wrong on the real aircraft and its failure mode is
silent: obstacles land in the wrong place and the map merely looks "a bit off".

Now the bridge stamps `livox_frame` too, and `delivery.launch.py` publishes the
mount transform **unconditionally** — it describes the airframe, not the
driver, and both worlds need it. `livox_mid360.launch.py` gained `start_driver`
(false in SITL: transform only, no sensor) and `publish_mount_tf` (false for
the compose `lidar` service, since the mission service alongside it already
publishes the airframe geometry), so the transform has exactly one publisher in
every combination.

Both worlds now resolve the identical chain:

```
map -> odom -> base_footprint -> base_link -> livox_frame
```

### The flight recorder had stopped recording

Found while reviewing the working tree after the first flight: `git status`
held an untracked 21 MB `mission_bag/` in the repo root, and the launch log
held this, once, in a wall of other lines:

```
[ERROR] [ros2bag]: Output folder 'mission_bag' already exists.
[ERROR] [ros2-19]: process has died [pid 69, exit code 1, cmd 'ros2 bag record -o mission_bag ...']
```

`bag_dir` defaulted to `mission_bag`, a fixed name in the current directory.
`ros2 bag record -o` **refuses to start when the directory already exists**, so
the first flight recorded and **every flight after it silently did not**.
Measured on this session: three SITL flights, one recording. The two that would
have been most worth having — the ones that failed — are the two that were not
recorded.

The readiness review calls the bag "the only record of WHY the software did
something", which makes a recorder that stops after the first run worse than no
recorder, because nobody thinks to check. And the fixed name landed in the repo
root when launched the documented way, one `git add -A` from being committed.

`bag_dir` now defaults to a **timestamped** directory under `bags/` —
`bags/mission_20260903_034519` — which is both collision-proof and already
covered by `.gitignore`. `mission_bag*/` is added to `.gitignore` too, for the
ones already on disk.

### The occupancy grid was smaller than the flight

The grey square in the navigation RViz window is `drone_slam`'s occupancy grid
— not the Nav2 costmap, which cannot render here at all (the shader link
failure that `costmap_to_cloud` exists to work around). It was hard-coded to
`MAP_SIZE_M = 200.0`: **±100 m of the pad**, on deliveries of several hundred
metres. The aircraft spent five sixths of the flight off the edge of its own
map, which is precisely the view you do not want when that map is the check
that the lidar is mounted and aimed correctly.

It is now **1400 m at 0.75 m — deliberately the same extent and resolution as
the Nav2 global costmap**, so the two grids line up cell for cell in RViz and
one number describes the operating area. `map_size_m`, `map_resolution` and
`map_pub_hz` are ROS parameters now rather than module constants; editing a
constant to fly a longer mission is how the map ended up smaller than the
flight in the first place.

**The blocker to scaling it up was serialisation, not the grid.**
`msg.data = prob.flatten().tolist()` converts the grid to a Python list element
by element. Measured per publish on this machine:

| Grid | `.tolist()` | `array.array` |
|---|---|---|
| 1000² — 200 m @ 0.2 m (old) | 0.070 s | 0.006 s |
| 1866² — 1400 m @ 0.75 m (new) | 0.214 s | 0.023 s |
| 2800² — 1400 m @ 0.5 m | 0.657 s | 0.084 s |

At the old 2 Hz the *old* map already spent ~28% of a core there — which is the
"about 25% of one core" this node was previously credited with. Scaling up
naively would have been fatal: 1400 m at 0.5 m takes 1.0 s per publish against
a 0.5 s timer. Switched to `array.array` (which rclpy takes straight through)
and 1 Hz, a map **49× the area costs less than the old one did**.

The saved view was the other half of the problem: `nav.rviz` was
`TopDownOrtho` at `Scale: 11` — 100 m across an 1100 px window — so a larger
map would have changed nothing visible. Now `Scale: 1.4` centred on (300, −50):
786 m across, framing the pad, the route and the destination.

### Three RViz windows will fail a SITL flight on one machine

Flight 3 ended `EKF position invalid — failsafe landing`. That is not a stack
defect; the mission did exactly the right thing. It is the machine:

```
load average: 30.50   (16 cores)
%CPU   COMMAND
 332   rviz2
 224   rviz2
 166   rviz2
 132   gzserver
 128   px4
  94   slam_3d_node
  79   slam_node
```

**The three RViz windows were consuming more than seven cores between them**,
PX4 runs in lockstep with Gazebo, and the EKF starved until the local position
went invalid. The log already records the same failure mode for the 933 MB
campus mesh — "the simulation is starved enough that the GPS plugin never
delivers a fix" — and for `gzclient`, which costs about half the real-time
factor. RViz is worse than either.

Widening the costmap made this sharper: `costmap_to_cloud` now renders a
1866² grid rather than 1800², and RViz draws every cell of it.

**Validate with `rviz:=false` and `slam_3d:=false`, and open the windows to
look at things afterwards.** Watching the flight and flying the flight are not
free to do at the same time on one laptop.

### Stale comments corrected

`mission_controller.cpp` still described the global costmap as "a ROLLING
WINDOW centred on the vehicle" in the passage explaining why Nav2 goals are
clamped to a carrot. It has been fixed and persistent since the "(night)"
entry. The carrot stays, for reasons that turn out to have nothing to do with
map size — a nearer goal is far cheaper to plan over a 7.84 M-cell grid, and
re-issuing it is what re-checks the route against what the lidar has seen since
— so the code was right and only the reasoning printed beside it was wrong.

### Hardware impact

- **Costmap memory and bandwidth are now the open question on the Jetson.**
  ~7.8 MB per grid at 1 Hz, 7.84 M cells per layer. Measure it before the first
  hardware flight; the 0.75 m lever above is the answer if it is too much.
- **`MAX_RANGE_M` stays at 500 m** — deliberately conservative, and now
  comfortably inside the mapped area rather than unrelated to it. Raising it
  past 700 m without widening the costmap first will get the flight refused at
  preflight, with all the numbers in the message.
- Nothing to wire.

### The progress watchdog, and two flights spent getting it right

The no-progress watchdog aborts a leg the planner cannot solve. Flying the
596 m route showed both that its threshold was too tight **and** that an
attempt to fix it properly was worse than the problem.

**Flight 1 — `no progress for 90 s, closest approach still 181 m`.** The route
runs into the **France A. Córdova Recreational Sports Center: 108 × 197 m, 20 m
tall, squarely across the path**. Skirting nearly 200 m of wall does not reduce
the straight-line distance to the goal at all. The aircraft was working — it
found steps and took them, climbed to 25 m, was making its way around the north
end — and the watchdog failsafe-landed it.

**So progress was re-defined as distance remaining along the planned route**,
which should shrink however sideways the detour is. That is wrong here, and
**flight 2 proved it in the clearest possible way**: the aircraft flew from
595 m to 252 m from the goal in a steady, almost unbroken decline, with two
brief holds in nine minutes — and was failsafe-landed reading

```
no progress for 180 s — best route remaining still 30 m, 249 m from the goal
```

The plan this mission holds is a plan to the **carrot**, not to the
destination: `nav2_goal_max_range_m` caps it at 45 m. Route-remaining therefore
sits at 30–45 m for the entire leg and never improves, however well the flight
is going. The aircraft was fine; the metric was not.

**Reverted.** Progress is the best straight-line distance to the goal, as it
was, and `no_progress_sec` goes 90 → **300 s** — enough for the largest detour
this world contains at the ~1.7 m/s this airframe actually achieves against a
4 m/s command. The transit deadline (744 s on this route) remains the real
backstop for a stuck leg; this watchdog only stops the aircraft burning all of
it on something it will never solve.

The error message now prints both numbers — best approach *and* current
distance — because printing only one is what made flight 2's failure look
plausible for a moment.

### How it was verified

**The widened costmap allocates and publishes.** `planner_server` with these
parameters comes up clean and publishes `width: 2800, height: 2800,
resolution: 0.5, origin (−700, −700)`, no errors, RSS 97 MB.

**Flown in SITL, on the route this unblocked.** Full stack on the Purdue campus
world — PX4 SITL headless, DDS agent, host lidar bridge, mission container with
all three RViz windows, 2D and 3D mapping, bag recording — with the delivery set
to Krach Lawn, the 596 m route that the 450 m map had put out of bounds:

```
Delivery to (40.4280586, -86.9210457) — NED (-96.9, 587.6), 595 m away
STATE IDLE -> WARMUP -> TAKEOFF -> TRANSIT
```

**Preflight passed, including the new costmap-coverage gate** — the route that
was refused before this change is accepted and flown. The transit itself ran
595 → 252 m with two brief holds before the watchdog bug above ended it.

**The start-request expiry fired for real**, and correctly: on one attempt Nav2
had not published a costmap within 60 s of the start being pressed, and the
mission logged `Start request expired after 60 s without passing preflight —
publish /arc/mission/start again once the warnings above clear`. That is the
designed behaviour, but it means the runbook must wait for the costmap before
pressing start; the SITL runbook now blocks on the topic.

The simulator's transform chain resolves end to end: `map → livox_frame` reads
translation `(-0.005, -0.005, -0.115)` on the pad, which is the 0.15 m mount
below a `base_link` sitting 0.035 m up. **The mount is exercised in simulation
for the first time.** The flight-level filter behaves as designed at both
extremes — `kept 0 of 1537 returns (band 1.5..5.9 m)` on the ground, where the
hard floor keeps the ground plane out of the map, and `kept 135 of 506 returns
(band 12.4..20.9 m)` at transit altitude, where buildings mark.

### ROOT CAUSE FOUND: the aircraft was flown unstable by its own commands

**The estimator was innocent. So was the CPU, and so was the scenery.** Four
flights on the 596 m route failed; two on the watchdog bugs above, and two with
`EKF position invalid`. That second pair is now diagnosed, from PX4's own
flight log rather than from the ROS side, because the ULog carries
`vehicle_local_position_groundtruth` and the ROS bag does not.

**The evidence, in the order it settles the question.**

*1. PX4 was already complaining, in its own words:*

```
0:03:34 WARNING: [health_and_arming_checks] Preflight Fail: Attitude failure (roll)
0:03:42 WARNING: [mc_pos_control] invalid setpoints
0:03:42 WARNING: [mc_pos_control] Failsafe: blind land
0:03:42 WARNING: Failsafe activated: Autopilot disengaged, switching to Descend
```

An **attitude** failure, seconds *before* anything was said about position.

*2. The estimate was tracking the truth almost exactly until after the trouble
started.* Estimated roll/pitch against ground-truth roll/pitch:

| t (s) | estimated roll | truth roll | \|gyro\| °/s | truth \|v\| m/s |
|---|---|---|---|---|
| 196 | 2.5 | 2.5 | 15 | 3.76 |
| 199 | −32.1 | −32.0 | 198 | 1.47 |
| 202 | 40.6 | 39.0 | 213 | 1.35 |
| 206 | 54.7 | 54.9 | 293 | 1.82 |
| 213 | 48.5 | 49.9 | 100 | 0.42 |
| 214 | 71.3 | **86.6** | 132 | 1.47 |
| 217 | 0.2 | **85.2** | 174 | 2.93 |
| 221 | −7.1 | **−89.6** | 103 | 0.50 |

Agreement to within ~1.5° up to t≈213, then divergence. `xy_valid` drops at
t=221.2. **The estimate went wrong because the aircraft was lost, not the other
way round.**

*3. And the truth itself is the story.* Look down the truth column: the
airframe is swinging −32°, +39°, −24°, +55° with gyro rates to 293 °/s while
its velocity oscillates 3.7 → 0.4 m/s. Peak accelerometer magnitude 152 m/s²
against a 11.1 m/s² baseline. **The aircraft was thrashing from t≈198 and
tumbling past 87° of roll by t≈214.**

*4. It hit nothing.* A point-in-polygon test of the whole flight against all 26
real footprints — ground truth position, each sample's own altitude against
each building's own height — finds **zero** samples inside a building below its
roof. (An earlier pass used bounding boxes and wrongly concluded it had flown
into the Córdova centre. Bounding boxes are not footprints.)

*5. So what was commanding it?* The commanded position setpoint against actual
position, from the log:

```
t=199.0  sp N=-20.3  act N=-24.2   commanded 4 m NORTH
t=200.5  sp N=-24.4  act N=-20.2   commanded 4 m SOUTH   (reversed)
t=202.0  sp N=-18.3  act N=-22.2   NORTH                 (reversed)
t=204.2  sp N=-19.7  act N=-15.6   SOUTH                 (reversed)
t=205.8  sp N=-12.4  act N=-16.5   NORTH                 (reversed)
```

**The commanded direction reversed through 180° roughly every 1.5–2 s**, and
the position error sat pinned at **3.7–4.1 m continuously** for the whole leg.

**The mechanism.** Nav2, asked to route around a building 108 × 197 m, has two
reasonable answers — north about and south about — and alternated between them.
`fly_guided_leg` adopted each new plan unconditionally and commanded a point a
fixed 4 m along it. So every replan became a 4 m step position demand in the
opposite direction, against 3.5 m/s of existing momentum, with no velocity
feedforward to shape it. A multirotor answers that with full attitude
authority. Repeated at roughly 0.5 Hz, it pumped the attitude loop until the
airframe tumbled.

Two things made the alternation worse: every blocked tick set
`transit_goal_sent_ = false`, re-requesting a plan at 20 Hz when Nav2 could
answer about every 6 s; and the fixed 4 m carrot meant the aircraft was
*permanently* saturated, sitting at −25° of pitch, so each reversal started
from an already-extreme state rather than from trim.

### The fix

**1. The commanded direction is rate-limited** —
`bearing_rate_limit_deg_s`, default **45 °/s**. A 180° reroute becomes a 4 s
arc instead of a one-tick slam.

Applied **before** the obstacle checks, deliberately: steps 5 and 6 of
`fly_guided_leg` now validate the direction actually about to be commanded,
not the raw planner output. Smoothing a reference must never smooth it past a
safety check. The filter also advances on ticks where the aircraft is *not*
allowed to move, so a reversed plan cannot deadlock against a stale bearing —
by the time a block clears, the commanded direction is already part-way round.

**2. Forced replans are rate-limited** — `replan_min_interval_sec`, default
**2.0 s**, replacing the unconditional `transit_goal_sent_ = false` on every
blocked tick. Asking sixty times for each answer is what made the alternation
visible to the controller.

### Flying it found the second half

The bearing limiter worked, and measurably — the same metric run on the before
and after logs, over the whole flight:

| | flight 4 (before) | flight 5 (after) |
|---|---|---|
| commanded-direction ticks turning >120 °/s | 103 | **37** |
| max roll reached | 180° (inverted) | 86.1° |
| max gyro | 994 °/s | 617 °/s |

**And it still failed.** Because flight 5's trigger was a different
discontinuity, in the same family. From its log, the instant a hold ended:

```
t         spN    actN    err | roll  | yaw_sp
232.8    37.0    37.0   0.00 |  -0.1 |   96     holding
233.8    33.0    36.7   3.98 |  45.1 |  157     moving
```

**Two step demands in one tick.** The position error went 0 → 3.98 m, and the
commanded yaw jumped 61°. Roll answered with 45° immediately.

Both had the same shape of cause. A hold commands the aircraft's own position
and its own heading; a move commands a point 4 m ahead and the direction of
travel — which had gone on rotating throughout the hold, because the bearing
filter keeps running while blocked. Nothing connected the two references
across the transition.

**Fix, part two — two more limits, both on the reference, neither on the
safety checks.**

**A global commanded-yaw rate limit**, `yaw_rate_limit_deg_s`, default 60 °/s,
applied inside `publish_setpoint` *and* `publish_land_setpoint`. Those are the
only two places a setpoint leaves this node, which makes them the only two
places that can guarantee the invariant: **no setpoint the mission sends ever
steps the attitude reference.** 60 °/s is well above anything asked for
legitimately — SEARCH sweeps at 11.5 °/s — so it only ever bites a step.

**An asymmetric carrot ramp**, `carrot_ramp_m_s`, default 2.0 m/s. The lead
distance may only *grow* at that rate, reaching the full 4 m in two seconds; it
collapses to zero the instant a hold begins.

**The asymmetry is the safety-preserving part and is deliberate.** Ramping the
growth costs two seconds of gentler acceleration. Ramping the *collapse* would
mean the aircraft kept being commanded forward for a second after an obstacle
appeared — and stopping immediately is the entire purpose of a hold. Smooth on
the way up, instant on the way down.

**Deliberately NOT changed, and why.** The obvious further fix is velocity
feedforward: publish `TrajectorySetpoint.velocity` alongside the position and
set `OffboardControlMode.velocity = true` (the plumbing already exists —
`publish_offboard_mode(bool)` is there and used once, for the landing
descent). That would stop a 4 m position error demanding whatever
`MPC_XY_VEL_MAX` allows, and it is the standard way to fly a guided leg.

It is not in this change because the persistent 4 m saturation is the
*amplifier*, not the cause — it was present on every previous flight,
including the ones that completed — and because it changes the offboard
control mode for every state that moves the aircraft. One change aimed at the
demonstrated root cause, validated on its own, then that. This log already
records what happens when a control-path fix is made at speed without
understanding it.

### It flies

**Flight 6 completed the mission.** The first end-to-end delivery since the
DELIVER rewrite, on the 596 m route that ended the five flights before it:

```
IDLE -> WARMUP -> TAKEOFF -> TRANSIT -> DELIVER -> RETURN
     -> SEARCH -> GOTO_TAG -> LAND -> LANDED
Arrived at delivery point (1.96 m) — descending to winch altitude
Package delivered and winch stowed — RETURN to launch
Handing the last metre to PX4 AUTO.LAND (no touchdown detected in time).
```

The same metric across all three logs, read complete this time:

| | flight 4 | flight 5 | **flight 6** |
|---|---|---|---|
| commanded-direction ticks >120 °/s | 103 | 37 | **21** |
| max roll | **180°** (inverted) | 86.1° | **36.4°** |
| max gyro | 994 °/s | 617 °/s | **400 °/s** |
| mean setpoint error | 31.4 m | 18.3 m | **3.61 m** |

36.4° of roll is ordinary manoeuvring. The tumble is gone.

**Three things ran for the first time since they were written.**

*The winch phase machine*, which was the highest-severity finding of the code
review, executed its full sequence under real timing rather than completing one
tick after it began:

```
DELIVER: winch 'released',   phase 'retracting'
DELIVER: winch 'retracting', phase 'retracting'   (~14 s)
Package delivered and winch stowed — RETURN to launch
```

Note `winch 'released', phase 'retracting'` — the mission advanced its own
phase, sent `retract` exactly once, and the winch then caught up. The old code
would have flown home with the package on the hook about 50 ms after commanding
"lower".

*The per-leg state reset*, exercised on `RETURN -> SEARCH -> GOTO_TAG` — the
transition that used to inherit a stale `best_dist_` and failsafe-land on its
opening tick.

*The landing*, which retried twice on `Tag lost during descent` — the AprilTag
library reporting `fix_pose_ambiguities(): more than one new minimum found` as
the tag fills the frame — reacquired within 250 ms each time, descended 5.08 m
to 0.50 m, and handed the last metre to PX4 AUTO.LAND. That is the designed
finish with no rangefinder fitted, and the retry path behaved exactly as
specified.

### How this was diagnosed, for the next person

The ROS bag could not have answered this. `/fmu/out/vehicle_local_position_v1`
is the *estimate*, and the estimate is what went wrong — an analysis built on
it concluded, wrongly, that the aircraft had flown into a building. PX4's own
ULog carries `vehicle_local_position_groundtruth` and
`vehicle_attitude_groundtruth` alongside the estimate, which is what makes
"the aircraft moved" separable from "the estimate moved" in a single file.

They are in `PX4-Autopilot/build/px4_sitl_default/rootfs/log/<date>/*.ulg`,
readable with `pyulog` (`ulog_messages` for the text, `ULog(...)` for the
series). **Start there, not with the bag.**

### Risks still open

- **One completed flight is one completed flight.** Flight 6 flew the route
  end to end with no failsafe, but a single run is not a demonstration of
  reliability — the readiness gate asks for ten consecutive. The five failures
  before it were not identical, and two of them only appeared once the one
  before had been fixed.
- **The landing needed two retries** on tag loss during descent, and finished
  under PX4 AUTO.LAND rather than under mission control. That is the designed
  behaviour without a rangefinder, but the tag-tracking margin at 4-5 m is
  visibly thin and worth a look before anyone trusts it over a real pad.
- **Velocity feedforward is still missing**, so the aircraft continues to fly
  every guided leg at a saturated 4 m position error and around −25° of pitch.
  That is the amplifier described above. It is the next change, and it wants
  its own validation run.
- The costmap cost is measured on a laptop, not a Jetson.
- Everything the previous two entries left open is unchanged: the rewritten
  delivery sequence, the winch bench test, the gimbal sweep, the ZED
  calibration, the PX4 parameter set, and a Mid-360 that has never been
  connected to anything.
- **`DELIVER`, `SECURE_PAYLOAD` and the landing sequence were never reached**
  in any of the four flights. Everything downstream of TRANSIT remains
  unexercised since the rewrite.

---

## 2026-09-02 (overnight, later) — The lidar has a driver

**In one sentence:** the Livox Mid-360 now has a driver in the repo, a place in
the transform tree and a documented bring-up, so obstacle avoidance has a
sensor on the aircraft for the first time — everything that can be closed
without the hardware in hand is closed.

### What changed

**`livox_ros_driver2` is vendored**, at
`navigation-stack/DD_Nav_WS/dd_gazebo_ws/src/livox_ros_driver2/` — version
1.2.7, pinned by commit `4a1def92` (2026-07-31). Vendored rather than cloned at
build time for the same reason the AprilTag library is pinned to a tag: a field
laptop with no internet must still be able to build the flight stack, and this
is a flight dependency, not a convenience. It is 1.2 MB.

Version matters here. **1.2.6 is the release that added Ubuntu 24.04 and ROS 2
Jazzy support**, which is what `arc-drone:jazzy` runs; 1.2.7 is the current head
and adds a fix for "no point cloud published when all config items are
omitted". Pinned by commit because 1.2.7 is not tagged yet.

**`Livox-SDK2` is built into the image**, pinned to commit `08f523c9` of the
same day. The driver links `liblivox_lidar_sdk_shared.so` with a `REQUIRED`
`find_library`, so without it the workspace fails at CMake configure time.
Livox releases the two together — bump them together or not at all. `libapr1-dev`
was added alongside; the driver's CMake looks for it via `pkg-config`.

**The driver's own launch files are not used, and that is the important part
of this entry.** `livox_ros_driver2` ships two per sensor:

| | |
|---|---|
| `msg_MID360_launch.py` | publishes Livox's own `CustomMsg` |
| `rviz_MID360_launch.py` | publishes `PointCloud2` — and also starts RViz |

Nav2's obstacle layer reads `PointCloud2` only, and `flight_level_filter` reads
it with a `PointCloud2ConstIterator`. **Nothing in this stack can read a
`CustomMsg`, and the failure is silent:** the driver runs, the topic appears in
`ros2 topic list`, the costmap stays empty, and the mission refuses to transit
with "waiting for a Nav2 route". The compose file's placeholder command — written
before the driver existed — pointed at exactly that launch file.

So there is a new one: **`vision_landing/launch/livox_mid360.launch.py`**. It
sets `xfer_format: 0` (PointCloud2), remaps `/livox/lidar` to **`/livox/points`**
— the topic `gazebo_scan_bridge` publishes in SITL, so the flight code is
byte-identical between simulation and the aircraft — and publishes the sensor's
mount transform.

**The mount is a launch parameter, not a constant.** `base_link → livox_frame`
defaults to 0.15 m below the airframe, matching `sim/models/typhoon_h480.sdf`.
Nav2 derives its raytracing origin from the cloud's frame, so this transform is
what decides where every obstacle is placed.

**Configuration and documentation.**
`vision_landing/config/livox/MID360_config.json` holds the Livox network
config, with a `README.md` beside it explaining every field, and
`docker/README.md` gains a Mid-360 bring-up section with the five commands that
tell you whether it is working.

**Two local patches to upstream**, both recorded in the package's `VENDORED.md`:

- `package.xml` is committed. Upstream ships `package_ROS1.xml` and
  `package_ROS2.xml` and expects its `build.sh` to copy one into place; a plain
  `colcon build` over `src/` sees a directory with no manifest and **skips the
  package silently**. `build.sh` itself is deleted — it runs
  `rm -rf ../../build/ ../../install/`, which in a shared workspace would take
  every other package with it.
- `CMakeLists.txt` defaults `DISTRO_ROS` from `$ENV{ROS_DISTRO}`. Upstream
  expects it passed on the command line; unset, it silently takes the
  pre-Humble typesupport path and fails at link time with a missing `rosidl`
  symbol. Our entrypoint, the SITL runbook and hand builds are four places that
  would each have to remember the flag.

**Wiring.** The compose `lidar` profile now launches our file instead of the
placeholder, and `delivery.launch.py` gains `lidar:=true` for a single-launch
bench bring-up. Not both at once — two drivers cannot share the sensor's UDP
ports.

**One unrelated correction.** `cloud_to_scan.cpp`'s height-band comment still
said the Mid-360 "sits above the airframe". It was moved underneath on
2026-09-02. The band is measured from the sensor so the numbers did not change,
but the reasoning printed next to them was wrong.

### Why

The costmap had no observation source on the aircraft at all. Since the
2026-09-02 evening entry the mission refuses to transit without a plan rather
than degrading to a blind straight line, so the practical effect of a missing
lidar was that a hardware delivery would abort on the pad. Safe, and not a
delivery aircraft.

The `CustomMsg` trap is worth the space it takes above because it is the exact
shape of failure this project keeps hitting: a component that runs, reports
nothing wrong, publishes on the topic you expected, and delivers data nobody
downstream can read. The versioned PX4 topic names, the QoS mismatch in
perception, the calibration variable named two different things — same family.

### Hardware impact

**This is the entry for the hardware team.** Everything below needs the
aircraft.

- **The sensor is a wired Ethernet device on its own subnet, and two addresses
  have to agree.** The sensor's own IP is `192.168.1.1xx`, where `xx` is the
  **last two digits of the serial number** on the body. The Jetson needs a
  static address on that subnet — `192.168.1.5/24` in the shipped config —
  brought up on boot, or the driver starts before the interface does and binds
  to nothing. Both are in
  `vision_landing/config/livox/MID360_config.json` and **both shipped values
  are placeholders.**
- **The two failures look different.** A wrong host IP is loud — `bind failed`
  / `Init lds lidar fail!` — but **the node stays up publishing nothing at
  all**, so seeing it in `ros2 node list` proves nothing. A wrong sensor IP
  binds fine and is expected to be quiet. `ping` the sensor first, then
  `ros2 topic hz /livox/points`.
- **Measure the mount.** `mount_x/y/z` and `mount_roll/pitch/yaw` on
  `livox_mid360.launch.py` default to 0.15 m below `base_link`, taken from the
  simulated model and never measured on the real airframe. A wrong mount
  transform does not fail — it puts obstacles in the wrong place, which reads
  as the map being "slightly off" right up until the aircraft flies into
  something.
- **Record the firmware version.** The Mid-360 downloads page
  (<https://www.livoxtech.com/mid-360/downloads>) currently offers
  **v13.18.0244 (2025-04-11)**, flashed with Livox Viewer 2 over the same
  Ethernet link — nothing in this repo touches it. Write the version the
  aircraft actually runs into `config/px4/README.md` with the other
  hardware-side settings.
- **Expect nothing on the ground.** `flight_level_filter` has a hard floor
  1.5 m above the launch elevation, so at rest on the pad it is *supposed* to
  pass almost no returns through to the costmap. Lift the sensor or set
  `min_absolute_z:=0.0` before concluding it is broken.

### How it was verified

**The image rebuilds** with the `Livox-SDK2` layer, which compiles and installs
`liblivox_lidar_sdk_shared.so` into `/usr/local/lib`.

**The workspace builds: 7 packages, 0 failures.** `livox_ros_driver2` compiles
in 30 s under a plain `colcon build` with no `DISTRO_ROS` flag, which is what
the CMakeLists patch exists to make true — it logs
`DISTRO_ROS not set, using ROS_DISTRO='jazzy'` and takes the Jazzy typesupport
path. `apr` resolves. Its only stderr is upstream's own PCL policy warnings.

**The launch file starts and the transform is right.** Started with no sensor
attached:

```
[livox_mid360]: Livox Ros Driver2 Version: 1.2.7
[livox_mid360]: Config file : .../vision_landing/config/livox/MID360_config.json
config lidar type: 8
successfully parse base config, counts: 1
[base_link_to_livox]: from 'base_link' to 'livox_frame'
```

`tf2_echo base_link livox_frame` returns translation `(0, 0, -0.150)` and
identity rotation, as intended.

**And it found a documentation bug in this very entry.** With no sensor and no
`192.168.1.5` on the machine, the driver fails *loudly* —

```
bind failed
Failed to init livox lidar sdk.
[ERROR] [livox_mid360]: Init lds lidar fail!
```

— but **does not exit**. It stays in `ros2 node list` looking perfectly healthy
and creates no topics at all. The first draft of this entry and both READMEs
said a wrong IP fails "silently"; it does not, and the corrected text is above.
A wrong *sensor* IP, with a valid host IP, binds successfully and is expected to
be the quiet one — untested, because there is no sensor to point it at.

**No point has ever arrived on `/livox/points` from this driver.** The SDK never
initialised, so the node never created its publishers — which means the one
thing this smoke test could *not* check is the message type and the remap. That
`xfer_format: 0` yields `PointCloud2` on `livox/lidar` comes from reading
`lddc.cpp` (`CreatePublisher` and `InitPointcloud2MsgHeader`), not from
watching a topic. The costmap has still never been filled by anything but the
simulator.

This entry makes the software ready for the sensor. It does not demonstrate the
sensor working.

### Risks still open

- **Point volume is untested and is the likeliest surprise.** The simulated
  bridge delivers 500–700 returns per scan. A Mid-360 delivers **200,000 points
  per second** — around 20,000 per message at 10 Hz, roughly thirty times as
  many. `flight_level_filter` and `cloud_to_scan` are C++ and should cope;
  **`slam_3d_node` is Python and measured at ~25% of a core on simulated
  clouds, so it probably will not.** Treat `slam_3d:=true` as a diagnostic to
  switch on deliberately, and watch Nav2's raytracing cost on the Jetson before
  trusting the 12 m clearing range.
- The two IP addresses are placeholders and the mount transform is from a
  simulation model.
- `/livox/imu` is published and nothing consumes it. The Mid-360's IMU is a
  real asset for the estimator; wiring it in is a separate decision, not an
  oversight to fix quietly.
- Everything else on the blocking list is unchanged: the winch bench test, the
  gimbal sweep, the ZED calibration, the PX4 parameter set — and the rewritten
  delivery sequence still has not flown.

---

## 2026-09-02 (overnight) — Twelve review findings fixed, five in the mission state machine

**In one sentence:** a full review of the flight-readiness branch turned up
twelve real defects — including one that would have flown the package home
still on the hook — and all twelve are fixed.

### What changed

**The winch sequence no longer finishes before it starts.**
`mission_controller.cpp`'s DELIVER state drove the whole drop off
`/arc/winch/state`. That topic is *latched*, so on the tick after `"lower"` was
commanded it still read the preflight value `"stowed"` — which was also the
completion condition. The mission would set `delivered_`, transition to RETURN,
and fly home with the package on the hook and the spool paying out. A
`winch_bridge` restart mid-delivery re-armed the same trap, because its
constructor republishes `"stowed"`.

DELIVER now tracks its own phase (`WinchPhase::NOT_STARTED → LOWERING →
RELEASING → RETRACTING`) and consults the winch's reported state only for the
exit condition of the phase it is actually in. A phase is never re-entered, so
each command is also sent exactly once — previously `"release"` went out every
50 ms while waiting for the state to come back, and each one restarted
`WinchBridge::start()`, pushing the phase end further out every time.

**DELIVER has a deadline.** It was the one active state without one. A winch
that stops answering — `winch_bridge` dies, or reports `retract_failed` — used
to leave the aircraft in an unbounded hover over a customer's address until the
battery failsafe put it down wherever it happened to be. New
`deliver_timeout_sec` (default 120 s) covers the whole state. `retract_failed`
and `aborted` are now acted on immediately rather than waited out.

**Per-leg guard state is cleared in one place.** Every guard inside
`fly_guided_leg` is scoped to one leg, but it was being reset at individual call
sites, and two were missed:

- `best_dist_` / `best_dist_at_` survived a legitimate 126 s SEARCH into
  GOTO_TAG. The no-progress watchdog was then already expired — judged against
  a best distance from the *previous* leg — so the first tick of the new leg
  called `enter_failsafe_land("no_progress")` and the aircraft landed
  immediately after finding its tag.
- `blocked_since_` survived a TRANSIT that ended while holding (the arrival
  check runs before `fly_guided_leg`). The next leg's first hold read as
  "already blocked for 10 s" and commanded a 5 m escape climb on its first
  tick. On the run-in to the landing pad, that breaks the descent approach.

All of it now resets in `transition()`, which is the only way a leg can change.

**A start request is no longer thrown away before preflight runs.** The IDLE
state cleared `start_requested_` *before* calling `preflight_ok()`. An operator
who published a start a second too early — before the EKF called the position
valid, or before the latched `/arc/winch/state` arrived — got one throttled
warning and then permanent silence, because `preflight_ok()` is only reached
via the flag that had just been cleared. The request is now held until preflight
passes, and expires after `start_request_valid_sec` (default 60 s) so an
abandoned request cannot arm the aircraft minutes later.

**Obstacle checks no longer fail open past the edge of the map.** The global
costmap is a fixed 900 × 900 m square centred on the pad, while `max_range_m`
defaults to 2000 m. A delivery accepted at, say, 700 m was inside the range
fence and outside the map — and `cost_at()` returns −1 for "off the map" exactly
as it does for "unknown", which is deliberately *not* treated as an obstacle.
So `point_blocked`, `segment_blocked` and `path_ahead_clear` all silently
returned "clear" while `costmap_fresh()` kept passing, and the documented "never
fly into a detected obstacle" guarantee was gone with nothing logged.

New `costmap_covers()` distinguishes off-map from unknown. Preflight now refuses
a delivery point outside the mapped area (naming the map size and origin), and
`fly_guided_leg` refuses a step that would leave it.

**A TF dropout no longer marks the ground under the aircraft.**
`flight_level_filter` republished the raw, unfiltered cloud when
`lookupTransform` failed, on the reasoning that an empty cloud reads as "all
clear". That reasoning does not survive how the topic is wired: it is the
costmap's *marking-only* source (`livox_mark`), so silence leaves existing marks
standing and clearing continues from the raw cloud — while passthrough sends
every ground return straight to a marking source, turning the aircraft's own
cell lethal and reproducing the exact deadlock the node was written to prevent.
`passthrough_without_tf` now defaults to **false**, and both branches log loudly.

**Smaller fixes.**

- `costmap_to_cloud` sized its output to the whole grid before filtering — a
  ~52 MB allocate-and-zero every second on the 1800 × 1800 costmap, discarded
  on the next line. It now counts first and allocates exactly what it needs.
  `delivery.launch.py` also starts it under the same `rviz` condition as the
  RViz windows; it is a display aid and nothing in the flight path reads
  `/arc/costmap_cloud`.
- `flight_level_filter` was using `std::memcpy`, `std::vector` and `std::array`
  without including `<cstring>`, `<vector>` or `<array>`; it compiled only
  through transitive includes from `rclcpp`.
- `payload_bridge.cpp` was in the tree but in no build target and no launch
  file, so `/arc/payload/command` had no subscriber anywhere. It is superseded
  by `winch_bridge`, which drives the same hook servo — building both would put
  two nodes on one PX4 actuator set (both default to index 1). It is now
  documented as not-built in `CMakeLists.txt` alongside the other superseded
  prototypes, its unused `travel_time_sec` parameter is gone, and its header no
  longer describes a `require_release_confirm` parameter that never existed.
- `processes.py` had a hardcoded absolute path to one developer's home
  directory. It now takes `PX4_DIR` from the environment, else resolves
  `PX4-Autopilot` relative to its own location, else `~/PX4-Autopilot`.
- `arc_landing/landing_fsm_node.py`'s DESCEND branch checked `tag_visible` but
  not `tag_pose_body`, so a `target_visible` arriving before the first pose
  killed the timer callback with an `AttributeError`. It now guards both, as
  APPROACH already did. (This package is `COLCON_IGNORE`d and deprecated.)

### Why

The branch had nine commits of new flight logic on it and had never been read
end to end. Five of the twelve findings were in `mission_controller.cpp`, and
four of those five were the same class of bug: state that belongs to one leg or
one phase, not reset at the boundary. That is worth naming, because it is the
shape the next one will take too.

Two would have ruined a flight outright — the package flown home on the hook,
and the instant failsafe land after a long search. Two more were silent: the
obstacle checks passing "clear" beyond 450 m, and a start request vanishing
into nothing. Silent failures are the expensive kind on an aircraft, because
the first evidence is the outcome.

### Hardware impact

**None to wire, one thing to know.** `deliver_timeout_sec` (120 s) now bounds
the whole delivery hover, so the bench-measured winch timings must fit inside
it: `lower_sec` + `release_sec` + `retract_sec` plus the settle and the descent.
The current defaults total 28 s of winch, so there is generous margin — but if
the real winch turns out much slower than the bench estimate, raise the timeout
rather than discovering it as a failsafe over a customer's address.

Also: if a delivery is planned beyond ~450 m from the pad, the global costmap
in `nav2_params.yaml` must be widened first. Preflight will now refuse the
flight and say so rather than flying it unguarded.

### How it was verified

`vision_landing` builds clean under `colcon build --cmake-args
-DCMAKE_BUILD_TYPE=Release` in the `arc-drone:jazzy` container. Its unit tests
pass: 18 tests, 0 failures — though those cover `mission_math.hpp`, which this
change does not touch. The three Python files byte-compile, and `processes.py`'s
new resolver was checked to land on the repo's own `PX4-Autopilot`.

**Not flown.** None of this has been through SITL yet, and the DELIVER rewrite
in particular deserves a full delivery run before it is trusted — it is the
state that handles the package. The costmap-coverage preflight gate also means
a delivery mission will now wait for Nav2's costmap before arming, which
changes the start-up timing of the runbook sequence.

### Risks still open

- The DELIVER phase machine, the deliver timeout, the `retract_failed` path and
  the `aborted` path have all never run. `SECURE_PAYLOAD`, which two of them
  now enter, still has never run either.
- The new preflight costmap gate has never been exercised. If Nav2 is slow to
  publish its first costmap, the mission will sit in IDLE logging why — and
  with a 60 s request expiry, a start published too early will now need to be
  published again.
- `flight_level_filter` publishing nothing on a TF dropout is the right default
  by argument, not by measurement. Nobody has watched what the costmap does
  through an actual TF gap in flight.
- Everything on the blocking list is unchanged: the Livox driver, the winch
  bench test, the gimbal sweep, the ZED calibration, and the PX4 parameter set.

---

## 2026-09-02 (late night) — The mid-air disarm risk is gone

**In one sentence:** the landing logic can no longer cut the motors while the
aircraft is still in the air.

### What changed

The previous touchdown logic disarmed when the aircraft was below 0.30 m and
had stopped descending for 3 s. That height came from the flight controller's
estimate relative to the takeoff point, which drifts with barometric pressure
over a long mission. Drift it 2 m low, hold steady for 3 s during a descent,
and the motors stopped 2 m up.

**A stalled descent now hands the last metre to PX4's own AUTO.LAND instead of
disarming.** The two failure modes are not symmetric, and that asymmetry is the
whole argument:

| | |
|---|---|
| disarm too early | the aircraft falls — damage, possibly worse |
| hand off too early | PX4 flies it down from wherever it actually is — a controlled descent |

**Direct disarming is now reserved for signals that cannot be wrong about being
on the ground:** PX4's own land detector, PX4's ground-contact stages, or a real
range sensor. `VehicleLocalPosition` already carries `dist_bottom` and
`dist_bottom_valid`, so when a rangefinder is fitted the mission uses a
*measurement* rather than an estimate, and disarming directly is sound.

The 90 s backstop became `land_handoff_sec` at 25 s, which is roughly the time
a descent from the lowest search level takes. Landings no longer sit on the pad
for a minute and a half waiting for a flag PX4 will not raise while we are
still commanding it.

### Hardware impact

**A downward rangefinder is now worth fitting, and the software will use it the
moment it appears.** Without one the aircraft finishes every landing under PX4
AUTO.LAND — safe, and slightly less precise over the pad. With one, the mission
keeps control all the way to touchdown. No configuration needed beyond PX4
seeing the sensor: the code checks `dist_bottom_valid` and the sensor bitfield
and switches automatically.

### How it was verified

Flown in simulation, search-and-land, on the campus world. SITL has no
rangefinder (`dist_bottom_valid: False`), so this run exercised exactly the
branch that used to disarm:

```
TAKEOFF -> SEARCH -> GOTO_TAG -> LAND -> FAILSAFE_LAND -> LANDED
Handing the last metre to PX4 AUTO.LAND (no touchdown detected in time).
This is the normal finish when no range sensor is fitted.
```

**Zero self-disarms.** The aircraft finished upright on the pad at
(0.0, 0.0, 0.22 m), roll 0.001, pitch −0.002.

### Risks still open

- The rangefinder branch has never run, because no simulated rangefinder
  exists. When one is fitted, test that path before trusting it.
- Everything else on the blocking list is unchanged: the Livox driver, the
  winch bench test, the gimbal sweep, the ZED calibration, and the PX4
  parameter set. All need hardware.
- `SECURE_PAYLOAD` still has never run.

---

## 2026-09-02 (night) — Lidar moved under the aircraft, and four bugs that hid behind each other

**In one sentence:** the simulated lidar now sits where it really sits — under
the airframe — and getting a clean flight out of that exposed a transposed
campus, a costmap that erased itself, a costmap that could not forget, and a
delivery point inside a building.

### What changed

**The lidar is mounted `under_drone`.** It was 0.15 m above the airframe, which
kept the body out of the beam and was therefore easier and wrong. It now sits
below, matching the aircraft. Measured consequences: about **three times as many
returns per scan** (500–700 against 130), because rays the body used to block
now pass freely — and the airframe occludes a cone directly above the sensor.
The modified model is vendored at `sim/models/` so it survives a clone.

**The campus was transposed, and every earlier avoidance result was worthless.**
Gazebo's world frame here is ENU: **+x is east, +y is north**. The world
generator wrote every building footprint as *(north, east)*, mirroring the
whole campus across the diagonal. The aircraft flew the correct GPS route —
that is computed in the flight controller's own frame and owes nothing to the
world file — so the buildings simply sat where the aircraft never went. Flights
looked clean and proved nothing.

Verified empirically this time rather than assumed: moving the model to
gazebo x = +60 moved its **longitude** 60 m east and left latitude unchanged.

**This corrects an earlier claim in this log.** The 2026-09-02 entry below says
the aircraft "routed around" the tall buildings. It did not. They were never on
its path.

**The obstacle filter was erasing the map it was supposed to build.** It
published its output already transformed into the map frame, which seemed
tidy. Nav2 derives the *sensor origin* from a cloud's frame, and raytraces from
there to every return to clear the cells between — so labelled `map`, every
scan raytraced from the landing pad and wiped everything along the way,
including the building ahead. **The aircraft flew into the Turf Recreation
Center at 15 m and crashed.** The filter now emits points in the sensor frame;
only the height *test* uses the map frame.

**Marking and clearing now use different data.** With both taken from the
flight-level cloud, the aircraft correctly refused to fly into a 20 m building,
climbed to 39 m to clear it — and stayed blocked, because at 39 m the building
was outside the flight-level band, so there were no returns left to raytrace
through cells that were already marked. It sat there until the leg timed out.
Marking now comes from the flight-level cloud, clearing from the raw cloud.

**The map is persistent, and no longer erases itself.** The global costmap was
a 120 m rolling window that discarded everything more than 60 m behind the
aircraft — on a 400 m mission the map dissolved as it flew. It is now fixed:
900 × 900 m at 0.5 m, centred on the pad. Clearing range was also cut from 35 m
to 12 m, which is enough to clear a stale mark the aircraft is sitting on
without wiping the wider map.

**A delivery point inside a building now fails honestly.** The point chosen for
the last route was inside the Morgan J. Burke Aquatic Center. Nav2 cannot plan
to a lethal cell, so it returned nothing, the safety check refused it, the
aircraft backed off, replanned, approached, and repeated — oscillating between
8 and 58 m from the goal for the whole leg. It now says so plainly and gives up
after 20 s, because that is not fixable in the air.

**A no-progress watchdog** ends a leg where the best distance achieved has not
improved for 90 s, rather than circling until the deadline.

**The transit deadline margin went from 3.5× to 5×**, because a run that had to
climb from 15 m to 39 m and back finished 31 m short.

**New route**, chosen with the destination checked this time: 402 m out, 25 m
clear of every building, with two 20 m buildings across the straight line. A
route around them was confirmed to exist before flying — 426 m against 402 m
straight, a 24 m detour.

### Hardware impact

- **This is the mount you are building.** The `under_drone` position triples
  the return count and blinds the aircraft directly above itself. If the Mid-360
  ends up anywhere else, say so — the simulated sensor should match, or the
  simulation stops being evidence.
- **The occlusion cone matters for the winch.** A sensor under the airframe is
  looking at the same space the payload descends through. Worth checking, on the
  bench, what the lidar returns while the winch is paying out.
- No parameter changes on the flight controller.

### How it was verified

**Partly.** Each fix was verified against the failure it addressed:

- Transposition: measured with a known model displacement, then the regenerated
  world was confirmed to put three named buildings across the straight line.
- Sensor-frame fix: the aircraft went from flying into the Turf Recreation
  Center to **detecting it and refusing** — `HOLDING — planned route is blocked
  at NED (-75.1, 291.6)`.
- Marking/clearing split: the aircraft climbed to 39 m, the map cleared, and it
  **flew over** the building it had been stuck against.
- Destination check and goal choice: the unreachable goal was confirmed inside
  the Aquatic Center; the replacement was confirmed clear, with a route.

**The final configuration has NOT been flown end to end.** All sixteen items on
the pre-run checklist pass, but the run itself was stopped before it started.
Nothing here should be read as a completed mission.

### Risks still open

- The full mission has not flown since the lidar moved. Everything above is a
  fix for an observed failure, not a demonstration of a working delivery.
- Climb-to-escape has now fired in anger and worked once. The reactive case — a
  route going stale because something new appears — still has not.
- The mid-air disarm risk in the touchdown logic is **unchanged and remains the
  top item** blocking a first flight.
- The persistent 900 × 900 m costmap at 0.5 m is 3.24 M cells published at 1 Hz.
  It has not been run long enough to know whether that is a problem on the
  Jetson.

---

## 2026-09-02 (late) — Real Purdue buildings, and Nav2 on every leg

**In one sentence:** the simulated world is now the actual Purdue campus —
real building footprints at their real positions, taken from the OSM data
already in this repo — and every part of the mission that moves the aircraft
sideways is routed and obstacle-checked, not just the two long legs.

### The world is now the real campus

26 real buildings within 480 m of the pad, each one an actual OSM footprint
converted from latitude/longitude into the local frame, extruded to height and
given collision geometry. Hillenbrand Hall, Shreve Hall, the Aquatic Center,
the Turf Recreation Center and the rest are where they actually are.

This is what "use the Purdue map" can mean in practice. The 933 MB campus mesh
still cannot be used — as collision geometry it never finished loading, and
even as scenery it starved the simulation until GPS stopped converging.
Footprints give the real layout, real positions and real collision at a cost
the simulator can carry.

**Building heights are the weak part and are labelled as such.** OSM gives an
explicit height for very few of these. Where there is one it is used; where
there is a storey count it is multiplied by 3.5 m; otherwise the building is
assumed to be 20 m. Every model in the world file records which of the three it
got. The assumed ones are a guess.

### Nav2 now guards every lateral leg

Previously the outbound and return legs were routed and checked while the
shorter moves — closing the last stretch onto the delivery address, and the run
in to the landing pad — were direct setpoints with no obstacle check at all.
They are short, but "short" is not "safe".

All of them now go through the same guarded path. One deliberate distinction:
below `min_route_m` (8 m) the aircraft moves directly rather than asking for a
route, because a global planner cannot usefully plan a two-metre nudge and
demanding one at the moment of landing would add a failure mode exactly where
the mission can least afford it. **The obstacle check still applies to those
short moves.** So the guarantee — never fly into something the lidar has seen —
holds across the whole mission, while the planner is used where a plan means
something.

### Hardware impact

**None directly, but the world is now a much better rehearsal.** Flying the
mission against real building positions is the closest thing to a site survey
available before the aircraft exists. If a route looks wrong here — cutting a
corner it should not, or refusing a gap it should take — that is worth knowing
before anyone stands in a field with a transmitter.

### How it was verified

Flown end to end on the real campus: pad at the Intramural Gold Fields to a
point 392 m away, and back. **No failsafe.**

```
TRANSIT: 391 m to go ... 4 m to go   ->  DELIVER
RETURN:  377 m to go ... 24 m to go  ->  SEARCH -> LAND
Touchdown: commanding descent at 0.20 m/s but stopped descending
at 0.03 m for 3.1 s — disarmed. Mission complete.
```

The only hold in the entire flight was "waiting for a Nav2 route" before the
first plan arrived. Nothing was ever blocked in flight. The altitude filter
tracked the aircraft throughout (band 12.6-21.1 m at transit altitude).

### An honest note on "three buildings"

The straight line from the pad to this delivery point passes through three real
buildings: the **Morgan J. Burke Aquatic Center**, the **Turf Recreation
Exercise Center**, and the **Hull "All-American" Marching Band Complex**.

Two of them are taller than the 15 m transit altitude and must be flown
around. The third, Hull, is recorded in OSM as two storeys — 7 m — so the
aircraft correctly flies **over** it rather than around. That is the
altitude-aware costmap doing its job, and it is the right behaviour, but it
means this run demonstrates *around two, over one* rather than around three.

No straight route from this pad crosses three buildings that are all taller
than the transit altitude, given the heights OSM provides. To have all three
flown around, either lower the transit altitude below 7 m or correct Hull's
height if 7 m is wrong for a rehearsal hall with high ceilings — it may well
be.

### Risks still open

- Nothing was blocked in flight, so the route around the two tall buildings was
  planned rather than reacted to. The reactive path — a route going stale
  because something new appeared — still has not been exercised.
- The climb-to-escape behaviour has still never triggered.
- The mid-air disarm risk in the touchdown logic is **unchanged and remains the
  top item** blocking a first flight.

---

## 2026-09-02 (evening) — Obstacle safety on both legs, altitude-aware costmap, new Purdue route

**In one sentence:** the return flight is now routed around obstacles like the
outbound one was, the aircraft refuses to move unless the route ahead is
confirmed clear, the obstacle map finally understands altitude, and the mission
flies from the Intramural Gold Fields to Krach Lawn.

### The route

Both coordinates are the centroids of the named features in the OSM extract
already in this repo (`Path_Planning/worlds/map-2.osm`) — not recalled from
memory.

| Point | Latitude | Longitude |
|---|---|---|
| Purdue Gold Intramural Fields — pad, takeoff and landing | **40.4289204** | **-86.9279914** |
| Krach Lawn — delivery | **40.4280586** | **-86.9210457** |

596 m apart: 589 m east, 96 m south.

### What changed

**The return leg was flying blind.** The outbound leg routed around obstacles;
the return was a plain straight line home — same ground, same altitude, no
route and no checking. Half of every delivery was unguarded. Both legs now go
through one shared piece of code, so they cannot drift apart again.

**The aircraft now refuses to move unless the way ahead is confirmed clear.**
Previously a plan was made once and then trusted. A plan is a statement about
the past: it was clear when the planner was asked. Three checks now run every
tick, and any of them failing means it holds position instead of flying:

- there is a current obstacle map (no map means no guarantee, so no flying);
- a route exists (never having had one means the lidar, the planner or the map
  is missing, and a straight line would be flown blind for the whole leg);
- **the route is still clear right now**, re-tested against the live map, plus
  the specific next step about to be commanded.

**The obstacle map now understands altitude — this is the big one.** Nav2's map
is 2D: it flattened every lidar return between 2 m and 40 m onto one plane, so
a building was marked whether the aircraft flew at 5 m or at 35 m. Two bad
consequences, both seen: the aircraft detoured around a low roof it would clear
by 7 m, and — worse — it ended up *inside* the flat footprint of a building
taller than its own altitude, where every direction including its own position
read as blocked, so it hovered until the leg timed out.

A new filter now cuts the lidar to a band around the aircraft's own height
before the map ever sees it. A building that rises through the flight level
still marks, because its wall is right there; a low roof underneath does not.
The 2D map now means "things at my level", which is a question a 2D map can
answer honestly.

**A blocked hold can no longer deadlock.** If a hold does not clear within
10 seconds the aircraft climbs to get above the obstruction. Height is the one
direction reliably clearer than where you are, and with an altitude-aware map
climbing genuinely shows the route clearing.

**The transit deadline now scales with distance.** A fixed 300 s cannot serve
both a 100 m hop and a 600 m leg. The 596 m route timed out 58 m short having
flown 520 m perfectly well — the leg was fine, the clock was wrong.

### Two bugs found the hard way

**The transform tree was flat, and it silently broke the new filter.** The
aircraft's height was never published: `base_footprint` is the ground
projection so its height is correctly zero, and the link from there to the
aircraft body was a *static identity*, so nothing in the system could answer
"how high am I?". Nothing minded while every consumer was 2D. The moment the
new filter asked, it got "on the ground, always", kept only returns near ground
level, left the map empty — and the new refuse-to-fly-unconfirmed rule
correctly grounded the mission. **Observed as: the aircraft lifted off and
landed again in the same place.** The height is now published where it belongs.

This also means **the 3D map described in the previous entry was wrong** — it
placed every scan at ground level regardless of altitude. It looked plausible
and was not. Corrected by the same fix.

**A launch setting was accepted and silently ignored.** `transit_timeout_sec`
was passed on the command line to a launch file that never declared it. No
error, no warning, and the default was used — which is what caused the timeout
above. The mission timings are now declared properly.

### Hardware impact

- **The altitude filter is what makes obstacle avoidance sane on a real
  aircraft.** Its band is 2.5 m below to 6 m above the aircraft, asymmetric on
  purpose: missing something you are about to climb into is the failure that
  matters, clearing something below you by a few metres is the case that should
  be allowed. Those numbers are parameters — revisit them against the real
  airframe and the Mid-360's actual mounting angle.
- **The filter has a hard floor 1.5 m above the launch elevation**, so the
  ground does not fill the map during takeoff and landing. If the pad is not
  level with the surrounding terrain, that floor needs checking.
- **`MAX_RANGE_M` in `docker/.env` is 500 m** and this route is 596 m —
  preflight will refuse it as out of range, correctly. Raise it deliberately
  for a long route rather than by reflex.
- Nothing here changes the outstanding hardware tasks.

### How it was verified

Full mission flown in simulation, IM Gold Fields to Krach Lawn and back, with
2D and 3D mapping and three RViz windows. **596 m out, 596 m back, no
failsafe:**

```
TRANSIT: 595 m to go ... 14 m to go  ->  DELIVER
RETURN:  585 m to go ...  18 m to go  ->  SEARCH -> LAND
Touchdown: commanding descent at 0.20 m/s but stopped descending
at 0.13 m for 3.0 s — disarmed. Mission complete.
```

- **Altitude filter tracked the aircraft**: on the ground, band 1.5-6.1 m,
  0 of 1273 returns kept — no ground clutter in the map. At transit altitude,
  band 12.5-21.0 m, 18-74 of ~130 returns kept.
- **Obstacle avoidance exercised**: the world has a 25 m building and an 8 m
  building placed dead on the straight line between the two points. The
  aircraft completed both legs without ever holding for an obstacle, which
  means it routed around the tall one. The single hold in the whole flight was
  "waiting for a Nav2 route" at the very start, before the first plan arrived.
- **Prune fix confirmed**: the 3D map reached its 400,000-voxel cap and 88,748
  of those read as occupied — 22%. Under the previous policy, which kept the
  highest values, the cap would have been filled almost entirely with occupied
  voxels because free space is stored as negative. Most of the map surviving as
  confident *free* space is the fix working.

### Risks still open

- The 8 m building should now be flown straight over rather than detoured
  around. The flight completed without holding either way, so this run shows
  the aircraft was not blocked by it — but it does not prove no detour
  happened. Comparing the flown track against the straight line would settle
  it.
- The mid-air disarm risk in the touchdown logic is **unchanged and still the
  top item** blocking a first flight.
- The escape-by-climbing behaviour has not been exercised: nothing blocked long
  enough to trigger it this run.

---

## 2026-09-02 (later) — 3D mapping, in its own window

**In one sentence:** the aircraft now also builds a **3D** map of everything
the lidar sees, shown in a separate RViz window, so buildings appear as shapes
with height rather than as outlines on a flat plan.

### What changed

- **`slam_3d:=true`** is a new option on the delivery mission. It starts a 3D
  voxel mapping node **and its own RViz window**. The existing 2D map stays in
  the navigation window; the two are answering different questions and sharing
  one window would only bury the route view.
- The 3D node reads the **full lidar cloud**, not the flattened 2D scan — the
  height is the entire point. Occupied voxels are published as a point cloud
  coloured by height (blue low, yellow high), which RViz draws natively with
  no extra plugin needed.
- It uses the **full aircraft attitude** via the transform tree, so roll and
  pitch during a banked turn are handled correctly. The 2D node only uses
  heading, which is fine for a flat slice and would smear a 3D map.
- Like the 2D node, it **publishes no transforms** and does not correct the
  aircraft's position. It is an observer.

### Why

Looking at a live point cloud tells you almost nothing — it is a few thousand
dots that vanish each frame. An accumulated 3D map is something you can judge
at a glance: the buildings should look like buildings, standing in the right
place, with vertical walls. That makes it the most direct check we have that
the lidar is mounted, aimed and scaled correctly. Those faults are miserable to
diagnose from live data and obvious in a map — walls lean, or the same wall
appears twice, or the ground curves away.

**A word on the name.** This is *mapping with known poses*, not full SLAM.
Real SLAM recognises somewhere it has been before and retro-fits the whole
trajectory so the map stays consistent. This does not: it takes the flight
controller's position as truth and inherits any drift in it. That is still
useful — it just is not a substitute for position when GPS is poor, and it
should not be described as one.

### Hardware impact

**None yet — but this is the tool you will want the day the Livox is mounted.**
Once the real lidar publishes, the same node runs unchanged; it reads the same
channel from the real driver as from the simulator. The acceptance test for a
newly mounted lidar is: fly a slow lap of somewhere you know, and see whether
the walls come out where the walls actually are, vertical, and once each.

### How it was verified

Full mission flown in simulation with both 2D and 3D mapping active and three
RViz windows open. Completed end to end, no failsafe:

```
Touchdown: commanding descent at 0.20 m/s but stopped descending
at 0.10 m for 3.0 s — disarmed. Mission complete.
```

The 3D map grew to about 118,000 occupied voxels over 835 lidar scans. Zero
channel-compatibility warnings and one dropped scan (the first, before the
transform tree was up). The three transform warnings in the log are Nav2
waiting for the position chain during startup, 36 seconds before the mission
began — they also appear without any mapping running, so they are not caused
by this.

### Risks still open

- **A bug this flight exposed, fixed afterwards and not yet re-flown.** The map
  has a voxel cap; on reaching it the node was discarding the *lowest* values
  first, which sounds sensible and is wrong — empty space is stored as negative
  values, so it was throwing away everything it knew to be clear. Free-space
  erasing would stop working from that point on and the map could only ever
  grow. It now discards the least *certain* voxels instead, keeping what it is
  sure about in either direction. Verified to build and start; not re-flown.
- The node is Python and does real work per scan. It kept up here at about 25%
  of one core, but on the Jetson, alongside everything else, it may not. Treat
  it as a diagnostic to switch on when you want it, not something to leave
  running on every flight.
- Nothing here changes flight readiness. Both mapping nodes are observers.

---

## 2026-09-02 — SLAM runs alongside the mission, as an observer

**In one sentence:** the aircraft now builds a 2D map of its surroundings from
the lidar while it flies the delivery, and the whole mission was re-flown in
simulation with mapping active.

### What changed

- **`slam:=true`** is a new option on the delivery mission. It starts the
  `drone_slam` mapping node, which accumulates an occupancy map of everything
  the lidar sees and publishes the aircraft's trajectory. Both now appear in
  the navigation RViz window.
- **New `cloud_to_scan` node.** The lidar is 3D and this mapping is 2D, so the
  cloud has to be flattened into a horizontal slice. It takes the nearest
  return in each direction within a height band around the sensor — nearest
  because for obstacle mapping a wall at 5 m matters more than the ground at
  30 m, and a band because otherwise the ground and the sky both collapse into
  the slice and the map fills with walls that are not there. It reads
  `/livox/points`, which is what the real Livox driver publishes **and** what
  the simulator bridge publishes, so the same node serves the aircraft and the
  simulator.
- **SLAM does not publish transforms here, deliberately.** The mission already
  publishes the full position chain from the flight controller's estimate. If
  the mapping node also published it, two things would fight over the same
  transform and one frame would end up with two different parents — an invalid
  setup that stops route planning entirely. Run standalone it still publishes
  them, because then nothing else does.
- **Two bugs found and fixed in `drone_slam` while wiring it up:**
  - It listened on a flight-controller channel name that does not exist on this
    firmware, so it would have run happily and never seen the aircraft move.
    Same class of mistake as the one corrected yesterday.
  - Its map was published in a mode RViz could not receive. The viewer
    connected, reported nothing wrong in the window, and displayed an empty
    map. The map is now latched, so a viewer that connects late immediately
    gets the current map.
- `drone_slam` is now part of the container build. It was never being compiled.

### Why

Two reasons. The obvious one is that a map of what the aircraft actually saw is
far easier to judge than a scrolling list of lidar readings — you can look at
it and say "that is the building, and it is in the right place". The second is
that it is a cross-check on the lidar and the position estimate together: if
the map comes out smeared or doubled, something is wrong with one of them, and
that is much better found in simulation than in the air.

### Hardware impact

**None yet, but this is the payoff for the Livox driver.** Once the real lidar
is publishing, this same mapping runs on the aircraft with no changes — the
flattening node reads the same channel from the real driver as from the
simulator. The map it produces is the clearest way to check the lidar is
mounted, aimed and scaled correctly: fly a slow lap of a known space and see
whether the walls come out where the walls are.

### How it was verified

Full mission re-flown in simulation with mapping active, on the Purdue campus
world with a 108 m delivery. Completed end to end with `failsafe=none`:

```
takeoff → transit → deliver → return → search → land → disarm
```

The map grew from 1,339 to about 2,900 mapped cells over the flight as the
aircraft covered the route and the two buildings. Checked specifically for the
failure this could have caused: **zero** transform conflict warnings and
**zero** channel-compatibility warnings for the whole flight, and the position
chain still resolved correctly with mapping running.

### Risks still open

- Everything in "Blocking a first flight" is unchanged. Mapping is an observer;
  it does not make the aircraft any more ready to fly.
- The map is built from the flight controller's own position estimate, not
  corrected by the mapping itself. It will inherit any drift in that estimate
  rather than correct for it. Fine for checking the lidar; not a substitute for
  position when GPS is poor.

---

## 2026-09-01 (evening) — First full mission flown in simulation

**In one sentence:** the complete delivery mission ran start to finish for the
first time — take off, fly to an address avoiding obstacles, lower the package
on the winch, fly home, find the landing pad by camera, land on it, shut down —
and getting there exposed five real defects, all now fixed.

The full sequence that ran:

```
takeoff → fly to the delivery address (routing around buildings)
        → hover and lower the package → release → reel the cable back in
        → fly home → search for the landing pad → centre over it
        → descend → touch down → disarm
```

No failsafe. The aircraft completed the mission under its own mission logic.

### What changed

**A correction to yesterday's work.** Earlier in the day I changed which
flight-controller data channels the software listens to, based on reading the
PX4 source. That was wrong. Reading the *running* flight controller showed the
original settings were correct — PX4 adds a version number to some channel
names and not others, and the mix that looked like a mistake was actually
right. Reverted, with the evidence recorded so nobody repeats it.

**The obstacle-avoidance planner could never produce a route.** The map the
planner works from is a 120 m box that moves with the aircraft, so it only
reaches 60 m in any direction. We were handing it a delivery address 108 m
away, which it rejected every single time. The software now aims at a point
45 m ahead along the route and re-aims as the aircraft advances — like
following a carrot — so the delivery distance is no longer limited by the map
size.

**The gimbal feedback I added yesterday was actively harmful.** The simulated
gimbal reports its angle while completely ignoring commands: told to point 45°
down, it kept reporting 5° up. Yesterday's change trusted that report, decided
every camera sighting was taken while the gimbal was still moving, threw them
all away, and the aircraft searched for the landing pad until it gave up. The
software now ignores gimbal feedback until the gimbal has demonstrably obeyed
at least one command — and only a properly tilted command counts, because a
gimbal stuck level would otherwise "prove" itself against a level command.

**One message definition was out of date and silently dropping data.** The
gimbal attitude message in our copy was missing three fields the flight
controller actually sends. The result is nasty: the connection looks healthy,
the channel appears in every listing, and every single message is thrown away.
Fixed. A full comparison found 63 of 227 message definitions differ from the
firmware, but only this one is used by the mission — the rest are written up in
`px4_msgs/PX4_SYNC.md` as a trap for whoever adds the next one.

**Touchdown was never detected.** Every landing sat on the pad for 90 seconds
with the software still thinking it was airborne, then gave up and handed the
aircraft to the flight controller's own landing mode. Cause: while our software
is actively commanding the aircraft, PX4 suppresses *every* stage of its own
landing detection — it will not tell us we have landed until we stop asking it
to fly. The software now works it out itself: if it is commanding a descent,
is close to the pad, and is no longer going down, it is on the ground.
Landing now completes in about 3 seconds instead of 90.

**Simulation worlds are now kept in the repo** (`navigation-stack/sim/worlds/`).
They previously lived only inside the PX4 folder, which is not in version
control, so a fresh checkout lost them.

- `purdue_campus.world` — the delivery mission world. Real West Lafayette
  coordinates, an AprilTag landing pad, and two buildings across the flight
  path so obstacle avoidance has something to avoid.
- `apriltag_landing.world` — the simpler search-and-land world. Its physics
  settings were wrong and are now fixed (see below).

**Two simulation problems worth knowing about**, both written up in
`navigation-stack/sim/worlds/README.md`:

- The old world used a coarse physics timestep. That made the simulated
  accelerometer read about −8.3 m/s² instead of −9.81 while the aircraft sat
  still, which the flight controller's position estimator read as the aircraft
  accelerating downwards. It refused to arm, reporting "High Accelerometer
  Bias". Fixed by using PX4's standard physics settings.
- The 933 MB Purdue campus 3D model **cannot be used**. As collision geometry
  the simulator spent 11 minutes at full CPU without finishing loading it. Even
  as scenery only, it slows things down enough that GPS never locks and the
  aircraft cannot arm. The campus world therefore keeps the real *location* and
  real obstacles, but not the 3D scenery.

### Hardware impact

- **Gimbal:** the software now cross-checks the gimbal's reported angle against
  what it commanded. When you do the props-off gimbal sweep, watch the log for
  `Gimbal feedback verified` — if it never appears, the gimbal is not reporting
  its position back through the flight controller and the software will fall
  back to assuming it obeyed. That is survivable, but it means a wiring or
  parameter error in the gimbal will not be caught automatically.
- **Landing:** the aircraft now decides for itself that it has touched down.
  **Read the open risk below before the first flight** — this is the one change
  from these two days that could damage the aircraft.
- **Delivery distance** is no longer capped by the planner's map size.

### How it was verified

Ran end to end in simulation, four times, on the Purdue campus world with a
108 m delivery, headless, with both visualisation windows and flight recording
active. Final run completed with no failsafe:

```
Touchdown: commanding descent at 0.20 m/s but stopped descending
at 0.03 m for 3.0 s — disarmed. Mission complete.
```

Also verified: the message-definition fix eliminated the data-dropping errors
(393 errors per minute → 0), and the gimbal fix restored pad detection.

### Risks still open

- **The touchdown logic can disarm the aircraft in mid-air.** It disarms when
  the aircraft is below 0.30 m and descending slower than 0.05 m/s for 3
  seconds. Height is measured by the flight controller relative to the takeoff
  point, and that estimate drifts with barometric pressure over a long flight.
  If it drifts about 2 m low and the aircraft holds steady for 3 seconds during
  a descent, **the software will cut the motors while it is still 2 m up.**
  Only possible during the final landing descent, but a 2 m drop damages the
  aircraft. Before flying: either cross-check against a downward-facing
  rangefinder, or remove this logic and instead shorten the 90-second handoff
  to PX4's own landing mode to about 10 seconds.
- **`SECURE_PAYLOAD` has still never run.** This is the safety behaviour that
  reels in or drops the package before an emergency landing. Zero occurrences
  across every simulation run — the most safety-critical path added yesterday
  is completely untested. Test it by aborting the mission during the winch drop
  in simulation.
- The obstacle-avoidance planner keeps replanning to a stale destination after
  the mission ends. Harmless noise, not yet cleaned up.

---

## 2026-09-01 (daytime) — Readiness audit, then made deployable and fail-safe

**In one sentence:** audited the whole stack against the question "could this
fly tomorrow?", found six things that stopped it running at all and six safety
gaps, fixed the software ones, and put the deployment tooling into version
control for the first time.

### What changed

**The container that runs the mission could not find the mission.** The startup
script looked for the code in a folder layout this repo does not use, gave up
silently, and instead loaded a build from July that contained the *old,
retired* flight software and none of the current mission. Anyone running the
documented startup command got a crash loop. Fixed, and the container now
refuses to start with one clear error rather than restarting forever.

**None of the deployment tooling was in version control.** The container
definitions, the `Makefile`, the delivery mission launch file, the winch
driver, the gimbal driver and the visualisation configs existed only on one
laptop. There was also no record anywhere of the flight controller's own
settings. Committed, along with a first written-down parameter set in
`config/px4/`.

**The delivery mission was not what the aircraft actually launched.** The
deployment ran the search-and-land pipeline, which does not start the winch and
takes no delivery address. Running `make up-hw` on the aircraft would have
produced a search-and-land, not a delivery. Fixed.

**The camera calibration was silently ignored.** The settings file named the
variable `ZED_CALIB_FILE`; the software read `CALIB_FILE`. They never matched,
so the calibration was always empty. Without it the camera can see the landing
pad but cannot work out where it is, so the aircraft searches, finds nothing,
and lands on the failsafe. Renamed, and the camera software now refuses to
start without a calibration rather than running blind.

**Obstacle avoidance had no sensor on the aircraft** and the software would
quietly fly straight lines anyway. The mission now refuses to start a delivery
leg if it has never received a route, instead of flying blind. A slot for the
real lidar driver was added to the deployment.

**Six safety behaviours added or fixed:**

1. **Failsafe during a winch drop no longer lands on the cable.** Previously an
   emergency during the drop handed the aircraft to automatic landing with the
   package half-lowered — descending onto its own cable. It now holds position
   and reels in first; if it cannot, it opens the hook and drops the load
   before descending. A true emergency (position lost, battery critical)
   releases immediately, because there may be no position estimate left to hold
   station with.
2. **Distance and altitude limits are now checked continuously in flight**, not
   just once before takeoff.
3. **The aircraft now reports what it is doing** on a dedicated channel, and
   every test flight is recorded. Previously the only insight was console text
   scrolling past.
4. **The gimbal angle is measured rather than assumed** (see the evening entry —
   this needed rework).
5. **The camera refuses to start without calibration.**
6. **The winch can now accept a limit switch** that overrides its internal
   timer, if one is fitted.

**Tests.** The two calculations that decide *where the aircraft physically
ends up* — converting a camera sighting into a map position, and converting a
GPS address into a local position — now have 17 unit tests. These are the
functions that fail by putting the aircraft in the wrong place rather than by
crashing, which makes a real flight a very expensive way to find a sign error.

**Eight smaller fixes,** including a copy-paste error in the rotation-rate
output, a retry counter that never reset, a planner that would stop replanning
forever if a request went unanswered, and a camera feed that would spin a CPU
core at 100% instead of reconnecting when the video stream dropped.

### Hardware impact

- **`config/px4/arc_delivery.params` is new and is for you.** It lists every
  flight controller setting the software depends on: gimbal routing, which AUX
  outputs drive the winch spool and the release hook, the data link settings,
  the geofence, and what happens if the onboard computer stops talking. Values
  marked `TUNE` are placeholders that must be measured on the actual aircraft.
  **This is a checklist, not a file to blindly load onto a new airframe** — it
  deliberately contains no PID gains or sensor calibration.
- **`config/px4/README.md` lists the bench tests these settings imply:** the
  gimbal sweep, the winch cycle with a load, and checking the hook opens and
  closes the way the software thinks it does.
- **The geofence matters.** The software's own distance and altitude limits
  only stop it *commanding* the aircraft. If the onboard computer locks up,
  they do nothing. The flight controller's `GF_*` settings are the layer that
  keeps working. Set both, and set the flight controller's slightly wider so
  the software gives up first.
- **Every test flight is now recorded** to `bags/`. Please keep these — they are
  the only record of *why* the software did something.

### How it was verified

Clean build from scratch inside the container (all five software packages,
7 min 31 s), 18 tests passing, all 14 processes starting, and the failure paths
exercised deliberately: the camera exits with a clear error when the
calibration is missing, and the status channel reports correctly.

**Not verified at this point:** no simulated flight had been run yet. That
happened the same evening — see the entry above, which found five more defects.

### Risks still open

Everything in the "Blocking a first flight" table at the top of this file.

---

## 2026-08-31 — Moved to a direct flight controller link, and designed the delivery mission

**In one sentence:** replaced the old communication layer between the onboard
computer and the flight controller with a direct one, packaged everything to
run in containers, and designed the winch-based delivery mission.

### What changed

**MAVROS was dropped.** The onboard computer used to talk to the flight
controller through a translation layer called MAVROS. It now talks directly
using the flight controller's native channels. Fewer moving parts, lower
latency, and one less thing that can silently reinterpret a command. The old
MAVROS-based landing logic in `arc_landing/` is retired.

**One mission stack.** `vision_landing` became the single flight program.
Several older, half-finished programs were left on disk but deliberately not
built, because each of them would have competed with the real mission for
control of the aircraft.

**Containerised deployment.** Three containers sharing one image: the flight
controller link, the mission, and (on the aircraft) the camera. This exists
because the Jetson onboard computer ships with an older operating system than
the software targets; containers bridge that gap without changing the code.

**The delivery mission was designed and written:**

- The aircraft **never lands at the customer.** It hovers and lowers the package
  on a winch, then reels the cable back in. The rotors stay well clear of people
  and property, and no marker is needed at the address.
- It **only lands at home**, on an AprilTag marker on the charging pad, using
  the camera to centre itself precisely.
- New software: the winch driver, the gimbal driver, the obstacle-avoidance
  route planner bridge, and the delivery launch configuration.

**Search pattern.** If the landing pad is not immediately visible, the aircraft
turns a full circle at 5 m, then 3 m, then 2 m, then 1 m, tilting the camera
further down at each level, until it sees the marker.

**The pilot always wins.** The moment the pilot takes manual control, the
software goes permanently passive. It never fights the pilot for the aircraft.

### Hardware impact

- **Flight controller wiring:** the data link runs over a wired serial port to
  the Jetson. The `UXRCE_DDS_CFG` setting must match the port used.
- **Gimbal:** the software sends an angle to the flight controller, which
  outputs a PWM signal on an AUX pin to the gimbal controller board. That board
  does its own stabilisation; the flight controller only supplies the desired
  angle. The AUX pin must be wired to the board's pitch input and its travel
  calibrated.
- **Winch:** two AUX outputs — one drives the spool motor, one drives the
  release hook.
- **Camera:** the ZED camera's video is encoded and sent to the mission software
  on the Jetson.

### How it was verified

Simulation of the search-and-land portion only. The delivery mission — transit,
winch drop, return — **was designed but never run**, in simulation or otherwise.
None of this work was committed to version control at the time; that happened
the following day.

### Risks still open

At the time: everything. This entry is written retrospectively; the audit the
next day is what actually established the state of it.

---

# Glossary

Terms used above, in plain language.

| Term | What it means |
|---|---|
| **AprilTag** | A high-contrast printed square pattern, a bit like a chunky QR code. The camera can work out exactly how far away it is and at what angle. Used as the landing pad marker. |
| **Costmap** | A map the route planner works from, where each square is marked free, occupied or near-something-occupied. Built live from the lidar. |
| **DDS / uXRCE-DDS** | The messaging system the onboard computer and flight controller use to talk. "The link". |
| **EKF** | The flight controller's position estimator. It fuses GPS, barometer, compass and accelerometers into one best guess of where the aircraft is and how fast it is moving. If it is unhappy, the aircraft will not arm. |
| **Failsafe** | An automatic safety response — for example, landing when the battery gets low. |
| **Gazebo** | The 3D physics simulator. Simulates the airframe, the world and the sensors. |
| **Gimbal** | The motorised camera mount that keeps the camera pointed where we want regardless of how the aircraft tilts. |
| **Headless** | Running the simulator with no 3D window. Faster, and the window is not needed. |
| **Nav2** | The route-planning software that finds a path around obstacles. In our setup it only *plans*; our own software does the flying. |
| **Offboard mode** | The flight controller mode where it takes movement commands from the onboard computer instead of from the pilot or a preloaded mission. |
| **PX4** | The flight controller firmware — the software running on the Pixhawk. |
| **RViz** | The visualisation window showing the aircraft, its planned route, the lidar returns and the camera's view of the landing pad. |
| **ROS 2** | The framework the onboard software is built on. Programs are "nodes" that exchange messages on named "topics". |
| **rosbag** | A recording of every message during a flight. Our black box. |
| **SITL** | "Software In The Loop" — the real flight controller firmware running on a laptop against a simulated aircraft, instead of on real hardware. |
| **Topic** | A named channel that messages are published to and read from, e.g. the aircraft's battery state. |
| **Lockstep** | The simulator and flight controller stepping forward together, so a slow computer makes the simulation run slower rather than producing wrong physics. |
