# Imports
import constants
import cv2
import pyrealsense2 as rs
import numpy as np
from matplotlib import pyplot as plot


# Helper Functions
def setup():
    # devices_reset()

    # Controls all connected realsense devices
    pipeline = rs.pipeline()
    config = rs.config()

    config.enable_stream(rs.stream.color, 640, 480, rs.format.bgr8, 30)
    config.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)

    return [pipeline, config]


def devices_reset():
    ctx = rs.context()
    devices = ctx.query_devices()
    for dev in devices:
        dev.hardware_reset()


def generate2DDepthMatrix(frame):
    occupancy_matrix = []
    for y in range(constants.height):
        occupancy_matrix_row = []
        for x in range(constants.width):
            dist = frame.get_distance(x, y)
            occupancy_matrix_row.append(dist)
        occupancy_matrix.append(occupancy_matrix_row)
    return occupancy_matrix


def convertDepthMatrix(depthMatrix, depth):
    depth = int(depth)
    occupancy_matrix3D = [[[0] * depth * int(1 / constants.blockSize)] * constants.width] * constants.height
    for y in range (constants.height):
        for x in range (constants.width):
            dist = depthMatrix[y][x]
            while (dist < depth):
                occupancy_matrix3D[y][x][int(dist / constants.blockSize)] = 1
                dist += constants.blockSize
    return np.array(occupancy_matrix3D)


def printTextRepresentation(frame, objectDepthDetection):
    coverage = [0] * 64
    for y in range(constants.height):
        for x in range(constants.width):
            dist = frame.get_distance(x, y)
            if 0 < dist and dist < objectDepthDetection:
                coverage[x // 10] += 1
        if y % 20 is 19:
            line = ""
            for c in coverage:
                line += " .:nhBXiWW"[c // 25]
            coverage = [0] * 64
            print(line)


def displayVideo(color_frame, depth_frame):
    print("Work in progress")


def cube_marginals(cube, normalize=False):
    c_fcn = np.mean if normalize else np.sum
    xy = c_fcn(cube, axis=0)
    xz = c_fcn(cube, axis=1)
    yz = c_fcn(cube, axis=2)
    return(xy,xz,yz)

def plotcube(cube,x=None,y=None,z=None,normalize=False,plot_front=False):
    """Use contourf to plot cube marginals"""
    (Z,Y,X) = cube.shape
    (xy,xz,yz) = cube_marginals(cube,normalize=normalize)
    if x == None: x = np.arange(X)
    if y == None: y = np.arange(Y)
    if z == None: z = np.arange(Z)

    fig = plot.figure()
    ax = fig.gca(projection='3d')

    # draw edge marginal surfaces
    offsets = (Z-1,0,X-1) if plot_front else (0, Y-1, 0)
    cset = ax.contourf(x[None,:].repeat(Y,axis=0), y[:,None].repeat(X,axis=1), xy, zdir='z', offset=offsets[0], cmap=plot.cm.coolwarm, alpha=0.75)
    cset = ax.contourf(x[None,:].repeat(Z,axis=0), xz, z[:,None].repeat(X,axis=1), zdir='y', offset=offsets[1], cmap=plot.cm.coolwarm, alpha=0.75)
    cset = ax.contourf(yz, y[None,:].repeat(Z,axis=0), z[:,None].repeat(Y,axis=1), zdir='x', offset=offsets[2], cmap=plot.cm.coolwarm, alpha=0.75)

    # draw wire cube to aid visualization
    ax.plot([0,X-1,X-1,0,0],[0,0,Y-1,Y-1,0],[0,0,0,0,0],'k-')
    ax.plot([0,X-1,X-1,0,0],[0,0,Y-1,Y-1,0],[Z-1,Z-1,Z-1,Z-1,Z-1],'k-')
    ax.plot([0,0],[0,0],[0,Z-1],'k-')
    ax.plot([X-1,X-1],[0,0],[0,Z-1],'k-')
    ax.plot([X-1,X-1],[Y-1,Y-1],[0,Z-1],'k-')
    ax.plot([0,0],[Y-1,Y-1],[0,Z-1],'k-')

    ax.set_xlabel('X')
    ax.set_ylabel('Y')
    ax.set_zlabel('Z')
    plot.show()


if __name__ == "__main__":
    pipeline, config = setup()

    # Start streaming
    pipeline.start(config)

    # while True:
        # Waits and gets frames from the current rs pipeline
    frames = pipeline.wait_for_frames()
    depthFrame = frames.get_depth_frame()
    colorFrame = frames.get_color_frame()

    if depthFrame:
        # Printing text-based representation of image in 10x20 pixel regions
        printTextRepresentation(depthFrame, constants.detectionDepth)

        # Generating a 2D matrix with depth as cell value
        depthMatrix = generate2DDepthMatrix(depthFrame)

        # Converting to a 3D occupancy matrix
        occupancyMatrix = convertDepthMatrix(depthMatrix, constants.detectionDepth)

        # display the occupancy matrix
        plotcube(occupancyMatrix)

        # Printing occupancy matrix matrix
        print(depthMatrix)

    if colorFrame:
        displayVideo(colorFrame, depthFrame)

    # Exit code
    if cv2.waitKey(1) & 0xFF == ord('q'):
        cv2.destroyAllWindows()
        # break

    pipeline.stop()
    exit(0)