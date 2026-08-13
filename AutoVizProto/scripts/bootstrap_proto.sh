#!/usr/bin/env bash

set -euo pipefail

# 构建唯一的协议 SDK，并安装到两个消费工程自己的 third_party 目录。
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PROTO_DIR=$(cd -- "${SCRIPT_DIR}/.." && pwd)
PROJECT_DIR=$(cd -- "${PROTO_DIR}/.." && pwd)
BUILD_DIR="${PROTO_DIR}/build"

if [[ ! -f "${PROTO_DIR}/CMakeLists.txt" ]]; then
    echo "ERROR: can't find AutoVizProto/CMakeLists.txt: ${PROTO_DIR}" >&2
    exit 1
fi

echo "===== CMake setup ====="
cmake -S "${PROTO_DIR}" -B "${BUILD_DIR}"

echo "===== Building ====="
cmake --build "${BUILD_DIR}" -j$(nproc)

cmake --install "${BUILD_DIR}" \
  --prefix "${PROJECT_DIR}/AutoVizClient/third_party/AutoVizProto"
cmake --install "${BUILD_DIR}" \
  --prefix "${PROJECT_DIR}/AutoVizServer/third_party/AutoVizProto"

echo "AutoVizProto SDK installed for AutoVizClient and AutoVizServer."
