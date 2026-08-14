#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
client_dir="$(cd -- "${script_dir}/.." && pwd)"
build_dir="${client_dir}/build"
package_dir="${client_dir}/package/AutoViz-Linux"

if [[ ! -f "${build_dir}/CMakeCache.txt" ]]; then
    echo "未找到已配置的构建目录：${build_dir}" >&2
    echo "请先在仓库根目录执行：" >&2
    echo "  cmake -S AutoVizClient -B AutoVizClient/build -DCMAKE_BUILD_TYPE=Release" >&2
    exit 1
fi

cmake --build "${build_dir}" --parallel
cmake -E remove_directory "${package_dir}"
cmake --install "${build_dir}" --prefix "${package_dir}"

echo "发布包已生成：${package_dir}"
echo "启动命令：${client_dir}/scripts/AutoViz.sh"
