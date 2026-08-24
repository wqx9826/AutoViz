#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "${script_dir}/.." && pwd)
protocol_dir="${repo_dir}/AutoVizProto"
output_dir="${1:-${repo_dir}/package/source}"

if [[ ! -f "${protocol_dir}/VERSION" ]]; then
    echo "ERROR: AutoVizProto source is missing: ${protocol_dir}" >&2
    exit 1
fi

protocol_version=$(tr -d '[:space:]' < "${protocol_dir}/VERSION")
repo_revision=$(git -C "${repo_dir}" rev-parse HEAD)
if [[ -n $(git -C "${repo_dir}" status --porcelain) ]]; then
    repo_revision="${repo_revision}-dirty"
fi
archive_name="AutoViz-source-protocol-${protocol_version}.tar.gz"
temp_dir=$(mktemp -d /tmp/autoviz-source.XXXXXX)
trap 'cmake -E remove_directory "${temp_dir}"' EXIT

mkdir -p "${temp_dir}/AutoViz"
{ while IFS= read -r -d '' path; do
    [[ -e "${repo_dir}/${path}" ]] && printf '%s\0' "${path}"
done
true
} < <(git -C "${repo_dir}" ls-files -z --cached --others --exclude-standard) \
    | tar --null --exclude-vcs --transform='s,^,AutoViz/,' \
          --exclude='AutoVizProto/build' \
          --exclude='AutoVizClient/build' \
          --exclude='AutoVizClient/package' \
          --exclude='AutoVizClient/third_party/protobuf' \
          --exclude='AutoVizClient/third_party/Qt5' \
          --exclude='AutoVizClient/third_party/Qt6' \
          --exclude='AutoVizServer/build' \
          --exclude='AutoVizServer/install' \
          --exclude='AutoVizServer/log' \
          --exclude='AutoVizServer/package' \
          --exclude='AutoVizServer/third_party/protobuf' \
          -C "${repo_dir}" -cf "${temp_dir}/AutoViz.tar" --files-from=-

printf 'AutoViz unified source package\nProtocol-Version: %s\nRepository-Revision: %s\n' \
    "${protocol_version}" "${repo_revision}" > "${temp_dir}/AutoViz/SOURCE_MANIFEST.txt"
tar -C "${temp_dir}" -rf "${temp_dir}/AutoViz.tar" AutoViz/SOURCE_MANIFEST.txt
mkdir -p "${output_dir}"
gzip -c "${temp_dir}/AutoViz.tar" > "${output_dir}/${archive_name}"

echo "Unified source package: ${output_dir}/${archive_name}"
