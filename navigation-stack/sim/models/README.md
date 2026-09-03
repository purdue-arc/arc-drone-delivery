# Simulation models

Vendored here because `navigation-stack/PX4-Autopilot/` is gitignored, so
anything edited only inside PX4's own `models/` directory is lost on the next
clone.

## Installing

```bash
cp navigation-stack/sim/models/typhoon_h480.sdf \
   navigation-stack/PX4-Autopilot/Tools/simulation/gazebo-classic/sitl_gazebo-classic/models/typhoon_h480/
```

## What is modified from stock

A **Livox Mid-360 stand-in** was added as a `<sensor type="ray">` on
`base_link`: 360 horizontal samples, 32 vertical, covering the Mid-360's real
vertical field of view of −7° to +52°, 0.35–40 m, 10 Hz.

It is mounted **under_drone** — `<pose>0 0 -0.15 0 0 0</pose>`, below the
airframe — because that is where the sensor sits on the aircraft. It used to be
0.12 m above the body, which kept the airframe out of the beam and was
therefore easier, and wrong.

The consequences of the real mount are worth knowing and are the reason to
simulate it:

- Roughly **three times as many returns per scan** (about 500–700 against 130
  from the top mount), because rays the airframe used to block now pass
  freely outward and down.
- The body and legs sit inside the upper part of the beam and **occlude** it.
  The sensor's own `min_range` and the bridge's discard the self-returns, but
  the occlusion is real and the aircraft is blind in a cone directly above
  itself.

Nothing else in the model differs from stock PX4.
