# SITL worlds

These live here because `navigation-stack/PX4-Autopilot/` is gitignored (it is
a ~4 GB upstream tree — see `navigation-stack/PX4-AUTOPILOT.md`). Anything
placed only in PX4's own `worlds/` directory is lost on the next clone.

## Installing them

```bash
cp navigation-stack/sim/worlds/*.world \
   navigation-stack/PX4-Autopilot/Tools/simulation/gazebo-classic/sitl_gazebo-classic/worlds/
```

## The worlds

| File | Use |
|---|---|
| `purdue_campus.world` | The delivery mission. Campus geodetic origin, AprilTag landing pad, two box obstacles across the transit corridor. |
| `apriltag_landing.world` | Search-and-land only. Bare ground plane plus the tag. |

```bash
# Full delivery mission
PX4_SITL_WORLD=purdue_campus make px4_sitl gazebo-classic_typhoon_h480

# Search and land only
PX4_SITL_WORLD=apriltag_landing PX4_HOME_ALT=5 \
  make px4_sitl gazebo-classic_typhoon_h480
```

Run Gazebo headless (no `gzclient`) — the GUI is not needed and costs frames
that the physics step needs:

```bash
HEADLESS=1 PX4_SITL_WORLD=purdue_campus make px4_sitl gazebo-classic_typhoon_h480
```

## Two things that will bite you if you edit these

**1. Do not coarsen the `<physics>` block.**

`apriltag_landing.world` used to specify `max_step_size 0.01` at 100 Hz with no
ODE solver tuning. At that step the contact solver under-resolves the airframe
resting on its legs, and the simulated accelerometer reads about
-8.3 m/s² instead of -9.81 while the vehicle is stationary. EKF2 reads the
1.5 m/s² shortfall as real downward acceleration, diverges, and the vehicle
never passes preflight:

```
WARN [health_and_arming_checks] Preflight Fail: High Accelerometer Bias
```

Both worlds now carry PX4's stock block (0.004 s step, 250 Hz, full ODE solver
settings). Verified: accelerometer centred on -9.8, zero accel-bias failures.

**2. Do not add the Purdue campus mesh.**

`Path_Planning/worlds/Purdue_map.world` loads `Purdue_map.dae` (933 MB) as both
visual and collision geometry. Measured on a 16-core / 14 GB machine:

- **as collision** — gzserver pegged one core for 11 minutes building the ODE
  trimesh BVH without finishing, at 2.9 GB RSS and climbing. Even built, ODE
  trimesh collision at that size cannot step in real time, and PX4 runs in
  lockstep with the simulator.
- **as visual only** — the world loads and steps at RTF 0.83, but the GPS
  plugin never delivers a fix. EKF2 reports a horizontal accuracy in the
  kilometres, `local_position_invalid` and `global_position_invalid` stay true,
  and the aircraft never arms. Removing the mesh and changing nothing else
  makes position valid within ~60 s of sim time.

`purdue_campus.world` therefore keeps the campus *location* (geodetic origin at
the pad, so the delivery waypoint is a real West Lafayette coordinate) and
real obstacles, without the mesh. To bring the campus geometry back, decimate
it first — order 10⁴ triangles for collision — and re-verify that GPS still
converges before flying anything against it.

## Delivery waypoint used with `purdue_campus`

About 108 m out on a bearing of ~22°, with both boxes across the straight line:

```
delivery_lat:=40.4245982  delivery_lon:=-86.9207278
```
