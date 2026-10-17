# Configuration reference

Open Settings with Menu / Start, F1, or the profile image at the top right. The
settings panel scrolls and keeps focused controls visible at larger UI scales.

## Preferences

| Setting | Default | Values and effect |
| --- | --- | --- |
| Playback | Automatic | Automatic prefers direct play and permits transcoding; Direct only rejects unplayable sources; Transcode forces server conversion |
| Streaming limit | 120 Mbps | UI cycles 4, 8, 20, 40, 80, 120 Mbps; sent in playback negotiation |
| Hardware decoding | Automatic | Linux tries VA-API/NVDEC, then software; Off uses software |
| Theme songs | On | Play server-provided themes on title details |
| Theme volume | 70% | 0–100%, slider or Left/Right in 5% steps; applies immediately |
| Fullscreen | On | Changes the window immediately; saved for the next launch |
| UI scale | 100% | 75–150% in 5% steps; scales the entire interface, not video resolution |
| Accent | Steam blue | Steam blue (`#66c0f4`), Sea glass (`#76dfba`), Warm amber (`#f3bb70`) |

Global streaming and decoding changes apply to the next stream. The player's
quality/audio/subtitle selections apply immediately, preserving position and pause
state. Detail-page playback settings apply when starting that title. See
[playback](playback.md) for quality precedence and caching limits.

## Settings file

Qt stores INI settings at `$XDG_CONFIG_HOME/Sofafin/client.ini`, normally
`~/.config/Sofafin/client.ini` on Linux. Set `XDG_CONFIG_HOME` to an absolute
directory to isolate an installation or test:

```sh
XDG_CONFIG_HOME=/tmp/sofafin-preview ./build/sofafin --windowed
```

This starts a separate configuration, including a separate generated device ID.
The application attempts owner-only read/write permissions on settings writes.
Remembered tokens are not encrypted. Never attach this file to a public issue.

Prefer the UI when editing settings. Close the application before editing the
file manually; INI values are Qt-serialized and changes can otherwise be overwritten.

| Key | Meaning / stored value |
| --- | --- |
| `fullscreen` | Boolean, default `true` |
| `hardwareDecoding` | Boolean, default `true` |
| `themeSongs` | Boolean, default `true` |
| `themeSongVolume` | Integer, 0–100, default 70 |
| `uiScale` | Factor, 0.75–1.5, default 1.0 |
| `bitrate` | Bits/second, default 120000000; setter accepts 1000000–1000000000 |
| `playbackMode` | `auto`, `direct`, or `transcode`; default `auto` |
| `accent` | One of the three hex colors above |
| `deviceId` | Generated UUID used to identify this client to Jellyfin |
| `server`, `username` | Most recently successful server URL and account name |
| `accessToken`, `userId` | Present for a remembered session; removed on sign-out |

Passwords are never written to settings. Sign-out clears the saved token and user
ID, while retaining the server, username, preferences, and device ID. Only one
server/account session is stored. See [migration](installation.md#upgrade-and-migration)
and [privacy](privacy.md).

## Command-line options

```sh
./build/sofafin --help
./build/sofafin --version
```

| Flag | Behavior |
| --- | --- |
| `-h`, `--help` | Display command-line help |
| `--help-all` | Include Qt-specific help options |
| `-v`, `--version` | Display the version generated from CMake |
| `--windowed` | Start windowed, overriding the saved fullscreen preference |
| `--fullscreen` | Start fullscreen, unless `--windowed` is also supplied |
| `--smoke-test` | Load the interface, skip session restoration, and exit after about two seconds |
| `--screenshot PATH` | Save a window screenshot at smoke-test exit; use with `--smoke-test` |

The two launch-mode flags do not change the saved preference. Screenshot failure
returns exit code 2; application initialization failure returns 1. Qt consumes
some of its own options before Sofafin's parser runs.

## Environment variables

| Variable | Used by | Purpose |
| --- | --- | --- |
| `XDG_CONFIG_HOME` | Application / Qt | Override the local configuration root |
| `QT_QPA_PLATFORM` | Qt | Select platform, e.g. `xcb`, `wayland`, or `offscreen` |
| `QT_QUICK_BACKEND` | Qt / Sofafin | `software` supports headless UI checks; normal playback uses OpenGL |
| `QT_DEBUG_PLUGINS` | Qt | `1` logs plugin diagnostics; useful for startup problems |
| `SDL_GAMECONTROLLERCONFIG` | SDL2 | Additional SDL controller mappings, if needed |
| `SOFAFIN_SERVER` | Live test executables | Full Jellyfin base URL |
| `SOFAFIN_USERNAME`, `SOFAFIN_PASSWORD` | Live test executables | Disposable test account credentials |
| `SOFAFIN_TEST_HEIGHT` | Live UI runner | 1080, 1440, or 2160; runner defaults to 1080 |
| `SOFAFIN_SCREENSHOT_DIR` | Live UI runner | Output directory for screenshots |
| `SOFAFIN_THEME_TEST_TITLE` | Live UI runner | Known series title for an optional theme continuity assertion |

`SOFAFIN_*` live-test credentials do not sign the normal application in. The app
uses its sign-in form or saved session. Qt and SDL support additional upstream
environment variables; the table lists the project's documented entry points.

The native test runner also supports developer screenshot variables documented
in [testing](testing.md#screenshots). Sofafin disables libmpv's user configuration,
default input bindings, and on-screen controller; `mpv.conf` does not configure it.
