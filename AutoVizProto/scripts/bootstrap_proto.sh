#!/usr/bin/env bash

set -euo pipefail

# 构建唯一的协议 SDK，并安装到两个消费工程自己的 third_party 目录。
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
proto_dir=$(cd -- "${script_dir}/.." && pwd)
repo_root=$(cd -- "${proto_dir}/.." && pwd)
build_dir="${repo_root}/build/proto"
build_jobs="${AUTOVIZ_BUILD_JOBS:-4}"

if [[ ! -f "${proto_dir}/CMakeLists.txt" ]]; then
    echo "ERROR: 未找到 AutoVizProto/CMakeLists.txt: ${proto_dir}" >&2
    exit 1
fi

cmake -S "${proto_dir}" -B "${build_dir}" \
  -DAUTOVIZ_PROTO_BUILD_TESTS=ON
cmake --build "${build_dir}" --parallel "${build_jobs}"
"${build_dir}/autoviz_proto_tests"

cmake --install "${build_dir}" \
  --prefix "${repo_root}/AutoVizClient/third_party/AutoVizProto"
cmake --install "${build_dir}" \
  --prefix "${repo_root}/AutoVizServer/third_party/AutoVizProto"

echo "AutoVizProto SDK installed for AutoVizClient and AutoVizServer."
