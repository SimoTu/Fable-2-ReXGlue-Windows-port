# Pablo Windows recompilation

Users must bring their own copy of the Xbox 360 game. The disc image, XEX, game data and generated recompiled C++ are excluded from the source repository and releases.

The Windows build uses ReXGlue SDK v0.10.0 and its Direct3D 12, kernel and audio implementations.

## Test with a DualSense

Connect the controller to Windows over USB or Bluetooth before launching. The
test launcher selects ReXGlue's SDL controller backend. The controller acts as
the guest Xbox 360 pad. Cross maps to A, Circle to B, Square to X, Triangle to Y,
and Options to Start.

From the project directory, point the launcher at the folder containing your
own extracted `default.xex` and game data.

```powershell
.\Launch-Pablo.ps1 -GameRoot 'D:\MyDump'
```

If several controllers are present, pass `-DualSenseOnly` to keep other pads out of the guest controller slots during the test.

## Record and replay controller input

Set one of these environment variables before launching the game. Use a
dedicated test profile for both runs, starting from the same save state.

- `PABLO_INPUT_RECORD` points to a new trace file. Recording uses the first SDL
  controller and stores changes to its buttons, triggers, and sticks as the
  game polls them.
- `PABLO_INPUT_REPLAY` points to an existing trace. The replay pad supplies
  those states on the recorded timeline, measured from the first guest poll.

For example, set the variable in the same PowerShell session that runs the
launcher, then remove it when finished.

```powershell
$env:PABLO_INPUT_RECORD = 'D:\MyTests\new-game.input.txt'
.\Launch-Pablo.ps1 -GameRoot 'D:\MyDump' -DualSenseOnly -UserDataRoot 'D:\MyTests\record-profile'
Remove-Item Env:PABLO_INPUT_RECORD
```

Use `PABLO_INPUT_REPLAY` in place of `PABLO_INPUT_RECORD` for playback, and point `-UserDataRoot` at a copy of the pre-recording profile. The
trace does not restore saves or record game state. SDL keyboard events and
controller connections are outside the trace. To record the scripted test
pad instead, set `PABLO_SMOKE_TEST_INPUT` to its command file while recording.

## Gameplay frame rate

Pablo defaults to a 120 Hz guest video mode. In the opening gameplay replay on
an AMD Radeon RX 9070, this produced 60 guest swaps per second. It uses the
existing ReXGlue D3D12 backend and keeps vertical sync enabled. To override
this setting, pass `--video_mode_refresh_rate=60` to the executable or set
`REX_VIDEO_MODE_REFRESH_RATE=60` before running the PowerShell launcher.

To measure guest swap intervals with the patched SDK, set
`REXGLUE_GUEST_FRAME_TRACE` to a new CSV path before launch. The two columns
record microseconds since the first guest swap and microseconds since the
previous swap. Quit normally to flush the last buffered rows.
For a DXGI cross-check, `REXGLUE_DXGI_FRAME_TRACE` records the latest observed
present ID, its refresh index, the synchronization refresh index, and its
QPC timestamp. The presenter can repaint the same guest frame between swaps.
Use the guest trace to count new game frames.

## Build on Windows

Use the x64 Visual Studio C++ Build Tools, Windows SDK, Clang, CMake and Ninja.
From the repository folder, clone and build the pinned SDK in a sibling folder.

```powershell
git clone --branch v0.10.0 --recurse-submodules https://github.com/rexglue/rexglue-sdk.git ../rexglue-sdk
git -C ../rexglue-sdk apply (Join-Path $PWD 'rexglue-sdk.patch')
Push-Location ../rexglue-sdk
cmake --preset win-amd64
cmake --build out/build/win-amd64 --config Release
cmake --install out/build/win-amd64 --config Release
Pop-Location
.\Build-Pablo.ps1 -SdkPrefix '../rexglue-sdk/out/install/win-amd64' -GameRoot 'D:\MyDump'
```

Code generation requires your extracted `default.xex`. At runtime, launch
without arguments to choose your ISO, or supply `-IsoPath 'D:\MyDisc.iso'`.
The picker remembers the image after its XEX loads. Pass `-SelectIso` to choose
again. The executable reads the image directly through ReXGlue's disc device.

Use `-Windowed` to start in a resizable window, **F11** to toggle fullscreen,
and **F4** to open in-game settings. **Save to config** persists settings.
Run `New-PabloShortcut.ps1` to add a windowed shortcut to your desktop.

## License

Project source is licensed under GPL-2.0-or-later. The ReXGlue patch applies
to the separately licensed SDK. Its BSD 3-Clause notice is in `LICENSE-ReXGlue`.
Releases contain source only.
