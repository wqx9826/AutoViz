#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
client_dir=$(cd -- "${script_dir}/.." && pwd)
repo_dir=$(cd -- "${client_dir}/.." && pwd)
protocol_dir="${client_dir}/third_party/AutoVizProto"
output_dir="${1:-${client_dir}/package/source}"

if [[ ! -f "${protocol_dir}/VERSION" ]]; then
    echo "ERROR: AutoVizProto source is missing. Run git submodule update --init --recursive." >&2
    exit 1
fi

protocol_version=$(tr -d '[:space:]' < "${protocol_dir}/VERSION")
protocol_commit=$(git -C "${protocol_dir}" rev-parse HEAD)
archive_name="AutoVizClient-source-protocol-${protocol_version}.tar.gz"
temp_dir=$(mktemp -d /tmp/autoviz-client-source.XXXXXX)
trap 'cmake -E remove_directory "${temp_dir}"' EXIT
cmake -E make_directory "${output_dir}" "${temp_dir}/AutoVizClient"

tar_path="${temp_dir}/AutoVizClient.tar"
git -C "${repo_dir}" ls-files -z --cached --others --exclude-standard -- AutoVizClient \
    | tar --null --exclude-vcs \
          --exclude='AutoVizClient/build' \
          --exclude='AutoVizClient/package' \
          --exclude='AutoVizClient/third_party/protobuf' \
          --exclude='AutoVizClient/third_party/Qt5' \
          --exclude='AutoVizClient/third_party/Qt6' \
          --exclude='AutoVizClient/third_party/AutoVizProto/build' \
          -C "${repo_dir}" -cf "${tar_path}" --files-from=-

printf 'AutoVizClient offline source package\nProtocol-Version: %s\nProtocol-Commit: %s\n' \
    "${protocol_version}" "${protocol_commit}" \
    > "${temp_dir}/AutoVizClient/SOURCE_MANIFEST.txt"
tar -C "${temp_dir}" -rf "${tar_path}" AutoVizClient/SOURCE_MANIFEST.txt
gzip -c "${tar_path}" > "${output_dir}/${archive_name}"

echo "Client source package: ${output_dir}/${archive_name}"
