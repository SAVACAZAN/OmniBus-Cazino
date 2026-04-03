"""
VPS Setup Script — Install deps + build OmniBus Casino on remote server.
"""
import paramiko
import sys
import time

HOST = "193.219.97.147"
USER = "alwyzon"
PASS = "96XCSevWY66M"

COMMANDS = [
    # 1. Install all build dependencies
    "sudo apt update",
    "sudo DEBIAN_FRONTEND=noninteractive apt install -y g++ cmake ninja-build git libgl1-mesa-dev libxkbcommon-dev libxkbcommon-x11-0",
    "sudo DEBIAN_FRONTEND=noninteractive apt install -y qt6-base-dev qt6-websockets-dev libqt6svg6-dev libssl-dev",

    # 2. Verify tools installed
    "g++ --version | head -1",
    "cmake --version | head -1",
    "ninja --version",

    # 3. Clean and rebuild
    "cd ~/OmniBus-Cazino && rm -rf build && mkdir build",
    "cd ~/OmniBus-Cazino/build && cmake .. -G Ninja -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake/Qt6",
    "cd ~/OmniBus-Cazino/build && cmake --build . -j$(nproc)",

    # 4. Check result
    "ls -lh ~/OmniBus-Cazino/build/OmniBusCazino",
]

def run():
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    print(f"Connecting to {HOST}...")
    ssh.connect(HOST, username=USER, password=PASS, timeout=15)
    print("Connected!\n")

    for cmd in COMMANDS:
        print(f">>> {cmd}")
        stdin, stdout, stderr = ssh.exec_command(cmd, timeout=300)
        out = stdout.read().decode()
        err = stderr.read().decode()
        if out.strip():
            print(out.strip())
        if err.strip():
            # Filter apt noise
            for line in err.strip().split("\n"):
                if "WARNING" not in line and "dpkg-preconfigure" not in line:
                    print(f"  [stderr] {line}")
        print()

    ssh.close()
    print("DONE!")

if __name__ == "__main__":
    run()
