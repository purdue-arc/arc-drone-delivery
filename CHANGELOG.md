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
