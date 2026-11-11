# Obsidian Launcher

A Minecraft Java Edition launcher for Windows, built with C++17 and Qt 6 Widgets.

Obsidian uses Minecraft versions and Java runtimes that are already installed on your PC. It does not download Minecraft, Java, or mod loaders.

## Download

The current release is **Obsidian Launcher 2.3 Beta**.

[Download the Windows installer](https://github.com/W00ky02/Obsidian-Launcher/releases/download/v2.3.0-beta.1/Obsidian-Beta-v2.3-setup.exe) · [All releases](https://github.com/W00ky02/Obsidian-Launcher/releases)

Windows 10 or 11, 64-bit, is required. The installer includes the launcher dependencies. Minecraft and a compatible Java runtime must already be installed.

This is the existing 2.3 build, published as the first GitHub beta. The installer has been renamed for the release; its contents have not changed. The launcher still reports version 2.3.

## Features

- Browse installed Vanilla, Fabric, Forge, NeoForge, and Quilt profiles, with search, filters, and favorites.
- Sign in with a Minecraft Services access token. Obsidian retrieves your profile name and UUID automatically.
- Play offline singleplayer with a local name.
- View live game logs in a separate console, with search, filters, and export.
- Set memory limits, resolution, fullscreen mode, and launch behavior.
- Choose English or German, light or dark themes, accent colors, fonts, spacing, navigation width, and optional animations.
- Show launcher and game activity in Discord Rich Presence, with a switch to turn it off.
- Open your mods, resource packs, screenshots, and worlds folders from the launcher.

All profiles use `%APPDATA%\.minecraft`, so they share mods, worlds, and settings.

## Signing in

**Minecraft access token:** Enter a valid Minecraft Services access token on the account page. Obsidian checks it against the Minecraft profile service and fills in your profile details. Microsoft, Xbox, and Discord tokens cannot be used here. Sessions last no more than about an hour and may expire sooner; there is no token refresh.

**Offline singleplayer:** Choose the offline method and enter a name using 1–16 ASCII letters, numbers, or underscores. No account request is made. Servers that verify accounts still require a valid login. Your name, including capitalization, determines your offline identity; changing it can affect inventory, progress, and pet ownership in existing worlds.

**Microsoft account:** The sign-in flow is implemented, but the released build has no registered Microsoft client ID and this option is not ready to use. See [Microsoft sign-in](docs/MICROSOFT_LOGIN.md).

Tokens, profile details, and offline names are kept in memory and are not saved by Obsidian. Settings are stored in `%APPDATA%\ObsidianLauncher\preferences.ini`. Minecraft receives its access token as a process argument; the game and mods can write their own logs outside Obsidian's control.

## Beta notes

- Every Minecraft profile must be fully installed locally. Missing files are reported rather than downloaded.
- Discord must be running and allow activity sharing for Rich Presence to appear.
- The installer is unsigned. A full installation or upgrade has not been verified in the development environment.
- Existing automated tests use synthetic profiles, mock Java, and local authentication and Discord test servers. Real Minecraft launches, real account sign-in, and real Discord activity have not been verified in that environment.
- Automatic updates are not included in 2.3. For now, download and run the installer from Releases.

See [launch behavior](docs/LAUNCH_BEHAVIOR.md) for details and limitations. If something goes wrong, [open an issue](https://github.com/W00ky02/Obsidian-Launcher/issues) with your Windows version, Minecraft profile, and steps to reproduce it. Remove tokens and personal information before sharing logs.

## Build from source

You need Visual Studio 2022 with Desktop development with C++, CMake 3.24 or later, and a matching Qt 6 MSVC x64 kit. Qt 6.8.3 is recommended; include Qt Test to run the tests.

In Developer PowerShell for VS 2022:

```powershell
.\build.ps1 -QtPath 'C:\Qt\6.8.3\msvc2022_64'
.\dist\ObsidianLauncher.exe
```

Or run the steps separately:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
$env:PATH = 'C:\Qt\6.8.3\msvc2022_64\bin;' + $env:PATH
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
```

CMake downloads the pinned zlib 1.3.1 source archive on the first configuration. For an offline build, provide an extracted copy with `-DFETCHCONTENT_SOURCE_DIR_ZLIB=C:/deps/zlib-1.3.1`.

Distribute the whole `dist` folder, including Qt libraries, plugins, licenses, and the required Microsoft Visual C++ runtime. See [installer instructions](installer/README.md) and [release instructions](docs/RELEASING.md).

## Dependencies

Obsidian uses Qt 6 and zlib. Third-party license texts are included in `licenses`; see [third-party notices](THIRD_PARTY_NOTICES.md).

This is an independent project, not an official Minecraft product. It is not affiliated with Mojang or Microsoft.
