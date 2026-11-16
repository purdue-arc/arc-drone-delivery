"""
Load an octree from a precomputed .json file.
Faster than recomputing the octree every time.
"""
import open3d as o3d

octree = o3d.io.read_octree("octree.json")
o3d.visualization.draw_geometries([octree])
