[CmdletBinding(DefaultParameterSetName = 'Launch')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Folder')]
    [string] $GameRoot,

    [Parameter(Mandatory, ParameterSetName = 'Iso')]
    [string] $IsoPath,

    [Parameter(Mandatory, ParameterSetName = 'Select')]
    [switch] $SelectIso,

    [switch] $Windowed,
    [switch] $DualSenseOnly,
    [string] $UserDataRoot
)

$ErrorActionPreference = 'Stop'
$executable = Join-Path $PSScriptRoot 'out\build\win-amd64-release\pablo-native.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw 'Build the win-amd64-release preset before launching Pablo.'
}

$projectRoot = Split-Path $PSScriptRoot -Parent
$userData = if ($UserDataRoot) {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($UserDataRoot)
} else {
    Join-Path $projectRoot 'pablo-user'
}

$game = $null
if ($IsoPath) {
    $game = Get-Item -LiteralPath $IsoPath
    if ($game.PSIsContainer -or $game.Extension -ine '.iso') {
        throw 'Select an Xbox 360 disc image with the .iso extension.'
    }
} elseif ($GameRoot) {
    $game = Get-Item -LiteralPath $GameRoot
    if (!$game.PSIsContainer -or
        !(Test-Path -LiteralPath (Join-Path $game.FullName 'default.xex') -PathType Leaf)) {
        throw 'GameRoot must be your extracted game directory containing default.xex.'
    }
}

$logFile = Join-Path $projectRoot ('pablo-controller-{0:yyyyMMdd-HHmmssfff}.log' -f (Get-Date))
$previousControllerFilter = $env:SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT
Write-Host "Game log: $logFile"
Push-Location (Split-Path $executable -Parent)
try {
    if ($DualSenseOnly) {
        $env:SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT = '0x054C/0x0CE6,0x054C/0x0DF2'
    }
    $gameArguments = @(
        '--gpu_plugin=xenos'
        '--input_backend=sdl'
        '--no-mnk_mode'
        ('"--user_data_root={0}"' -f $userData)
        ('"--log_file={0}"' -f $logFile)
        '--log_level=debug'
        '--log_flush_interval=1'
    )
    if ($game) { $gameArguments += ('"--game_data_root={0}"' -f $game.FullName) }
    if ($SelectIso) { $gameArguments += '--pablo_select_iso' }
    if ($Windowed) { $gameArguments += '--no-fullscreen' }
    $gameProcess = Start-Process -FilePath $executable -ArgumentList $gameArguments -PassThru
    $gameProcess.WaitForExit()
    exit $gameProcess.ExitCode
} finally {
    $env:SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT = $previousControllerFilter
    Pop-Location
}
