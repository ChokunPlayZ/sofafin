# JF Gamescope

A native, controller-first Jellyfin client for Steam Deck, with a Steam Big Picture-inspired interface. C++20, Qt 6 Quick, SDL2 input, and embedded libmpv video. Qt renders the interface directly; there is no browser or webview.

This is an early working foundation. It needs validation on a physical Deck and a reachable Jellyfin server before a release.

## Build and run

Requires CMake 3.22+, a C++20 compiler, Qt 6.5+ (Quick, Quick Controls 2, Network, OpenGL, Test), SDL 2.0.14+, and libmpv client API 2.0+. `ffmpeg` generates the local playback test fixture; the `mpv` executable is used by the optional live server check.

On Arch/CachyOS, the development packages are `base-devel cmake ninja qt6-base qt6-declarative sdl2 mpv ffmpeg`. On Debian/Ubuntu they are `build-essential cmake ninja-build pkg-config qt6-base-dev qt6-declarative-dev qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-templates qml6-module-qtqml-workerscript libsdl2-dev libmpv-dev mpv ffmpeg`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
./build/jf-gamescope --windowed
```

Enter the server's full base URL, username, and password. A controller-operated keyboard is available by pressing A in an input field. Keyboard and mouse also work.

For Steam Gaming Mode, add the built `jf-gamescope` executable as a non-Steam game, use `--fullscreen` in its launch options, and select a normal **Gamepad** Steam Input template. If Qt selects the wrong display platform, use `QT_QPA_PLATFORM=xcb %command% --fullscreen` as the Steam launch options.

For nested gamescope on a Linux desktop:

```sh
gamescope -w 1280 -h 800 -W 1280 -H 800 -f -- env QT_QPA_PLATFORM=xcb ./build/jf-gamescope --fullscreen
```

Installation is optional: `cmake --install build --prefix ~/.local` installs the executable, desktop entry, and icon. This does not bundle runtime libraries. SteamOS packaging (Flatpak/AppImage) is still pending; avoid modifying the Deck's read-only system to install development packages.

## Controls

| Controller | Keyboard | Action |
| --- | --- | --- |
| D-pad / left stick | Arrow keys | Move focus; seek ±10 seconds during playback |
| A | Enter | Select; open keyboard in a field; pause in the player |
| B | Escape | Back; close a dialog; stop playback |
| X | Space | Pause in the player |
| Y | F | Search |
| LB / RB | Page Up / Page Down | Home / Movies / Shows |
| Menu | F1 | Options |
| — | F11 | Toggle fullscreen |

Press Down in the player to reach audio and subtitle controls, and Up to return to the player. The controller can be disconnected and reconnected without restarting.

## Implemented

- Server sign-in, optional remembered sessions, and sign-out.
- Home shelves for continue watching, next episodes, and recently added movies.
- Library/folder browsing, Movies and Shows catalogs, pagination, search, and title details.
- Direct play over authenticated HTTP and Jellyfin-negotiated HLS transcoding, selected using a libmpv device profile.
- Resume, pause, seeking, embedded and external subtitle tracks, and audio track cycling.
- Playback start/progress/stop reporting and cleanup of server transcoding jobs.
- Automatic / direct-only / forced-transcoding modes, bitrate limits, hardware decoding, fullscreen, poster size, and accent colors.

Direct play is preferred across the media versions Jellyfin returns. Direct-only mode shows an error if none is playable. Automatic mode permits server transcoding; forced-transcoding mode disables video/audio stream copying. Transcoded seeking requests a fresh stream at the target position so playback progress remains on the source timeline. Playback setting changes apply to the next stream.

Authentication and stream negotiation use Qt Network with verified TLS and same-origin API redirects. Playback and external subtitle URLs are restricted to the server's origin. Passwords are never saved. If **Keep me signed in** is selected, the access token is stored in the user's `JF-Gamescope/client.ini` settings file with owner-only permissions; it is not encrypted. Sign out clears that token and ends the server session. Treat media URLs containing tokens as credentials when sharing logs.

## Validation

```sh
ctest --test-dir build --output-on-failure
```

Tests cover playback policy, media-version selection, base paths, token handling, malformed responses, expired/stale sessions, controller-style UI navigation, the on-screen keyboard, browsing/search/pagination, and actual libmpv decoding/pause/seek/resume of a generated local clip. The player test uses libmpv's software render API to run without a display; the application uses the OpenGL render API. The loopback HTTP test reports a skip if the execution environment forbids sockets.

To exercise a real server, provide a disposable account through environment variables. The checker authenticates, browses libraries, negotiates direct play and forced transcoding, and asks mpv to decode a short sample in each mode (up to 30 frames). It requires at least one accessible movie/episode and server permission to transcode. Credentials and playback URLs are not printed.

```sh
export JF_SERVER='https://your-server.example'
read -r -p 'Username: ' JF_USERNAME
read -r -s -p 'Password: ' JF_PASSWORD
export JF_USERNAME JF_PASSWORD
./build/jf-live-check
unset JF_SERVER JF_USERNAME JF_PASSWORD
```

For a headless screenshot of the sign-in screen:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/jf-gamescope --windowed --smoke-test --screenshot /tmp/jf-gamescope.png
```

Known limits: no live TV, downloads, Quick Connect, automatic next-episode playback, server-side selection of audio/subtitle tracks during transcoding, HDR certification, or bundled distribution yet. Track cycling during transcoding exposes the tracks present in the server's output. The actual OpenGL surface, hardware decoding, Gamescope integration, and physical controller mappings require hardware validation.

## Source layout

`src/JellyfinClient.*` owns the authenticated API transport. `src/Playback.*` builds device capabilities and chooses a negotiated source. `src/AppController.*` manages browsing and playback sessions. `src/MpvPlayer.*` owns libmpv and observes its events; `src/MpvVideo.*` renders into a Qt Quick OpenGL framebuffer. `src/ControllerInput.*` translates SDL gamepad events into native focus navigation. `qml/` contains the native interface.

Protocol/rendering references: [Jellyfin transcoding](https://jellyfin.org/docs/general/post-install/transcoding/), [Jellyfin PlaybackInfo controller](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/MediaInfoController.cs), [libmpv render API](https://github.com/mpv-player/mpv/blob/master/include/mpv/render.h), [Qt framebuffer integration](https://doc.qt.io/qt-6/qquickframebufferobject.html).

## Git in this workspace

This managed workspace exposes `.git` as read-only. Its repository metadata therefore lives in the ignored `.git-repository/` directory. Use the supplied wrapper here:

```sh
./scripts/git status
./scripts/git log --oneline
./scripts/git diff
```

The wrapper delegates directly to Git with an explicit work tree and metadata directory. Commit identity is repository-local `Codex <codex@localhost>`. When the normal `.git` path becomes writable, the metadata can be moved there and ordinary Git commands used. The workaround is specific to this managed workspace.
