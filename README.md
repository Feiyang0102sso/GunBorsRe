# Gun Bros Re for Windows

[简体中文](README_CN.md)

`Gun Bros Re` is an unofficial remake of *Gun Bros* for Windows. The project recreates the game runtime using C++17, SDL3, and OpenGL, and includes both the game itself and a resource viewer.

The project currently supports Windows x64 only. Running the game requires the XGA BIG resources extracted from a legally owned copy of iOS version 3.6.0. HVGA BIG resources are not required.

## Disclaimer

This is an unofficial project created for learning, research, and preservation. It is not affiliated with, authorized by, or associated with the original game's developers, publishers, or any other rights holders.

*Gun Bros*, its related trademarks and names, and all original artwork, audio, and other game assets are the property of their respective owners. Users are responsible for complying with applicable local laws and license agreements.

This project is provided “as is,” without warranty of any kind. Use it at your own risk.

## Cheat Codes

Enter lowercase letters continuously while the game window is active; you do not need to press Enter. Do not hold `Shift`, `Ctrl`, `Alt`, or the `Windows` key while typing. The interval between adjacent characters must not exceed 2.5 seconds. Cheat codes are available in both Debug and Release builds.

### `ch`: Character and Progression

| Cheat code | Effect | Available in |
| --- | --- | --- |
| `chm` | Adds 500,000 coins and 500 Warbucks | Menus, combat |
| `chplo` | Adds 500 Xplodium | Menus, combat |
| `cht` | Advances the daily reward timer by one day | Menus, combat |
| `chupdate` | Advances the challenge day by one day, updates BRO-OPS, and saves | Menus, combat |
| `chxp` | Advances to the starting XP of the next level; has no effect at the level cap | Menus, combat |
| `chlvmax` | Advances to the highest level defined by the BIG experience table | Menus, combat |
| `chw` | Unlocks every wave on the four official survival planets | Menus, combat |

`chw` changes wave progression only. It does not grant perfect-wave rewards or bypass a planet's level requirement.

### `st`: State and Combat

| Cheat code | Effect | Available in |
| --- | --- | --- |
| `std` | Toggles debug mode | Menus, combat |
| `stc` | Toggles the local online-menu adapter | Menus, combat |
| `stref` | Advances active refinery slots by one day | Menus, combat |
| `stlockref` | Unlocks or relocks available refinery slots | Menus, combat |
| `stboss` | Fast-forwards to the boss of the current survival stage | Survival combat |
| `stk` | Starts the death sequence for the current character | Combat |

`stboss` is unavailable while the game is paused, during a rescue, or while either character is playing an item animation. `stc` only enables the locally recreated online menus; it does not connect to the discontinued official servers.

### `bro`: Local Teammate

| Cheat code | Effect | Available in |
| --- | --- | --- |
| `brow` | Makes the local bot play the normal weapon-switch animation | Single-player friend mode, Live |
| `brok` | Kills the local bot | Single-player friend mode, Live |
| `bror` | Revives the local bot | Single-player friend mode, Live |
| `bros` | Makes the local bot open the combat store for 10 seconds | Live |
| `brop` | Makes the local bot use a random available item | Live |

## Configuration File

The first time the game starts, it creates `GunBrosRe.cfg` in the same directory as `GunBrosRe.exe`. Common options can be changed in the startup dialog. You can also close the game and edit the file directly.

Configuration keys are case-sensitive. Section names in square brackets are used for organization only and do not affect how keys are read.

| Configuration key | Value | Default | Description |
| --- | --- | --- | --- |
| `Title` | UTF-8 text | `GunBroRe` | Game window title; quotation marks are not required |
| `StartDialog` | `0` or `1` | `1` | Whether to display the settings dialog at startup |
| `ScreenX` | `1`–`16384` | `1600` | Width of the window's client area |
| `ScreenY` | `1`–`16384` | `1200` | Height of the window's client area |
| `SoundVolume` | `0`–`10` | `3` | Background music volume; `0` is muted and `3` is close to the original volume |
| `EffectsVolume` | `0`–`10` | `3` | Sound effects volume; `0` is muted |
| `IsConnected` | `0` or `1` | `0` | Whether to enable the local online-menu adapter |
| `DMBotLevel` | `1`, `2`, or `3` | `1` | Deathmatch bot difficulty: easy, normal, or hard |
| `control` | `1` or `2` | `1` | Control style: screen-space mouse aiming or right-stick dragging |
| `DebugMode` | `0` or `1` | `0` | Whether to enable debug information and debug shortcuts |
| `DrawFPS` | `0` or `1` | `1` | Whether to display the FPS counter |

