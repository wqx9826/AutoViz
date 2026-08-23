#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
server_dir=$(cd -- "${script_dir}/.." && pwd)
repo_dir=$(cd -- "${server_dir}/.." && pwd)
protocol_dir="${server_dir}/third_party/AutoVizProto"
output_dir="${1:-${server_dir}/package/source}"

if [[ ! -f "${protocol_dir}/VERSION" ]]; then
    echo "ERROR: AutoVizProto source is missing. Run git submodule update --init --recursive." >&2
    exit 1
fi

protocol_version=$(tr -d '[:space:]' < "${protocol_dir}/VERSION")
protocol_commit=$(git -C "${protocol_dir}" rev-parse HEAD)
archive_name="AutoVizServer-source-protocol-${protocol_version}.tar.gz"
temp_dir=$(mktemp -d /tmp/autoviz-server-source.XXXXXX)
trap 'cmake -E remove_directory "${temp_dir}"' EXIT
cmake -E make_directory "${output_dir}" "${temp_dir}/AutoVizServer"

tar_path="${temp_dir}/AutoVizServer.tar"
git -C "${repo_dir}" ls-files -z --cached --others --exclude-standard -- AutoVizServer \
    | tar --null --exclude-vcs \
          --exclude='AutoVizServer/build' \
          --exclude='AutoVizServer/install' \
          --exclude='AutoVizServer/log' \
          --exclude='AutoVizServer/package' \
          --exclude='AutoVizServer/third_party/protobuf' \
          --exclude='AutoVizServer/third_party/AutoVizProto/build' \
          -C "${repo_dir}" -cf "${tar_path}" --files-from=-

printf 'AutoVizServer offline source package\nProtocol-Version: %s\nProtocol-Commit: %s\n' \
    "${protocol_version}" "${protocol_commit}" \
    > "${temp_dir}/AutoVizServer/SOURCE_MANIFEST.txt"
tar -C "${temp_dir}" -rf "${tar_path}" AutoVizServer/SOURCE_MANIFEST.txt
gzip -c "${tar_path}" > "${output_dir}/${archive_name}"

echo "Server source package: ${output_dir}/${archive_name}"
