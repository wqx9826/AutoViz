#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

client_dir="$(cd -- "${script_dir}/.." && pwd)"

if [ -x "${script_dir}/AutoViz.bin" ]; then
    package_dir="${client_dir}"
    program="${script_dir}/AutoViz.bin"
else
    package_dir="${client_dir}/package/AutoViz-Linux"
    program="${package_dir}/bin/AutoViz.bin"
fi


if [ ! -x "${program}" ]; then
    echo "未找到发布程序：${program}" >&2
    echo "请先在 AutoVizClient 目录执行 ./scripts/package_linux.sh" >&2
    exit 1
fi

if [ -n "${LD_LIBRARY_PATH:-}" ]; then
    export LD_LIBRARY_PATH="${package_dir}/lib:${LD_LIBRARY_PATH}"
else
    export LD_LIBRARY_PATH="${package_dir}/lib"
fi
exec "${program}" "$@"
