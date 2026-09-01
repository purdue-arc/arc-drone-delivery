# ARC Drone Delivery - Monorepo

This is the unified monorepo for Purdue's Autonomous Robotics Club (ARC) Drone Delivery project. All drone delivery subsystems have been consolidated here for easier development and collaboration.

## 📄 Documentation

Please refer to the `Documentation` folder for project docs, guides, and technical references.

## 📁 Repository Structure

```
arc-drone-delivery/
├── navigation-stack/     # Core ROS 2 Navigation Workspace (DD_Nav_WS) + PX4 Gazebo simulation
│   ├── ROS2_PX4_Offboard_Example/  # PX4 offboard velocity & navigation control
│   ├── config/                     # SLAM toolbox + Nav2 config YAMLs
│   ├── simple_odometry/            # Odometry TF publisher for PX4
│   ├── default.sdf                 # Gazebo world file
│   └── model.sdf                   # Drone model file
├── onboarding/           # Tutorial and onboarding materials
├── avoidance-viz/        # Obstacle avoidance visualization tool
├── path-planning/        # Path planning and SLAM algorithms
├── obstacle-avoidance/   # C++ Intel RealSense obstacle avoidance
├── octree-generator/     # Octree generation for mapping
└── operations-website/   # Solid-start frontend operations website
```

## 🚀 Quick Start

Each subdirectory contains its own README with specific setup instructions. Here's a brief overview:

### Navigation Stack
**Path**: `navigation-stack/`  
**Purpose**: Core ROS 2 workspace for drone navigation, PX4 integration, and Gazebo simulation  
**Key Features**: GPS global mapping, Nav2 implementation, PX4 flight controller integration  
**Setup**: See [navigation-stack/README.md](navigation-stack/README.md)

### Drone Delivery Simulation
**Path**: `navigation-stack/` (simulation packages)  
**Purpose**: ROS2 PX4 Gazebo Harmonic simulation with SLAM-based mapping and Nav2 autonomous navigation  
**Key Features**: Drone SLAM mapping, goal-based navigation via RViz2, LiDAR bridge, odometry TF publishing  
**Prerequisites**: ROS2 Humble, Gazebo Harmonic, PX4 Autopilot  
**Packages**:
- `ROS2_PX4_Offboard_Example/` — offboard velocity and navigation control
- `simple_odometry/` — odometry and TF publisher
- `config/` — SLAM toolbox and Nav2 parameter files
- `default.sdf` / `model.sdf` — Gazebo world and drone model  

**Setup**: See [navigation-stack/README.md](navigation-stack/README.md)

### Onboarding
**Path**: `onboarding/`  
**Purpose**: Comprehensive tutorial for new team members  
**Topics**: Git workflow, ROS 2 basics, Gazebo simulation, publisher/subscriber patterns  
**Setup**: See [onboarding/README.md](onboarding/README.md)

### Avoidance Visualization
**Path**: `avoidance-viz/`  
**Purpose**: Visualization tool for obstacle avoidance verification  
**Setup**: See [avoidance-viz/README.md](avoidance-viz/README.md)

### Path Planning
**Path**: `path-planning/`  
**Purpose**: Path planning algorithms including A*, D*, and Dijkstra  
**Language**: C++  
**Setup**: See [path-planning/README.md](path-planning/README.md)

### Obstacle Avoidance
**Path**: `obstacle-avoidance/`  
**Purpose**: Real-time obstacle detection using Intel RealSense cameras  
**Platform**: Windows with Visual Studio  
**Setup**: See [obstacle-avoidance/README.md](obstacle-avoidance/README.md)

### Octree Generator
**Path**: `octree-generator/`  
**Purpose**: Generate octree representations for 3D mapping  
**Setup**: See [octree-generator/](octree-generator/)

### Operations Website
**Path**: `operations-website/`  
**Purpose**: Web-based drone operations dashboard with CesiumJS 3D visualization  
**Tech Stack**: Solid.js, Solid Start, Material UI (SUID), Nhost, PostgreSQL  
**Features**: Manual drone control, flight monitoring, delivery operations  
**Setup**: See [operations-website/README.md](operations-website/README.md)

## 🤖 AI Coding Assistants (Recommended)

To help you understand and contribute to this large codebase, we **strongly recommend** using AI coding assistants:

