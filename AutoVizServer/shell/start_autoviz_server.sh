#!/usr/bin/env bash
# ROS 2 Humble 的 setup.bash 会读取可选的未定义环境变量，不能启用 nounset (-u)。
set -eo pipefail

WORKSPACE_DIR="/home/nvidia/AutoVizServer"
ROBOT_WS_DIR="/home/nvidia/robot_ws"

cd "${WORKSPACE_DIR}"
source /opt/ros/humble/setup.bash
source "${ROBOT_WS_DIR}/install/setup.bash"
source "${WORKSPACE_DIR}/install/setup.bash"

# 使用 exec 让 systemd 直接跟踪 ros2 launch 进程，并将 SIGINT 正确传递给 ROS。
exec ros2 launch autoviz_server autoviz_server.launch.py
