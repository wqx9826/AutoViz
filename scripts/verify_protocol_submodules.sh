#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "${script_dir}/.." && pwd)
paths=(
    "AutoVizProto"
    "AutoVizClient/third_party/AutoVizProto"
    "AutoVizServer/third_party/AutoVizProto"
)

expected_commit=""
expected_version=""
for relative_path in "${paths[@]}"; do
    protocol_dir="${repo_dir}/${relative_path}"
    if [[ ! -f "${protocol_dir}/CMakeLists.txt" || ! -f "${protocol_dir}/VERSION" ]]; then
        echo "ERROR: AutoVizProto submodule is not initialized: ${relative_path}" >&2
        echo "Run: git submodule update --init --recursive" >&2
        exit 1
    fi

    commit=$(git -C "${protocol_dir}" rev-parse HEAD)
    version=$(tr -d '[:space:]' < "${protocol_dir}/VERSION")
    if [[ -z "${expected_commit}" ]]; then
        expected_commit="${commit}"
        expected_version="${version}"
    elif [[ "${commit}" != "${expected_commit}" || "${version}" != "${expected_version}" ]]; then
        echo "ERROR: AutoVizProto revisions are inconsistent." >&2
        printf '  expected: %s %s\n' "${expected_commit}" "${expected_version}" >&2
        printf '  actual:   %s %s (%s)\n' "${commit}" "${version}" "${relative_path}" >&2
        exit 1
    fi
done

printf 'AutoVizProto %s (%s) is consistent in all three submodules.\n' \
    "${expected_version}" "${expected_commit}"
