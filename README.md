# Sofafin

<img src="assets/logo.svg" alt="Sofafin icon" width="128" height="128">

A native, controller-first Jellyfin client for Linux, designed for Steam Deck and
gamescope. Browse movies and shows, resume watching, and play video through
embedded libmpv. C++20, Qt 6 Quick, and SDL2; the interface runs without a browser.

**Status: early preview, targeting 0.1.0.** Desktop and headless tests exist;
physical Steam Deck, Steam keyboard, native GPU rendering, and Gamescope checks
remain release gates. No Flatpak, AppImage, or bundled binary is provided yet.
See [known limitations](docs/README.md#support-and-limitations) before installing.

## Features

- Server sign-in, optional remembered sessions, profile images, and sign-out.
- Home shelves, server library tabs, search, folders, pagination, filters, and sorting.
- Full-screen movie, series, and episode details with artwork, cast, chapters,
  favorites, watched status, and optional theme songs.
- Direct play or Jellyfin HLS transcoding; quality, audio, subtitle, and version
  selection; resume, seek previews, trickplay, chapter markers, and skip segments.
- Automatic next episodes, with prebuffering once the current episode is fully cached.
- Gamepad, keyboard, and pointer navigation; scalable UI, fullscreen, and accent colors.

Sofafin needs your own Jellyfin server and account. It does not provide media and
is an independent project, unaffiliated with Jellyfin or Valve.

## Quick start

Install the [build dependencies](docs/installation.md#dependencies), then run from
an extracted source release or a checkout:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/sofafin --windowed
```

Enter your server's full base URL, username, and password. A / F2 opens Steam's
keyboard in a focused field when Steam is running; a physical keyboard also works.
Remembered access tokens are stored locally, unencrypted, with owner-only file
permissions. Passwords are not saved. See [privacy](docs/privacy.md).

Optional user installation:

```sh
cmake --install build --prefix "$HOME/.local"
```

This installs the executable, desktop entry, icon, and documentation; runtime
libraries still come from your system. See [installation and upgrades](docs/installation.md).
For Gaming Mode, add Sofafin to Steam as a non-Steam game, choose a **Gamepad**
Steam Input template, and use `--fullscreen`. See [Steam Deck setup](docs/installation.md#steam-deck-and-gaming-mode).

## Controls

| Controller | Keyboard | Action |
| --- | --- | --- |
| D-pad / left stick | Arrows | Move focus; preview a seek in the player |
| A | Enter | Select; pause when the video has focus |
| A in a text field | F2 | Open Steam keyboard |
| B | Escape | Back, dismiss controls, cancel a seek, or confirm stopping |
| X | Space | Pause / resume |
| Y | F | Search while browsing |
| LB / RB | Page Up / Page Down | Cycle Home and library tabs |
| Menu / Start | F1 | Settings |
| View / Back | I | Media info during playback |
| — | F11 | Toggle fullscreen |

See the [user guide](docs/usage.md) for player focus, seek confirmation, autoplay,
and stop confirmation behavior.

## Documentation

Start at the [documentation index](docs/README.md).

| Topic | Guide |
| --- | --- |
| Dependencies, building, installing, Gaming Mode, upgrades, removal | [Installation](docs/installation.md) |
| Sign-in, browsing, controls, playback, episodes | [User guide](docs/usage.md) |
| Settings, defaults, command-line flags, environment variables | [Configuration](docs/configuration.md) |
| Direct play, transcoding, cache, tracks, segments, hardware decoding | [Playback](docs/playback.md) |
| Build, login, display, input, and playback problems | [Troubleshooting](docs/troubleshooting.md) |
| Local data, credentials, and network behavior | [Privacy](docs/privacy.md) |
| Code structure and Jellyfin integration | [Architecture](docs/architecture.md) |
| Automated, live-server, and hardware checks | [Testing](docs/testing.md) |
| Versioning, source archives, release checklist | [Releasing](docs/releasing.md) |

## Contributing and support

Read [CONTRIBUTING.md](CONTRIBUTING.md) before sending a patch. Use the repository's
issue tracker for reproducible bugs and feature requests. Redact credentials and
media URLs from logs. Report vulnerabilities as described in [SECURITY.md](SECURITY.md).

See [CHANGELOG.md](CHANGELOG.md) for release notes, [BACKLOG.md](BACKLOG.md) for
planned work, and [THIRD_PARTY.md](THIRD_PARTY.md) for dependency acknowledgments.

## License

Copyright © 2026 Sofafin contributors. Original code, documentation, and bundled
artwork are licensed under the **GNU General Public License v3.0 or later**
(`GPL-3.0-or-later`). See [LICENSE](LICENSE) for the full terms. Dependencies retain
their own licenses; see [THIRD_PARTY.md](THIRD_PARTY.md).
