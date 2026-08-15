[CmdletBinding()]
param(
    [ValidateRange(1, 1024)]
    [int]$Parallel = [Environment]::ProcessorCount,
    [switch]$Run
)

$ErrorActionPreference = 'Stop'

$ScriptDir = $PSScriptRoot
$ClientDir = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $ClientDir 'build'
$PackageDir = Join-Path $ClientDir 'package\AutoViz-MinGW'
$CacheFile = Join-Path $BuildDir 'CMakeCache.txt'

if (-not (Test-Path -LiteralPath $CacheFile -PathType Leaf)) {
    throw "未找到已配置的 Client 构建目录：$BuildDir`n请先在 CLion/Qt Creator 中配置并完成 Release 构建，或按 README 使用 MinGW Makefiles 配置该目录。"
}

cmake --build $BuildDir --config Release --parallel $Parallel
if ($LASTEXITCODE -ne 0) {
    throw "Client Release 构建失败，退出码：$LASTEXITCODE"
}

cmake -E remove_directory $PackageDir
if ($LASTEXITCODE -ne 0) {
    throw "无法清理旧发布目录：$PackageDir"
}

cmake --install $BuildDir --config Release --prefix $PackageDir
if ($LASTEXITCODE -ne 0) {
    throw "Client 安装/打包失败，退出码：$LASTEXITCODE"
}

Write-Host "发布包已生成：$PackageDir"
if ($Run) {
    & (Join-Path $PackageDir 'bin\AutoViz.exe')
}
