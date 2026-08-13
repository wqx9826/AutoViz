[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

# Build the single protocol SDK with the Windows toolchain and install it for
# both consumers. Paths are derived from this script, so it can run anywhere.
$ScriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProtoDirectory = Split-Path -Parent $ScriptDirectory
$ProjectDirectory = Split-Path -Parent $ProtoDirectory
$BuildDirectory = Join-Path $ProtoDirectory "build"
$ClientProtobufDirectory = Join-Path $ProjectDirectory "AutoVizClient\third_party\protobuf"
$ClientInstallDirectory = Join-Path $ProjectDirectory "AutoVizClient\third_party\AutoVizProto"
$ServerInstallDirectory = Join-Path $ProjectDirectory "AutoVizServer\third_party\AutoVizProto"
$ProtoCMakeLists = Join-Path $ProtoDirectory "CMakeLists.txt"

if (-not (Test-Path -LiteralPath $ProtoCMakeLists -PathType Leaf)) {
    throw "Cannot find AutoVizProto/CMakeLists.txt: $ProtoCMakeLists"
}

$ConfigureArguments = @(
    "-S", $ProtoDirectory,
    "-B", $BuildDirectory,
    "-DCMAKE_BUILD_TYPE=$Configuration"
)
if (Test-Path -LiteralPath $ClientProtobufDirectory -PathType Container) {
    $ConfigureArguments += "-DCMAKE_PREFIX_PATH=$ClientProtobufDirectory"
}

Write-Host "===== CMake setup ====="
& cmake @ConfigureArguments
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed with exit code $LASTEXITCODE" }

Write-Host "===== Building $Configuration ====="
& cmake --build $BuildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed with exit code $LASTEXITCODE" }

foreach ($InstallDirectory in @($ClientInstallDirectory, $ServerInstallDirectory)) {
    & cmake --install $BuildDirectory --config $Configuration --prefix $InstallDirectory
    if ($LASTEXITCODE -ne 0) { throw "CMake install failed with exit code $LASTEXITCODE" }
}

Write-Host "AutoVizProto SDK installed for AutoVizClient and AutoVizServer."
