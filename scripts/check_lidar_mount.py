#!/usr/bin/env python3
"""Check that the lidar mount pose agrees everywhere it is written down.

The Mid-360 mount is recorded in three places:

  navigation-stack/sim/models/typhoon_h480.sdf.jinja   (the template, the source)
  navigation-stack/sim/models/typhoon_h480.sdf         (generated from it)
  .../vision_landing/launch/livox_mid360.launch.py     (mount_* defaults, the TF)

The launch file says of them: "This MUST match the sensor pose in
typhoon_h480.sdf ... Nothing checks that; a mismatch puts every obstacle in
the wrong place and says nothing." This is the thing that checks it.

Exit 0 and print OK when all three agree within TOLERANCE; otherwise print a
table of mismatches and exit 1. Exit 2 if a file cannot be read or parsed.

Usage:
  python3 scripts/check_lidar_mount.py
"""

import ast
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MODELS = REPO / 'navigation-stack' / 'sim' / 'models'
SDF_JINJA = MODELS / 'typhoon_h480.sdf.jinja'
SDF = MODELS / 'typhoon_h480.sdf'
LAUNCH = (REPO / 'navigation-stack' / 'DD_Nav_WS' / 'dd_gazebo_ws' / 'src' /
          'vision_landing' / 'launch' / 'livox_mid360.launch.py')

# SDF <pose> order.
AXES = ('x', 'y', 'z', 'roll', 'pitch', 'yaw')
TOLERANCE = 1e-4

# Regex rather than an XML parser: the .jinja is not valid XML until rendered.
XML_COMMENT = re.compile(r'<!--.*?-->', re.DOTALL)
JINJA_COMMENT = re.compile(r'\{#.*?#\}', re.DOTALL)
LIDAR_SENSOR = re.compile(
    r'<sensor\b[^>]*\bname\s*=\s*["\']lidar["\'][^>]*>(.*?)</sensor>',
    re.DOTALL)
POSE = re.compile(r'<pose\b[^>]*>(.*?)</pose>', re.DOTALL)


class CheckError(Exception):
    pass


def read_sdf_pose(path):
    """Return the lidar sensor's <pose> as a tuple of six floats."""
    try:
        text = path.read_text()
    except OSError as e:
        raise CheckError(f'{path}: {e.strerror}')

    # Both files carry long comments inside the sensor block that quote old
    # poses ("This line read 0 0 0.12 0 0 0 ..."). Strip them so only live
    # markup is searched.
    text = JINJA_COMMENT.sub('', XML_COMMENT.sub('', text))

    sensors = LIDAR_SENSOR.findall(text)
    if len(sensors) != 1:
        raise CheckError(
            f'{path}: expected one <sensor name="lidar">, found {len(sensors)}')

    poses = POSE.findall(sensors[0])
    if not poses:
        raise CheckError(f'{path}: lidar sensor has no <pose>')
    # The sensor's own pose comes first; anything later belongs to a child.
    raw = poses[0].strip()

    fields = raw.split()
    if len(fields) != len(AXES):
        raise CheckError(
            f'{path}: lidar <pose> has {len(fields)} values, expected '
            f'{len(AXES)}: {raw!r}')
    try:
        return tuple(float(f) for f in fields)
    except ValueError:
        # Most likely a {{ template expression }} in the .jinja.
        raise CheckError(f'{path}: lidar <pose> is not numeric: {raw!r}')


def read_launch_mount(path):
    """Return the mount_* default_values as a tuple of six floats."""
    try:
        tree = ast.parse(path.read_text(), filename=str(path))
    except OSError as e:
        raise CheckError(f'{path}: {e.strerror}')
    except SyntaxError as e:
        raise CheckError(f'{path}: {e}')

    defaults = {}
    for node in ast.walk(tree):
        if not (isinstance(node, ast.Call)
                and getattr(node.func, 'id', getattr(node.func, 'attr', None))
                == 'DeclareLaunchArgument'
                and node.args
                and isinstance(node.args[0], ast.Constant)):
            continue
        name = node.args[0].value
        if not (isinstance(name, str) and name.startswith('mount_')):
            continue
        for kw in node.keywords:
            if kw.arg == 'default_value':
                if not isinstance(kw.value, ast.Constant):
                    raise CheckError(
                        f'{path}: {name} default_value is not a literal')
                defaults[name] = kw.value.value

    values = []
    for axis in AXES:
        name = f'mount_{axis}'
        if name not in defaults:
            raise CheckError(f'{path}: no default_value for {name}')
        try:
            values.append(float(defaults[name]))
        except (TypeError, ValueError):
            raise CheckError(
                f'{path}: {name} default is not numeric: {defaults[name]!r}')
    return tuple(values)


def main():
    try:
        sources = {
            SDF_JINJA.name: read_sdf_pose(SDF_JINJA),
            SDF.name: read_sdf_pose(SDF),
            LAUNCH.name: read_launch_mount(LAUNCH),
        }
    except CheckError as e:
        print(f'ERROR: {e}', file=sys.stderr)
        return 2

    mismatches = []
    for i, axis in enumerate(AXES):
        values = [pose[i] for pose in sources.values()]
        if max(values) - min(values) > TOLERANCE:
            mismatches.append((axis, values))

    if not mismatches:
        print(f'OK: lidar mount agrees in all {len(sources)} files '
              f'(tolerance {TOLERANCE:g})')
        return 0

    names = list(sources)
    width = max(len(n) for n in names)
    header = f'{"axis":<6}  ' + '  '.join(f'{n:>{width}}' for n in names)
    print(f'MISMATCH: lidar mount differs (tolerance {TOLERANCE:g})\n')
    print(header)
    print('-' * len(header))
    for axis, values in mismatches:
        print(f'{axis:<6}  ' + '  '.join(f'{v:>{width}.6g}' for v in values))
    return 1


if __name__ == '__main__':
    sys.exit(main())
