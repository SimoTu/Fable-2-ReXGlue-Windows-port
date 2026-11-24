[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $GameRoot,
    [Parameter(Mandatory)] [string] $SdkPrefix
)
$ErrorActionPreference = 'Stop'
$game = (Resolve-Path -LiteralPath $GameRoot).ProviderPath.Replace('\', '/')
$sdk = (Resolve-Path -LiteralPath $SdkPrefix).ProviderPath
$rexglue = Join-Path $sdk 'bin\rexglue.exe'
if (!(Test-Path -LiteralPath "$game/default.xex" -PathType Leaf)) { throw 'GameRoot must contain default.xex.' }
if (!(Test-Path -LiteralPath $rexglue -PathType Leaf)) { throw 'SdkPrefix must be the patched ReXGlue install.' }
Push-Location $PSScriptRoot
try {
    $manifest = @"
[project]
name = "pablo"
sdk_version = "0.10.0"
game_root = $(ConvertTo-Json -InputObject $game -Compress)

[entrypoint]
file_path = $(ConvertTo-Json -InputObject "$game/default.xex" -Compress)
out_directory_path = "generated/default"
includes = ["startup_functions.toml"]
"@
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot 'pablo_manifest.toml'), $manifest)
    & $rexglue codegen pablo_manifest.toml
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
    cmake --preset win-amd64-release "-Drexglue_DIR=$sdk/lib/cmake/rexglue" -DREXSDK_VERSION=0.10.0
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
    cmake --build --preset win-amd64-release
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
