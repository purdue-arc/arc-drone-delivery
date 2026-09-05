# Simulation models

Vendored here because `navigation-stack/PX4-Autopilot/` is gitignored, so
anything edited only inside PX4's own `models/` directory is lost on the next
clone.

**Both the `.sdf` and the `.sdf.jinja` are kept, and the `.jinja` is the one
that matters.** PX4's build regenerates `typhoon_h480.sdf` from the template on
every `make px4_sitl`, and it has overwrite protection: if the `.sdf` has been
hand-edited it does not quietly regenerate, it **fails the build** with

```
ERROR: generation would overwrite changes to `typhoon_h480.sdf`.
Changes should only be made to the template file
```

Editing only the `.sdf` therefore either loses the change or breaks the build,
depending on timing. This bit us on 2026-09-04. Edit the `.jinja`, and keep the
`.sdf` in step with it.

## Installing

```bash
D=navigation-stack/PX4-Autopilot/Tools/simulation/gazebo-classic/sitl_gazebo-classic/models/typhoon_h480
cp navigation-stack/sim/models/typhoon_h480.sdf.jinja "$D/"
cp navigation-stack/sim/models/typhoon_h480.sdf       "$D/"
```

If the build still refuses, delete `$D/typhoon_h480.sdf` and let PX4 regenerate
it from the template.

## What is modified from stock

A **Livox Mid-360 stand-in** was added as a `<sensor type="ray">` on
`base_link`: 360 horizontal samples, 32 vertical, covering the Mid-360's real
vertical field of view of −7° to +52°, 0.35–40 m, 10 Hz.

It is mounted **under_drone and INVERTED** —
`<pose>0 0 -0.15 3.14159265 0 0</pose>`. Position is where the sensor sits on
the aircraft: 0.15 m below the airframe, not 0.12 m above it, which kept the
airframe out of the beam and was therefore easier, and wrong.

### Why it is inverted, and why the real aircraft must be too

The Mid-360's cone is −7° to +52° about its own plane — **mostly upward**. Hung
under the airframe the right way up, it points at the sky. Measured on
2026-09-04:

| | |
|---|---|
| On the pad | Only **4 of its 32 vertical rings** point downward at all, so the entire scene is a ring of ground at 2.4 m radius. 1440 of 11520 rays return. |
| At 15 m | A ground return inside the 40 m range needs **20.6° of depression; the sensor has 7°**. Ground is 122 m away. It sees nothing but structures *taller* than the aircraft within 40 m. |

That is survivable for "do not hit a building taller than me" — all the costmap
was ever asked for — and it leaves a lidar-inertial estimator with no geometry
to solve on at any point in the mission.

Rolled π the cone points down (−52° to +7°) and the ground is 11.7 m away at
transit height. **Obstacle detection improved sharply as a side effect:**
`flight_level_filter` went from keeping **0** returns to ~3900 of 9713, and the
global costmap from **0** obstacle points to ~3700.

`mount_z` and `mount_roll` in `vision_landing/launch/livox_mid360.launch.py`
must match this pose, and so must the sensor as actually bolted to the
aircraft. Nothing checks that automatically, and a mismatch puts every obstacle
in the wrong place without complaining.

### Other consequences of the belly mount

- The body and legs sit inside part of the beam and **occlude** it. The
  sensor's own `min_range` and the bridge's discard the self-returns, but the
  occlusion is real.
- Return counts scale with what the cone can actually reach: ~1500 per scan
  pointing up, ~9400 pointing down.

Nothing else in the model differs from stock PX4.
