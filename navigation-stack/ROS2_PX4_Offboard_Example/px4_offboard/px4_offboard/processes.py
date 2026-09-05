
#!/usr/bin/env python3

# Import the subprocess and time modules
import subprocess
import time

# Path to the repository's PX4-Autopilot (contains the fixed Typhoon H480 model)
PX4_DIR = "/home/raghav/Documents/arc-drone-delivery/navigation-stack/PX4-Autopilot"

# List of commands to run
commands = [
    # Run the Micro XRCE-DDS Agent
    "MicroXRCEAgent udp4 -p 8888",

    # Run the PX4 SITL simulation above the origin with zero orientation.
    # gz_typhoon_h480 = the repo's fixed hexarotor (rotor joints + meshes).
    # z=0.3: spawning at z=0 embeds the legs in the apriltag_pad plate at the
    # origin and the contact solver pins the drone to the ground (motors spin,
    # zero lift). Spawning slightly above lets it settle cleanly onto the pad.
    f"cd {PX4_DIR} && PX4_GZ_MODEL_POSE='0,0,0.3,0,0,0' make px4_sitl gz_typhoon_h480"

    # Run QGroundControl
    # "cd ~/QGroundControl && ./QGroundControl.AppImage"
]
# Loop through each command in the list
for command in commands:
    # Each command is run in a new tab of the gnome-terminal
    subprocess.run(["gnome-terminal", "--tab", "--", "bash", "-c", command + "; exec bash"])
    
    # Pause between each command
    time.sleep(1)
