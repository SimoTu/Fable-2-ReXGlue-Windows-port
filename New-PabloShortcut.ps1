[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$desktop = [Environment]::GetFolderPath('Desktop')
$shortcutPath = Join-Path $desktop 'Pablo.lnk'
if (Test-Path -LiteralPath $shortcutPath) {
    throw "A shortcut already exists at $shortcutPath."
}
$executable = Join-Path $PSScriptRoot 'out\build\win-amd64-release\pablo-native.exe'
if (!(Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw 'Build the win-amd64-release preset before creating the shortcut.'
}
$userData = Join-Path (Split-Path $PSScriptRoot -Parent) 'pablo-user'
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $executable
$shortcut.Arguments = '--gpu_plugin=xenos --input_backend=sdl --no-mnk_mode --no-fullscreen "--user_data_root={0}"' -f $userData
$shortcut.WorkingDirectory = Split-Path $executable -Parent
$shortcut.IconLocation = "$executable,0"
$shortcut.Description = 'Launch Pablo in a window'
$shortcut.Save()
Get-Item -LiteralPath $shortcutPath
