#!/usr/bin/env bash
set -eo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
server_dir=$(cd -- "${script_dir}/.." && pwd)
repo_dir=$(cd -- "${server_dir}/.." && pwd)
protocol_dir="${repo_dir}/AutoVizProto"
output_dir="${1:-${server_dir}/package/runtime}"

if [[ ! -f "${protocol_dir}/VERSION" ]]; then
    echo "ERROR: AutoVizProto source is missing: ${protocol_dir}" >&2
    exit 1
fi
if ! command -v colcon >/dev/null 2>&1; then
    echo "ERROR: colcon is required on the build machine." >&2
    exit 1
fi

protocol_version=$(tr -d '[:space:]' < "${protocol_dir}/VERSION")
protocol_commit=$(git -C "${repo_dir}" rev-parse HEAD)
architecture=$(uname -m)
package_name="AutoVizServer-linux-${architecture}-protocol-${protocol_version}"
stage_dir="${server_dir}/package/.stage/${package_name}"
archive_path="${output_dir}/${package_name}.tar.gz"

cd "${server_dir}"
colcon build --cmake-args \
    -DCMAKE_BUILD_TYPE=Release \
    -DAUTOVIZ_THIRD_PARTY_DIR="${server_dir}/third_party"

server_binary="${server_dir}/install/autoviz_server/lib/autoviz_server/autoviz_server_node"
if [[ ! -x "${server_binary}" ]]; then
    echo "ERROR: Server binary was not produced: ${server_binary}" >&2
    exit 1
fi

dependency_report=$(ldd "${server_binary}")
if grep -q 'not found' <<< "${dependency_report}"; then
    echo "ERROR: Server runtime has unresolved dynamic dependencies:" >&2
    echo "${dependency_report}" >&2
    exit 1
fi

cmake -E remove_directory "${stage_dir}"
cmake -E make_directory "${stage_dir}" "${stage_dir}/shell" "${output_dir}"
cmake -E copy_directory "${server_dir}/install" "${stage_dir}/install"
cmake -E copy_directory "${server_dir}/systemd" "${stage_dir}/systemd"
cmake -E copy "${server_dir}/shell/start_autoviz_server.sh" "${stage_dir}/shell/"
cmake -E copy "${server_dir}/shell/service_install.sh" "${stage_dir}/shell/"

printf 'AutoVizServer runtime package\nProtocol-Version: %s\nProtocol-Commit: %s\nArchitecture: %s\nRequired-Runtime: ROS2 Humble and robot_ws/custom_msgs\n' \
    "${protocol_version}" "${protocol_commit}" "${architecture}" > "${stage_dir}/VERSION"
printf '%s\n' "${dependency_report}" > "${stage_dir}/runtime-dependencies.txt"

tar -C "$(dirname -- "${stage_dir}")" -czf "${archive_path}" "${package_name}"
echo "Server runtime package: ${archive_path}"
