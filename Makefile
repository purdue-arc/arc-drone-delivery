# Makefile (place at repo root)
# ----------------------------------------------------------------------------
# Convenience wrapper for the arc-drone-delivery Docker setup (uXRCE-DDS stack).
#
# Common workflows:
#   make build               # build the arc-drone:jazzy image
#   make up-sitl             # bring up containers for SITL mode
#   make up-hw               # bring up for hardware mode (Tarot T960 + Pixhawk)
#   make start               # start the mission (arms and flies!)
#   make abort               # abort into PX4 AUTO.LAND
#   make down                # stop all containers
#   make logs SVC=mission    # tail logs for one service
#   make shell SVC=mission   # interactive bash inside a service
#   make clean               # remove containers + volumes + dangling images
#
# MODE selects which overlay the inspection targets talk to (sitl|hw).
# It defaults to sitl; use `make logs MODE=hw SVC=mission` on the drone.
# ----------------------------------------------------------------------------

DOCKER_DIR := docker
COMPOSE := docker compose -f $(DOCKER_DIR)/docker-compose.yml
COMPOSE_SITL := $(COMPOSE) -f $(DOCKER_DIR)/docker-compose.sitl.yml
COMPOSE_HW := $(COMPOSE) -f $(DOCKER_DIR)/docker-compose.hardware.yml

# Which overlay the inspection/dev targets use. Overlays also carry the
# environment-specific settings (RTP port, tag size, calibration), so the
# right one must be selected for exec/logs to address the running stack.
MODE ?= sitl
ifeq ($(MODE),hw)
COMPOSE_ACTIVE := $(COMPOSE_HW)
else
COMPOSE_ACTIVE := $(COMPOSE_SITL)
endif

# Default service for shell/logs/restart targets if SVC isn't set
SVC ?= mission

.PHONY: help
help:
	@echo "arc-drone-delivery — Docker workflow (uXRCE-DDS)"
	@echo ""
	@echo "Build & lifecycle:"
	@echo "  make build              Build arc-drone:jazzy image"
	@echo "  make up-sitl            Start containers in SITL mode"
	@echo "  make up-hw              Start containers in hardware mode"
	@echo "  make down               Stop all containers"
	@echo "  make clean              Remove containers, volumes, dangling images"
	@echo ""
	@echo "Mission control:"
	@echo "  make start              Start the mission — ARMS AND FLIES"
	@echo "  make abort              Abort into PX4 AUTO.LAND"
	@echo "  make state              Print PX4 vehicle_status over uXRCE-DDS"
	@echo ""
	@echo "Inspection (add MODE=hw on the drone):"
	@echo "  make logs SVC=mission   Tail one service (xrce_agent|mission)"
	@echo "  make ps                 List running containers"
	@echo "  make shell SVC=mission  Bash inside a running container"
	@echo "  make topics             List ROS topics from a container"
	@echo "  make mission-state      Watch the mission FSM's own telemetry"
	@echo "  make check-px4-topics   Verify PX4 topic names match this build"
	@echo "  make test               Run the coordinate-transform unit tests"
	@echo ""
	@echo "Development:"
	@echo "  make rebuild-ws         Rebuild ROS workspace inside container"
	@echo "  make restart SVC=mission  Restart one container"

# -----------------------------------------------------------------------------
# Build
# -----------------------------------------------------------------------------
.PHONY: build
build:
	$(COMPOSE) build --build-arg USER_UID=$$(id -u) --build-arg USER_GID=$$(id -g)

# -----------------------------------------------------------------------------
# Lifecycle
# -----------------------------------------------------------------------------
.PHONY: up-sitl
up-sitl:
	@echo "==> Starting arc-drone in SITL mode"
	@echo "    Make sure PX4 SITL (Gazebo Classic) is already running on the host:"
	@echo "    cd navigation-stack/PX4-Autopilot && \\"
	@echo "      PX4_SITL_WORLD=apriltag_landing make px4_sitl gazebo-classic_typhoon_h480"
	$(COMPOSE_SITL) up -d
	@echo ""
	@echo "Watch mission:    make logs SVC=mission"
	@echo "Fly it:           make start"
	@echo "Stop everything:  make down"

.PHONY: up-hw
up-hw:
	@echo "==> Starting arc-drone in hardware mode"
	@echo "    Verify /dev/ttyTHS1 is accessible:"
	@ls -la /dev/ttyTHS1 2>/dev/null || echo "    WARNING: /dev/ttyTHS1 not found"
	$(COMPOSE_HW) up -d
	@echo ""
	@echo "Check PX4 link:   make state MODE=hw"
	@echo "Watch mission:    make logs MODE=hw SVC=mission"
	@echo "Fly it:           make start MODE=hw"

