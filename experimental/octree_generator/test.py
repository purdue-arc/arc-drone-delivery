import numpy as np
import open3d as o3d

N = 500000
OCT_DEPTH = 10
MESH_LOCATION = r"/Users/bbworld/git/octree_generator/export.obj"


mesh = o3d.io.read_triangle_mesh(MESH_LOCATION)
# mesh2 = copy.deepcopy(mesh)
pcd = mesh.sample_points_poisson_disk(N)
# fit to unit cube
pcd.scale(1 / np.max(pcd.get_max_bound() - pcd.get_min_bound()),
          center=pcd.get_center())
pcd.colors = o3d.utility.Vector3dVector(np.random.uniform(0, 1, size=(N, 3)))
o3d.visualization.draw_geometries([pcd])

print('octree division')
octree = o3d.geometry.Octree(max_depth=OCT_DEPTH)
octree.convert_from_point_cloud(pcd)

o3d.io.write_octree("octree.json", octree)

o3d.visualization.draw_geometries([octree])
