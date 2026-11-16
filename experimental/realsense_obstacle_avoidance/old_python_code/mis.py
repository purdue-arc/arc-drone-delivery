# Imports
import cv2
import pyrealsense2 as rs
import numpy as np


# Helper Functions
def reset_devices():
    ctx = rs.context()
    devices = ctx.query_devices()
    for dev in devices:
        dev.hardware_reset()


def setup():
    reset_devices()
    pipeline = rs.pipeline()
    config = rs.config()
    config.enable_stream(rs.stream.color, 640, 480, rs.format.bgr8, 30)
    config.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)

    profile = pipeline.start(config)
    depth_sensor = profile.get_device().first_depth_sensor()
    depth_scale = depth_sensor.get_depth_scale()

    align_to = rs.stream.color
    align = rs.align(align_to)

    while True:
        color_frame, depth_frame = get_frame(pipeline)

        aligned_frames = align.process(color_frame)

        clipping_distance_in_meters = 1
        clipping_distance = clipping_distance_in_meters / depth_scale

        aligned_depth_frame = aligned_frames.get_depth_frame()
        color_frame = aligned_frames.get_color_frame()

        depth_image = np.asanyarray(aligned_depth_frame.get_data())
        color_image = np.asanyarray(color_frame.get_data())

        grey_color = 153
        depth_image_3d = np.dstack((depth_image, depth_image, depth_image))
        bg_removed = np.where((depth_image_3d > clipping_distance) | (depth_image_3d <= 0), grey_color, color_image)

        depth_colormap = cv2.applyColorMap(cv2.convertScaleAbs(depth_image, alpha=0.03), cv2.COLORMAP_JET)
        images = np.hstack((bg_removed, depth_colormap))
        cv2.namedWindow('Align Example', cv2.WINDOW_AUTOSIZE)
        cv2.imshow('Align Example', images)
        key = cv2.waitKey(1)

        if key & 0xFF == ord('q'):
            cv2.destroyAllWindows()
            break


def get_frame(pipeline):
    frames = pipeline.wait_for_frames()

    depth_image = np.asanyarray(frames.get_depth_frame().get_data())
    color_image = np.asanyarray(frames.get_color_frame().get_data())

    return depth_image, color_image


if __name__ == "__main__":
    setup()
    while True:
        ret, depth_frame, color_frame = []
        cv2.imshow(color_frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break