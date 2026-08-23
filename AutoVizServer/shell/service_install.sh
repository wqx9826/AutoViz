#!/usr/bin/env bash
# ROS 2 Humble 的 setup.bash 会读取可选的未定义环境变量，不能启用 nounset (-u)。
set -eo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
WORKSPACE_DIR="${AUTOVIZ_SERVER_HOME:-$(cd -- "${script_dir}/.." && pwd)}"
SERVICE_USER="${AUTOVIZ_SERVER_USER:-${SUDO_USER:-$(id -un)}}"
SERVICE_GROUP="${AUTOVIZ_SERVER_GROUP:-$(id -gn "${SERVICE_USER}")}"
service_template="${WORKSPACE_DIR}/systemd/autoviz_server.service"
rendered_service=$(mktemp /tmp/autoviz-server-service.XXXXXX)
trap 'unlink -- "${rendered_service}" 2>/dev/null || true' EXIT

sed -e "s|@AUTOVIZ_SERVER_HOME@|${WORKSPACE_DIR}|g" \
    -e "s|@AUTOVIZ_SERVER_USER@|${SERVICE_USER}|g" \
    -e "s|@AUTOVIZ_SERVER_GROUP@|${SERVICE_GROUP}|g" \
    "${service_template}" > "${rendered_service}"
sudo install -m 0644 "${rendered_service}" /etc/systemd/system/autoviz_server.service


sudo systemctl daemon-reload

echo "AutoViz Server service installed successfully."

sudo systemctl enable autoviz_server.service
sudo systemctl start autoviz_server.service

systemctl is-enabled autoviz_server.service

systemctl --no-pager status autoviz_server.service

echo "AutoViz Server service started successfully."
