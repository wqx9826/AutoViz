#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)
build_dir="${repo_root}/build/proto"
build_jobs="${AUTOVIZ_BUILD_JOBS:-4}"

cmake -S "${repo_root}/AutoVizProto" -B "${build_dir}" \
  -DAUTOVIZ_PROTO_BUILD_TESTS=ON
cmake --build "${build_dir}" --parallel "${build_jobs}"
"${build_dir}/autoviz_proto_tests"

cmake --install "${build_dir}" \
  --prefix "${repo_root}/AutoVizClient/third_party/AutoVizProto"
cmake --install "${build_dir}" \
  --prefix "${repo_root}/AutoVizServer/third_party/AutoVizProto"

echo "AutoVizProto SDK installed for AutoVizClient and AutoVizServer."
