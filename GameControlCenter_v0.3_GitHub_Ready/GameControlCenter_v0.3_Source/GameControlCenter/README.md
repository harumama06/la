# Game Control Center v0.3 — Windows

Windows C++20 / Qt 6 Widgets / CMake / vcpkg project. This version focuses on functional CS2 profile and audio controls.

## Implemented in this reviewed source

- CS2 process monitoring and Steam URI launch.
- Two Windows display-mode preferences: **in-game mode** and **after-CS2-exit mode**, each including resolution and refresh rate.
- Launch flow: snapshot current settings, apply the saved CS2 profile, apply the selected in-game Windows display mode, launch Steam/CS2, and restore settings after CS2 exits.
- Default playback and microphone device selection.
- Playback endpoint volume and microphone input volume/mute.
- Active Windows audio-session listing and per-process session volume sliders (0–100%).
- Register custom `.exe` paths, launch a registered app, and save a per-app volume rule. GCC polls active sessions and reapplies the saved rule when a matching process audio session appears.
- Custom application paths and volume rules are included in profiles.
- Windows Volume Mixer shortcut for per-app output routing.
- JSON profiles, system tray, NVIDIA Share detection and Alt+Z / Alt+F9 hotkeys.

## Important audio limitations

- Windows Core Audio's standard per-application session volume API is limited to 0–100%. Genuine per-app gain above 100% is **not implemented**.
- Per-app output-device routing is managed by Windows Volume Mixer; GCC provides a shortcut.
- GCC enumerates audio sessions on the default render endpoint. Apps routed to a different endpoint may not appear in the session table.

## Build in Visual Studio 2026

1. Install **Desktop development with C++**, MSVC x64 tools, Windows SDK, and CMake tools.
2. Set `VCPKG_ROOT` to your vcpkg folder.
3. Open this project folder in Visual Studio 2026 as a local folder.
4. Configure the `vs2026-local` preset and build Release.

The project manifest uses Qt 6 Widgets. The first vcpkg configure can take a while.

## GitHub Actions

The workflow is stored at the repository root in `.github/workflows/windows-build.yml`. The project source is nested at `GameControlCenter_v0.3_Source/GameControlCenter/`. See `BUILD_ON_GITHUB.md` for the exact run and artifact steps.

## Notes

- Setting a default audio endpoint uses Windows' undocumented `IPolicyConfig` interface. If Windows rejects it, select the device in Windows Sound Settings.
- Auto-restore snapshots are held in memory. Keep GCC running while CS2 is running.
- Display changes may briefly blank the monitor. Select a mode supported by the display.
