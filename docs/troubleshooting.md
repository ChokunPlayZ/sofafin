# Troubleshooting

## Gather useful diagnostics

Record the Sofafin version/commit, distro, desktop session, Qt/libmpv versions,
controller, Jellyfin server version, playback mode, and reproduction steps.

```sh
QT_QPA_PLATFORM=offscreen ./build/sofafin --version
cmake --version
pkg-config --modversion Qt6Core mpv sdl2
ctest --test-dir build --output-on-failure
```

The pkg-config commands require development packages; installed end users can
report library versions from their package manager instead. Capture terminal
output if useful, then redact passwords, tokens, Authorization headers, full media
URLs, private hostnames, and account information before sharing. Do not attach
`client.ini` or publish a server test account. See [security](../SECURITY.md).

## Build or startup fails

| Symptom | Check / action |
| --- | --- |
| Qt 6.5 or a Qt component is missing | Follow the distro-specific [dependency list](installation.md#dependencies); point `CMAKE_PREFIX_PATH` at a matching SDK if needed |
| mpv/sdl2 pkg-config check fails | Install development packages and pkg-config; verify `pkg-config --modversion mpv sdl2` |
| libavutil or dbus-run-session is missing | Install test dependencies, or build a separate application-only directory with `-DBUILD_TESTING=OFF` |
| `module QtQuick... is not installed` | Install the listed QML runtime modules from the same Qt version |
| Icons are missing | Install the Qt SVG runtime plugin (`qt6-svg` / `libqt6svg6`) |
| xcb/Wayland platform plugin fails | Ensure a running graphical session and matching Qt platform dependencies; use `QT_DEBUG_PLUGINS=1` to identify missing libraries |
| Old CMake cache selects a different Qt | Configure into a fresh `build-*` directory or correct `CMAKE_PREFIX_PATH` |
| Desktop launcher is missing or fails | Ensure the install prefix's `bin` is on PATH; confirm the desktop file and icon were installed; remove an obsolete earlier desktop ID after upgrading |

`--help` and `--version` still construct a Qt GUI application. In a terminal
without a display, set `QT_QPA_PLATFORM=offscreen` for those commands.

## Cannot sign in or browse

Use the full server base URL, including a reverse proxy's base path if present.
Do not use a `/web/` browser URL, embedded userinfo, a query string, or a fragment.
Check connectivity and account/library permissions on the same network.

Sofafin does not bypass certificate verification. Fix expired/untrusted certificates
and proxy HTTPS configuration. A response containing HTML instead of JSON usually
means the wrong URL or an authentication/proxy page. Browser SSO and Quick Connect
are not implemented. A revoked/expired token requires another sign-in.

An API redirect to a different host/scheme/port is refused. Configure the actual
public Jellyfin origin and consistent proxy base paths. Media and subtitle URLs
also need that origin. See [network behavior](privacy.md#network-behavior).

## Controller or Steam keyboard does not work

Use Steam's normal Gamepad template, confirm SDL recognizes the controller, and
reconnect it. Sofafin uses the first recognized controller. Keyboard arrows,
Enter, and Escape can help distinguish controller mapping from focus problems.
Custom devices can need `SDL_GAMECONTROLLERCONFIG` mappings.

The integrated keyboard requires Steam and a working `steam://` URL handler.
Launch Sofafin through its Steam entry in Gaming Mode, focus the field, and press
A or F2. Physical keyboard input works independently. If desktop display selection
is wrong, try the [xcb launch option](installation.md#steam-deck-and-gaming-mode).

## Video is black, crashes, or plays poorly

Normal playback uses Qt Quick OpenGL. Remove `QT_QUICK_BACKEND=software` from a
normal launch; it is a headless UI/testing option. Check the graphics driver and
Qt platform, then try Hardware decoding Off. Linux deliberately excludes Vulkan
decoder probing; rebuilding with `hwdec=auto` can reintroduce Gamescope startup
failures.

If Direct only rejects a title, switch to Automatic and confirm that the Jellyfin
account may transcode. A fixed bitrate requests transcoding; Original requests
direct play. Inspect media info and try a different media version/track. Server
transcode logs can explain unsupported codecs, subtitles, or conversion failures.
Reduce quality for slow links. High-bitrate media and next-episode prefetch can
use substantial RAM; see [cache limits](playback.md#buffering-and-next-episode-prefetch).

No audio: check the system output device/volume and another application first.
Sofafin requests stereo. Theme volume controls themes only. Missing skip buttons,
themes, or trickplay can simply mean the server has not supplied that data.

## Idle dimming, settings, or test problems

Video inhibition requires a session D-Bus screensaver service. It is intentionally
released when paused, and themes never hold it. Gaming Mode also requires Steam
client support for media-app inhibition. Test on a real device before assuming
the private D-Bus regression test proves system behavior.

If preferences cannot be saved, check ownership/writability of the configuration
directory. To try clean settings without losing the original, set an isolated
`XDG_CONFIG_HOME` as shown in [configuration](configuration.md#settings-file).

For test timeouts or skips, confirm FFmpeg's `libx264` encoder, loopback sockets,
and `dbus-run-session` are available. View `build/Testing/Temporary/LastTest.log`
or rerun the failed suite with `ctest --test-dir build -V -R NAME`.
Report the failure with sanitized output and whether it reproduces outside a
restricted sandbox. See [testing](testing.md).
