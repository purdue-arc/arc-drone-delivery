#!/usr/bin/env python3
"""Convert a ZED factory calibration file into the YAML zed_apriltag_node reads.

The ZED 2i stores factory intrinsics on-device; the SDK caches them as
    /usr/local/zed/settings/SN<serial>.conf   (or ~/.zed/settings/SN<serial>.conf)
and Stereolabs serves the same file at
    https://calib.stereolabs.com/?SN=<serial>

That .conf is an INI file with a section per camera/resolution, e.g.
[LEFT_CAM_HD] with fx, fy, cx, cy, k1, k2, p1, p2. This script picks one
section and writes an OpenCV FileStorage YAML with camera_matrix plus the
image_width/image_height that the intrinsics belong to — the node needs those
to rescale if the streamed resolution differs.

Usage:
  # from the on-device factory calibration
  ./zed_calib_to_yaml.py --conf /usr/local/zed/settings/SN12345.conf \\
      --side left --resolution HD720 -o zed2i_hd720.yaml

  # list what's in the file
  ./zed_calib_to_yaml.py --conf SN12345.conf --list

IMPORTANT: the resolution you pick must match the resolution the Jetson
actually streams. Factory intrinsics are rectified/pinhole values, which is
what AprilTag pose estimation wants; if you stream a cropped (not merely
scaled) image, re-calibrate instead of converting.
"""
import argparse
import configparser
import sys

# ZED resolution names -> (width, height) of a single (left or right) image.
RESOLUTIONS = {
    "2K":     (2208, 1242),
    "FHD":    (1920, 1080),
    "HD":     (1280, 720),
    "HD720":  (1280, 720),
    "VGA":    (672, 376),
}

# Section suffix used in the .conf for each resolution.
SECTION_SUFFIX = {
    "2K": "2K", "FHD": "FHD", "HD": "HD", "HD720": "HD", "VGA": "VGA",
}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--conf", required=True, help="ZED SN<serial>.conf file")
    ap.add_argument("--side", default="left", choices=["left", "right"],
                    help="Which camera of the stereo pair is being streamed")
    ap.add_argument("--resolution", default="HD720",
                    help="ZED resolution the Jetson streams (%s)" %
                         ", ".join(sorted(RESOLUTIONS)))
    ap.add_argument("-o", "--output", help="Output YAML path")
    ap.add_argument("--list", action="store_true",
                    help="List sections in the .conf and exit")
    args = ap.parse_args()

    cp = configparser.ConfigParser(strict=False)
    # ZED .conf keys are case-sensitive-ish; keep them as written.
    cp.optionxform = str
    try:
        with open(args.conf, "r", encoding="utf-8", errors="replace") as fh:
            cp.read_file(fh)
    except OSError as exc:
        print(f"error: cannot read {args.conf}: {exc}", file=sys.stderr)
        return 1

    if args.list:
        for name in cp.sections():
            print(name)
        return 0

    res = args.resolution.upper()
    if res not in RESOLUTIONS:
        print(f"error: unknown resolution {args.resolution!r}; "
              f"choose from {', '.join(sorted(RESOLUTIONS))}", file=sys.stderr)
        return 1
    if not args.output:
        print("error: -o/--output is required", file=sys.stderr)
        return 1

    section = f"{args.side.upper()}_CAM_{SECTION_SUFFIX[res]}"
    if section not in cp:
        print(f"error: section [{section}] not found in {args.conf}.\n"
              f"       Run with --list to see what the file contains.",
              file=sys.stderr)
        return 1

    sec = cp[section]

    def num(key):
        # ZED writes keys like fx=1054.6; be tolerant of case.
        for candidate in (key, key.lower(), key.upper()):
            if candidate in sec:
                try:
                    return float(sec[candidate])
                except ValueError:
                    break
        return None

    fx, fy, cx, cy = num("fx"), num("fy"), num("cx"), num("cy")
    missing = [n for n, v in (("fx", fx), ("fy", fy), ("cx", cx), ("cy", cy))
               if v is None]
    if missing:
        print(f"error: [{section}] is missing {', '.join(missing)}",
              file=sys.stderr)
        return 1

    width, height = RESOLUTIONS[res]

    # Distortion: factory values describe the RAW image. AprilTag pose here is
    # computed with a pinhole model, so emit zeros and rely on a rectified
    # stream. If you stream raw/unrectified video, undistort first.
    with open(args.output, "w", encoding="utf-8") as fh:
        fh.write("%YAML:1.0\n---\n")
        fh.write(f"# Generated from {args.conf} section [{section}]\n")
        fh.write("# Intrinsics belong to the resolution below; zed_apriltag_node\n")
        fh.write("# rescales them if the incoming stream differs.\n")
        fh.write(f"image_width: {width}\n")
        fh.write(f"image_height: {height}\n")
        fh.write("camera_matrix: !!opencv-matrix\n")
        fh.write("   rows: 3\n   cols: 3\n   dt: d\n")
        fh.write(f"   data: [ {fx:.6f}, 0., {cx:.6f},\n")
        fh.write(f"           0., {fy:.6f}, {cy:.6f},\n")
        fh.write("           0., 0., 1. ]\n")
        fh.write("distortion_coefficients: !!opencv-matrix\n")
        fh.write("   rows: 1\n   cols: 5\n   dt: d\n")
        fh.write("   data: [ 0., 0., 0., 0., 0. ]\n")

    print(f"wrote {args.output}")
    print(f"  section     : {section}")
    print(f"  resolution  : {width}x{height}")
    print(f"  fx,fy       : {fx:.3f}, {fy:.3f}")
    print(f"  cx,cy       : {cx:.3f}, {cy:.3f}")
    print()
    print("Verify against the live stream before flying:")
    print("  ros2 run zed_apriltag_streaming zed_apriltag_node --ros-args \\")
    print(f"    -p calib_file:={args.output} -p tag_size_m:=<your tag> \\")
    print("    -p publish_debug_image:=true")
    print("Then hold the tag at a measured distance and confirm the z value in")
    print("  ros2 topic echo /landing_target_pose")
    print("matches reality. If it is off by a constant factor, the tag size or")
    print("the streamed resolution is wrong.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
