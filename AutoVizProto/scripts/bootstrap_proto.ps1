[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$Generator = "Ninja",
    [string]$CMakePrefixPath = "",
    [string]$CxxCompiler = ""
)

$ErrorActionPreference = "Stop"

# Build the protocol SDK with the Windows Client toolchain. The ROS2 Server is
# built on Linux, so its SDK must be installed by bootstrap_proto.sh there.
# Paths are derived from this script, so it can run anywhere.
$ScriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProtoDirectory = Split-Path -Parent $ScriptDirectory
$ProjectDirectory = Split-Path -Parent $ProtoDirectory
$BuildDirectory = Join-Path $ProtoDirectory "build"
$ClientProtobufDirectory = Join-Path $ProjectDirectory "AutoVizClient\third_party\protobuf"
$ClientInstallDirectory = Join-Path $ProjectDirectory "AutoVizClient\third_party\AutoVizProto"
$ProtoCMakeLists = Join-Path $ProtoDirectory "CMakeLists.txt"

if (-not (Test-Path -LiteralPath $ProtoCMakeLists -PathType Leaf)) {
    throw "Cannot find AutoVizProto/CMakeLists.txt: $ProtoCMakeLists"
}

$ConfigureArguments = @(
    "-S", $ProtoDirectory,
    "-B", $BuildDirectory,
    "-G", $Generator,
    "-DCMAKE_BUILD_TYPE=$Configuration"
)
if (Test-Path -LiteralPath $ClientProtobufDirectory -PathType Container) {
    $ConfigureArguments += "-DCMAKE_PREFIX_PATH=$ClientProtobufDirectory"
}
elseif ($CMakePrefixPath) {
    $ConfigureArguments += "-DCMAKE_PREFIX_PATH=$CMakePrefixPath"
}

if ($CxxCompiler) {
    $ConfigureArguments += "-DCMAKE_CXX_COMPILER=$CxxCompiler"
}

Write-Host "===== CMake setup ====="
& cmake @ConfigureArguments
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed with exit code $LASTEXITCODE" }

Write-Host "===== Building $Configuration ====="
& cmake --build $BuildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed with exit code $LASTEXITCODE" }

Write-Host "===== Installing SDK for AutoVizClient ====="
& cmake --install $BuildDirectory --config $Configuration --prefix $ClientInstallDirectory
if ($LASTEXITCODE -ne 0) { throw "CMake install failed with exit code $LASTEXITCODE" }

Write-Host "AutoVizProto SDK installed for AutoVizClient."