Example:

```ini
[common]
Title=GunBroRe
StartDialog=1
ScreenX=1600
ScreenY=1200

[audio]
SoundVolume=3
EffectsVolume=3

[game]
IsConnected=0
DMBotLevel=1

[control]
control=1

[debug]
DebugMode=0
DrawFPS=1
```

The following shortcuts are available when `DebugMode` is enabled:

| Shortcut | Function |
| --- | --- |
| `Shift+C` | Shows or hides collision outlines |
| `Shift+I` | Shows or hides runtime information |
| `Shift+M` | Opens the map browser |
| `Shift+T` | Opens the menu tutorial inspector |

In the map browser, use the arrow keys to select a map or change pages, press `Enter` to load it, and press `Esc` to return. Map previews use a copy of the current save and do not write changes back to the main save data.

## Building

### Requirements

- 64-bit Windows
- Visual Studio with the “Desktop development with C++” workload
- MSVC v145 platform toolset
- Windows 10 SDK

Project dependencies such as SDL3 and zlib are included in the repository and do not need to be downloaded separately.

### Runtime Resources

Before building, make sure the following directories exist in the repository root:

```text
big/       XGA BIG resources extracted from iOS version 3.6.0
assets/    Project-provided shaders, startup resources, audio, and icons
```

The project does not require HVGA BIG resources. If either directory is missing, the build will fail while copying runtime files.

### Using the Build Script

Run the following command from the repository root:

```bat
"vs2026 build.bat"
```

By default, the script builds both Debug and Release configurations. You can also build a single configuration:

```bat
"vs2026 build.bat" Debug
"vs2026 build.bat" Release
```

For automated use, pass `/nopause` to prevent the script from waiting for a key press:

```bat
"vs2026 build.bat" Release /nopause
```

The build script locates Visual Studio and MSBuild automatically. It primarily produces the following programs:

| Program | Purpose |
| --- | --- |
| `GunBrosRe.exe` | The game |
| `GunBrosViewer.exe` | BIG resource viewing and research tool |

Build outputs are written to `bin/Debug/` or `bin/Release/`. Build logs are written to `obj/build-Debug.log` or `obj/build-Release.log`.

## Roadmap and Frequently Asked Questions

### A Unified Engine?

No.

The BIG resources used by different versions of *Gun Bros* vary considerably. There are also significant structural differences between major releases, including Sections 31, 32, 33, and 35. Even resources with the same Section number can differ internally.

Related titles such as *MvM*, *Star Blitz*, and *Eternity Warrior* also contain many differences. Supporting all of these games and every version of *Gun Bros* in one engine would require extensive adaptation of resource formats and runtime logic. The effort would be disproportionate to the practical benefit, so this is not planned.

### Multiple Languages?

No.

The original engine theoretically supports multiple languages, but every known game release contains only one language. Regional localizations were generally produced and published separately by local studios rather than selected within a single release.

In addition, the game uses `fontbitmap` resources instead of TTF fonts. Producing a Chinese version would require adding and processing Chinese glyphs individually, which would involve a substantial amount of work. Multi-language configuration is therefore not currently planned.

### Documentation?

Not yet complete. Binary Templates are still missing for some `.bin` files, and the corresponding format descriptions and research documentation also need further work. These will be added gradually.

### Multiplayer?

True multiplayer is not currently supported.

The current implementation can only expose some menus that originally required an online connection and provide local bot matches. The original servers and their communication protocols have not been restored, and there is currently no practical way to revive the official multiplayer service.
