# Third-party acknowledgments

Sofafin's original code, documentation, and bundled artwork use
[GPL-3.0-or-later](LICENSE). External dependencies retain their own copyrights
and licenses. This file is an inventory, not a replacement for their license texts.

| Dependency / service | Use | Upstream licensing / documentation |
| --- | --- | --- |
| Qt 6 | Core, GUI, Quick/Controls, Network, OpenGL, D-Bus; Qt Test in tests | [Qt licensing](https://doc.qt.io/qt-6/licensing.html); terms vary by module/distribution |
| mpv / libmpv | Video, themes, render API, cache, track control | [mpv copyright and build-dependent terms](https://github.com/mpv-player/mpv/blob/master/Copyright) |
| SDL2 | Controller input | [SDL license](https://www.libsdl.org/license.php), zlib for SDL2 |
| FFmpeg / libavutil | libmpv dependency; test fixture generation and hardware guard | [FFmpeg licensing](https://ffmpeg.org/legal.html); terms depend on build options |
| Jellyfin | External user-supplied server and API | [Jellyfin project](https://github.com/jellyfin/jellyfin) |
| D-Bus / desktop screensaver service | Session idle inhibition | [D-Bus](https://www.freedesktop.org/wiki/Software/dbus/) |
| Steam / gamescope | Optional launch environment and Steam keyboard handler | [Steam](https://store.steampowered.com/), [gamescope](https://github.com/ValveSoftware/gamescope) |

The source tree does not vendor these dependency implementations. Runtime
libraries come from the system or a separately installed SDK. Artwork, themes,
subtitles, and media served by Jellyfin belong to their respective owners and are
not included in a source release. The bundled Sofafin SVGs live in `assets/`.

If distributing binaries or bundling libraries, inventory the exact library
versions/build options, retain their notices, and satisfy the applicable source
and redistribution obligations. A dynamically linked executable is not by itself
a complete dependency license bundle. See [downstream packaging](docs/releasing.md#downstream-packaging).

Sofafin is independent of Jellyfin, Valve, Qt, mpv, and SDL; their names describe
interoperability and do not imply endorsement.
