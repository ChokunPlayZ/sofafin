# Installation

## Requirements

Use a Linux desktop with an OpenGL-capable Qt environment, a reachable Jellyfin
server, and an account permitted to play the desired libraries. Server transcoding
permission is needed when direct play is unavailable. Steam is optional on desktop
and required for the integrated Steam keyboard.

| Component | Minimum / purpose |
| --- | --- |
| C++ compiler | C++20 support |
| CMake | 3.22 |
| Qt | 6.5; Core, Gui, Quick, Quick Controls 2, Network, OpenGL, D-Bus |
| Qt SVG plugin | Runtime rendering of the bundled SVG icons |
| SDL2 | 2.0.14; controller support |
| libmpv | pkg-config client API version 2.0 or newer |
| pkg-config | Library discovery |
| Ninja or Make | Build executor |

`mpv --version` is the player release version; `pkg-config --modversion mpv`
reports the client API package version checked by CMake. They use different numbers.
Qt Test, FFmpeg (with the `libx264` encoder), libavutil headers, and
`dbus-run-session` are also needed for the full Linux test suite. The `mpv`
executable is used only by the optional live-server checker.

## Dependencies

### Arch Linux / CachyOS

```sh
sudo pacman -S --needed base-devel cmake ninja pkgconf qt6-base qt6-declarative qt6-svg sdl2 mpv ffmpeg dbus
```

Install `qt6-wayland` as well to use Qt's native Wayland platform. X11/XWayland
is available through the Qt xcb platform.

### Debian 13 (trixie)

```sh
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-declarative-dev libqt6opengl6-dev libqt6svg6 \
  qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-templates \
  qml6-module-qtqml-workerscript libsdl2-dev libmpv-dev libavutil-dev \
  ffmpeg mpv dbus-daemon fonts-dejavu-core
```

Debian 13 provides [Qt 6.8 packages](https://packages.debian.org/trixie/qt6-base-dev)
and is the CI dependency baseline. For native Wayland, also install `qt6-wayland`.

Ubuntu and older Debian releases may provide Qt below 6.5. Check the installed
version before using their packages. When using a separate Qt SDK, install its
Quick/Quick Controls, OpenGL, D-Bus, SVG, and Test components and configure with
`-DCMAKE_PREFIX_PATH=/absolute/path/to/Qt/version/gcc_64`. Keep the SDK's runtime
plugins and QML modules together with its libraries; mixing Qt versions can fail
at startup. See [Qt's Linux documentation](https://doc.qt.io/qt-6/linux.html).

## Build and run

Run these commands at the root of a checkout or extracted source archive:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/sofafin --windowed
```

The tests require no Jellyfin credentials. Review any skipped cases as described
in [testing](testing.md). Lower `--parallel` if memory is limited.

To build only the application, use a separate directory:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-release --parallel 4
```

This omits Qt Test, the libavutil guard, the D-Bus test runner, and all test binaries.
libmpv still requires its own runtime dependencies. QML, icons, and artwork
placeholders are compiled into the executable; the source tree is not needed at runtime.

## Install

```sh
cmake --install build --prefix "$HOME/.local"
```

With default GNU install directories, this installs:

| Path relative to prefix | Contents |
| --- | --- |
| `bin/sofafin` | Application |
| `share/applications/moe.ckl.sofafin.desktop` | Fullscreen desktop launcher |
| `share/icons/hicolor/scalable/apps/moe.ckl.sofafin.svg` | Application icon |
| `share/doc/Sofafin/` | README, policies, release notes, and guides |

Ensure `$HOME/.local/bin` is on your `PATH` so the desktop launcher's `Exec=sofafin`
resolves. A desktop session may need to refresh its application menu. The install
does not bundle Qt, libmpv, SDL2, or graphics drivers.

Downstream packagers can set `-DCMAKE_INSTALL_PREFIX=/usr` when configuring and
stage an installation using `DESTDIR=/path/to/package-root cmake --install build`.
See [packaging notes](releasing.md#downstream-packaging).

## Steam Deck and Gaming Mode

Build on a normal Linux development system with matching runtime dependencies.
Bundled SteamOS distribution remains pending; do not disable SteamOS's read-only
system just to install development packages. A binary built on another distro
may require newer Qt, glibc, or libmpv than SteamOS provides.

On a system where Sofafin and its runtime dependencies are available:

1. In Steam Desktop Mode, add the `sofafin` executable as a non-Steam game.
2. Set launch options to `--fullscreen` and choose a normal **Gamepad** Steam Input template.
3. Launch it in Gaming Mode. Select a text field and press A to request Steam's keyboard.
4. If Qt chooses an unusable display platform, try `QT_QPA_PLATFORM=xcb %command% --fullscreen`
   in Steam launch options. XWayland/xcb must be available.

The app is fully tested in gamescope running on CachyOS Deckify. Use the
[hardware checks](testing.md#hardware-release-checks) to validate overlay, gamepad,
GPU, audio, and suspend behavior on other systems.

For nested gamescope on a desktop with gamescope installed:

```sh
gamescope -w 1280 -h 800 -W 1280 -H 800 -f -- \
  env QT_QPA_PLATFORM=xcb ./build/sofafin --fullscreen
```

## Upgrade and migration

Read the [changelog](../CHANGELOG.md), rebuild from the new source, run the tests,
and install into the same prefix. Settings are separate from the binary and
survive reinstalls. Back up the [settings file](configuration.md#settings-file)
privately if needed; it can contain a reusable access token.

On the first launch without existing Sofafin settings, the old
`JF-Gamescope/client.ini` is moved to `Sofafin/client.ini`, retaining the device ID,
preferences, and remembered session. Existing Sofafin settings take precedence.
The earlier `io.github.sofafin.desktop` and matching icon are not removed by a new
install; remove those two obsolete files from your chosen prefix if upgrading
from that desktop ID.

## Uninstall

Close Sofafin. Remove only the files installed under your selected prefix:
`bin/sofafin`, `share/applications/moe.ckl.sofafin.desktop`,
`share/icons/hicolor/scalable/apps/moe.ckl.sofafin.svg`, and `share/doc/Sofafin/`.
The build directory's `install_manifest.txt` lists the exact installed paths.
Package-managed installations should be removed through the package manager.

Uninstalling does not erase user settings. Sign out first to request server-session
logout, then remove `Sofafin/client.ini` if you want to erase local preferences
and remembered credentials. Revoking the session on Jellyfin also invalidates a
token in an old settings backup.
