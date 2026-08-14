#!/usr/bin/env bash
# ROS 2 Humble 的 setup.bash 会读取可选的未定义环境变量，不能启用 nounset (-u)。
set -eo pipefail

WORKSPACE_DIR="/home/nvidia/AutoVizServer"

sudo install -m 0644 "${WORKSPACE_DIR}/systemd/autoviz_server.service" /etc/systemd/system/autoviz_server.service


sudo systemctl daemon-reload

echo "AutoViz Server service installed successfully."

sudo systemctl enable autoviz_server.service
sudo systemctl start autoviz_server.service

systemctl is-enabled autoviz_server.service

systemctl --no-pager status autoviz_server.service

echo "AutoViz Server service started successfully."