.PHONY: down
down:
	$(COMPOSE_ACTIVE) down

.PHONY: clean
clean: down
	docker system prune -f --volumes

# -----------------------------------------------------------------------------
# Mission control
# -----------------------------------------------------------------------------
# The mission controller idles until told to go. `start` arms the vehicle and
# flies the full search-and-land mission; `abort` hands it to PX4 AUTO.LAND.
.PHONY: start
start:
	@echo "==> Starting mission — the drone will ARM and TAKE OFF"
	$(COMPOSE_ACTIVE) exec mission bash -ic \
		'ros2 topic pub --once /arc/mission/start std_msgs/msg/Bool "{data: true}"'

.PHONY: abort
abort:
	@echo "==> Aborting mission — PX4 AUTO.LAND"
	$(COMPOSE_ACTIVE) exec mission bash -ic \
		'ros2 topic pub --once /arc/mission/start std_msgs/msg/Bool "{data: false}"'

# -----------------------------------------------------------------------------
# Inspection
# -----------------------------------------------------------------------------
.PHONY: logs
logs:
ifeq ($(SVC),all)
	$(COMPOSE_ACTIVE) logs -f
else
	$(COMPOSE_ACTIVE) logs -f $(SVC)
endif

.PHONY: ps
ps:
	$(COMPOSE_ACTIVE) ps

.PHONY: shell
shell:
	$(COMPOSE_ACTIVE) exec $(SVC) bash

# -----------------------------------------------------------------------------
# Development
# -----------------------------------------------------------------------------
.PHONY: rebuild-ws
rebuild-ws:
	@echo "==> Rebuilding ROS workspace inside running container"
	@echo "    (this repo has no top-level src/ — the entrypoint knows where"
	@echo "     the packages actually live; don't run colcon by hand here)"
	$(COMPOSE_ACTIVE) exec -e ARC_WS_REBUILD=1 $(SVC) /entrypoint.sh true

.PHONY: restart
restart:
	$(COMPOSE_ACTIVE) restart $(SVC)

# -----------------------------------------------------------------------------
# Quick one-shot debug helpers
# -----------------------------------------------------------------------------
.PHONY: topics
topics:
	$(COMPOSE_ACTIVE) exec $(SVC) bash -ic 'ros2 topic list'

.PHONY: state
state:
	$(COMPOSE_ACTIVE) exec mission bash -ic \
		'ros2 topic echo --once $${PX4_STATUS_TOPIC:-/fmu/out/vehicle_status}'

# What the mission controller thinks it is doing. Console logs are not
# telemetry; this is the topic to watch during a flight.
.PHONY: mission-state
mission-state:
	$(COMPOSE_ACTIVE) exec mission bash -ic \
		'ros2 topic echo /arc/mission/state'

# Confirm the PX4 topic names this build expects actually exist on the
# aircraft. A mismatch here is the failure that looks like a dead DDS link.
.PHONY: check-px4-topics
check-px4-topics:
	@echo "==> PX4 topics visible from the mission container:"
	@$(COMPOSE_ACTIVE) exec mission bash -ic 'ros2 topic list | grep fmu/out' || true
	@echo ""
	@echo "==> Topics this mission is configured to use:"
	@$(COMPOSE_ACTIVE) exec mission bash -ic \
		'echo "  $${PX4_STATUS_TOPIC:-/fmu/out/vehicle_status}"; \
		 echo "  $${PX4_LOCAL_POSITION_TOPIC:-/fmu/out/vehicle_local_position}"; \
		 echo "  $${PX4_BATTERY_TOPIC:-/fmu/out/battery_status}"; \
		 echo "  $${PX4_LAND_DETECTED_TOPIC:-/fmu/out/vehicle_land_detected}"'
	@echo ""
	@echo "If the lists disagree, override PX4_*_TOPIC in docker/.env — ALL FOUR."

# Unit tests for the coordinate transforms that place the aircraft.
.PHONY: test
test:
	$(COMPOSE_ACTIVE) exec $(SVC) bash -lc \
		'cd $${ARC_WS_BUILD:-/home/arc/build_ws} && \
		 colcon test --packages-select vision_landing --event-handlers console_direct+ && \
		 colcon test-result --verbose'
