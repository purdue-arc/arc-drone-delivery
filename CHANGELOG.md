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
| Obstacle avoidance | Works in simulation; **no driver for the real lidar yet** |
| Winch | Runs the sequence in simulation; **never actuated in real life** |
| Gimbal | Commanded correctly; **never swept against a protractor** |

### Blocking a first flight

| # | What | Who |
|---|---|---|
| 1 | Resolve the mid-air disarm risk in the landing logic (see 2026-09-01 entry, "Risks still open") | Software |
| 2 | Bench-test the winch with a load and measure the real timings | Hardware |
| 3 | Verify gimbal tilt direction and travel, props off, against a protractor | Hardware |
| 4 | Generate the ZED camera calibration file | Software/Hardware |
| 5 | Add a driver for the Livox lidar so obstacle avoidance has a sensor | Software |
| 6 | Apply and export the PX4 parameters, including a geofence for the test site | Hardware |
| 7 | Exercise a failsafe *during* a winch drop in simulation before trusting it with a real cable | Software |

---

# Entries

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