### GitHub Copilot
- **Free for students**: Get a free subscription with your `.edu` email
- **Sign up**: [GitHub Student Developer Pack](https://education.github.com/pack)
- **Features**: Code completion, chat, and explanations directly in VS Code

### Google AI Pro (Gemini)
- **Free tier**: 1 year of Google AI Pro with your education email
- **Sign up**: [Google for Education](https://edu.google.com/intl/ALL_us/workspace-for-education/)
- **Features**: Advanced code understanding, debugging assistance, and documentation generation
- Download Google Antigravity IDE and sign in

These tools are invaluable for:
- Understanding unfamiliar ROS 2 concepts
- Debugging complex C++ and Python code
- Learning the PX4 flight controller API
- Navigating this large monorepo structure

## 🛠️ Development Workflow

### Prerequisites
- **ROS 2 Humble** (for navigation-stack, onboarding, simulation)
- **Gazebo Harmonic** (for simulation)
- **Ubuntu 22.04** (recommended for ROS components)
- **Node.js** (for operations-website)
- **Python 3** (for various scripts)
- **Visual Studio** (for obstacle-avoidance on Windows)

### Building Components

Each component has its own build system:
- **ROS packages**: Use `colcon build` in the workspace directory
- **C++ projects**: Use `make` or Visual Studio
- **Website**: Use `npm install` and `npm run dev`

## 📝 Contributing

1. Create a feature branch from `main`
2. Make your changes in the appropriate subdirectory
3. Test thoroughly
4. Submit a pull request with a clear description

## 🧹 Monorepo Migration Notes

This monorepo was created on **February 1, 2026** by consolidating 7 individual repositories:
- Removed 121 accidentally-committed build log files from navigation-stack
- Cleaned git history to improve clone performance
- Maintained logical separation of components in subdirectories

### Original Repositories
- [`DD_Nav_WS`](https://github.com/purdue-arc/DD_Nav_WS) → `navigation-stack/`
- [`DD_On_Boarding`](https://github.com/purdue-arc/DD_On_Boarding) → `onboarding/`
- [`dd-avoidance-visualization`](https://github.com/purdue-arc/dd-avoidance-visualization) → `avoidance-viz/`
- [`dd-navigation`](https://github.com/purdue-arc/dd-navigation) → `path-planning/`
- [`DD-obstacle-avoidance`](https://github.com/purdue-arc/DD-obstacle-avoidance) → `obstacle-avoidance/`
- [`dd-octree_generator`](https://github.com/purdue-arc/dd-octree_generator) → `octree-generator/`
- [`drone-delivery-website`](https://github.com/purdue-arc/drone-delivery-website) → `operations-website/`
- [`DroneDeliverySim`](https://github.com/Ymz2006/DroneDeliverySim) → `navigation-stack/` (simulation packages)

### ⚠️ Important: Accessing Old Branches and Large Files

**Specialized Branches Not in Monorepo:**
The following branches were **not** migrated to preserve a clean monorepo:
- `DD_Nav_WS`: `px4`, `px4-twist`, `px4-test`, `nav2_implementation`, `teleop`, `gps_global_mapping`
- `DD_On_Boarding`: `simulation_dev`
- `dd-navigation`: `SLAM_Main`
- `dd-octree_generator`: `demonstration-of-bad-voxels`
- `drone-delivery-website`: `docker-testing`, `geography_point_change`, `supabase-integration`

**To access these branches:**
1. Clone the original GitHub repositories: `https://github.com/purdue-arc/[repo-name]`
2. Checkout the specific branch you need
3. Cherry-pick commits into the monorepo if needed

**Git LFS Large Files:**
The following large files are stored in Git LFS and were **not** included in the monorepo:
- `Purdue_map.dae` (977 MB) - Purdue campus 3D model
- `purdue_map.gltf` - Purdue campus GLTF format
- `test_world.dae` - Gazebo test world file

**To access LFS files:**
1. Clone the original `DD_Nav_WS` repository from GitHub
2. Run `git lfs pull` to download the large files
3. Copy them to your local monorepo if needed (they're gitignored)

## 📚 Resources

- [Purdue ARC Website](https://purduearc.com/)
- [ROS 2 Documentation](https://docs.ros.org/en/humble/)
- [PX4 Autopilot](https://px4.io/)
- [Gazebo Simulation](https://gazebosim.org/)
- [DroneDeliverySim (original)](https://github.com/Ymz2006/DroneDeliverySim)

## Flight software status

Date: 2026-09-01
Environment:
- Host: Ubuntu 22.04 (unchanged)
- Docker image: arc-drone:jazzy (Ubuntu 24.04 + ROS 2 Jazzy)
- Workspace: ~/Documents/arc-drone-delivery
- Build: colcon --symlink-install

**PX4 interface: uXRCE-DDS end-to-end.** MAVROS has been dropped — the
flight stack talks to PX4 directly on `/fmu/*` topics through the Micro
XRCE-DDS Agent. There is one mission stack:

| Role | Package |
|---|---|
| Mission logic (search + precision landing + failsafes) | `navigation-stack/.../vision_landing` (`mission_controller`) |
| Perception, hardware (ZED) | `landing/zed_apriltag_streaming` (`zed_apriltag_node`) |
| Perception, SITL (Gazebo camera) | `vision_landing` (`apriltag_detector`) |
| Deployment | `docker/` — `xrce_agent`, `mission`, `perception`/`zed_apriltag` |

Both perception nodes publish the same `/landing_target_pose` (camera-frame
tag pose), so the mission controller is unchanged between sim and hardware.

The mission controller starts **IDLE** and does not arm until an operator
publishes to `/arc/mission/start` (`make start`). See
`navigation-stack/DD_Nav_WS/dd_gazebo_ws/src/vision_landing/README.md`.

### Flight readiness

The full delivery mission has **not yet run end to end**, in SITL or on
hardware. Before a first powered test:

- [x] Container build and launch path works (`make up-sitl` / `make up-hw`)
- [x] PX4 topic names pinned to the firmware, and checkable (`make check-px4-topics`)
- [x] Delivery mission wired into the hardware overlay (winch, Nav2, transit)
- [x] Failsafe stows or drops the payload before landing (`SECURE_PAYLOAD`)
- [x] Companion-side geofence enforced in flight; PX4 `GF_*` set in `config/px4/`
- [x] Mission telemetry on `/arc/mission/state`, bagged with `record:=true`
- [x] Unit tests on the two transforms that place the aircraft (`make test`)
- [ ] **Livox driver** vendored and publishing `/livox/points` — until then the
      transit refuses to fly (`require_plan_to_transit`), by design
- [ ] **Winch bench-tested** with a load, props off — the timings in
      `winch_bridge` are unmeasured placeholders
- [ ] **Gimbal sweep verified** against a protractor, props off
- [ ] **ZED calibration generated** and `CALIB_FILE` set — perception now
      refuses to start without it
- [ ] Full mission flown repeatedly in SITL

`config/px4/README.md` lists the bench tests these imply.

Deprecated:
- `arc_landing` — the old MAVROS landing FSM (see `arc_landing/DEPRECATED.md`)

Ignored via COLCON_IGNORE:
- navigation-stack/PX4-Autopilot (flight controller, not a ROS package)