# Experimental

Prototypes and research code from before the flight software took its
current shape. **None of it is built, launched or used by the aircraft.**
Each project has its own build setup and is independent of the rest of the
repo.

Before you build on anything here, check whether the flight software already
solves the problem. The table says what replaced each project.

| Project | What it is | Language / platform | Status |
|---|---|---|---|
| [`path_planning/`](path_planning/) | A*, D* and Dijkstra grid planners with a maze generator and tests. Build with `make`. | C++ | Not used. Flight route planning is done by Nav2's planner (`drone_nav`, `vision_landing/nav2_path_bridge`). An earlier version of this code is in `legacy/path_planning_original/`. |
| [`realsense_obstacle_avoidance/`](realsense_obstacle_avoidance/) | Obstacle detection from an Intel RealSense depth camera. `old_python_code/` is the first Python version. | C++, Visual Studio, Windows | Not used. The RealSense was evaluated and rejected (about 1 m effective range); the aircraft uses a Livox Mid-360 lidar. |
| [`octree_generator/`](octree_generator/) | Builds a 3D octree map from a mesh exported from OpenStreetMap (`export.osm`), and views it. | Python, Open3D | Not used. `test.py` reads its mesh from a hardcoded path on the original author's machine; change `MESH_LOCATION` before running. GPL-3.0 licensed (see its `LICENSE`). |
| [`avoidance_viz/`](avoidance_viz/) | Pygame GUI for drawing a grid and visualizing a D* path through it; `dstar/` holds the C++ planner. | Python + C++ | **Does not run.** `gui.py` imports `find_path` from a `dstar` Python module that doesn't exist in this repo (and never did after the monorepo merge). |

Original repositories are listed in the root `README.md` under
"Monorepo Migration Notes".
