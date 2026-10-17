# swave-imirror

AirPlay Screen Mirroring receiver for Windows 10 and 11, built with C++ and Qt6.

## Features

- High performance iOS screen mirroring over Wi-Fi
- Configurable discovery name visible in iOS Control Center
- Target frame rate selector (30 FPS / 60 FPS)
- Latency profiles: Stable, Balanced, and Low Latency
- Modern native desktop interface with system tray controls
- Direct3D 11 and Direct3D 12 video renderer support
- Autostart on Windows login option
- Integrated Bluetooth LE discovery beacon support

## Prerequisites

For building from source:
- Windows 10 or Windows 11 (x64 / ARM64)
- MSYS2 (UCRT64 environment for x64)
- Qt 6 (Core, Gui, Widgets)
- GStreamer 1.0 runtime & development packages
- Python 3.11+ for Windows (for WinRT Bluetooth beacon build)

## Building

```powershell
# Build and package portable bundle (x64)
.\build.ps1 package -Architecture x64 -SkipInstaller
```

Build outputs will be generated under `out/x64/artifacts/`.

## Configuration

Settings are saved in:
```text
%APPDATA%\Swave\swave-imirror\arguments.txt
```

Machine-wide default fallback:
```text
%ProgramData%\swave-imirror\arguments.txt
```

## License

Please see [LICENSE](./docs/LICENSE.rtf) for details.
