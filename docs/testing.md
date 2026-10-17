# Testing

## Local automated checks

Install the [full dependency set](installation.md#dependencies), then:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
python3 scripts/check-docs.py
desktop-file-validate packaging/moe.ckl.sofafin.desktop
```

Python 3 is used by repository tools only, not the app. Desktop entry validation
requires `desktop-file-utils`. CTest sets the native UI and smoke suites to use
Qt offscreen/software rendering. No normal CTest case connects to a real server
or requires account credentials.

| CTest suite | Coverage |
| --- | --- |
| `core` | Playback policy, versions, URL/base paths, tokens, segments, malformed replies, session generations, real loopback HTTP, settings migration |
| `native` | QML controller-style focus, details/seasons, browsing/search, artwork/BlurHash, themes, settings, tracks, timeline, prompts, local mpv decode/pause/seek/restart |
| `episode-buffering` | Wait for full current cache, reuse direct/HLS next-episode streams, memory-limit behavior, cancellation, cleanup, preparation failures |
| `screensaver` (Linux) | Private D-Bus service, playback idle inhibition, pause/stop/end/error, delayed replies, reconnect, shutdown |
| `playback-startup` (Linux) | H.264 start/restart with hardware unavailable; test-only libavutil guard fails if Vulkan probing is attempted |
| `ui-smoke` | Application launches its QML interface and exits cleanly with isolated settings |

The suite count is six on Linux. Qt Test can skip individual cases while CTest
reports the executable as passed. FFmpeg missing from PATH and prohibited loopback
sockets are possible skip reasons. For release evidence, inspect verbose output
and `build/Testing/Temporary/LastTest.log`; record skips and rerun in a suitable
environment. FFmpeg must support `libx264` for the H.264 fixture.

The hardware guard is built only for tests and must never be installed or used
as a production `LD_PRELOAD`. The private screensaver test does not change the
desktop's actual idle settings.

## CI

The GitHub Actions workflow builds Debug and Release in a Debian 13 container,
runs all six suites, checks local documentation links and the desktop entry,
stages installation, and exercises application-only builds. The Release job
creates a committed-tree source archive, verifies its checksum, and rebuilds it
without Git metadata. Test logs and the source archive are retained as artifacts.
It has read-only repository permissions and does not publish releases.

CI artifacts from ordinary commits/PRs are development snapshots. A passing job
does not establish physical GPU, Steam overlay, gamepad, audio, or HDR support.

## Live-server checks

These tools are opt-in and excluded from CTest/CI. Use a disposable Jellyfin
account with at least one accessible movie/episode, direct play, and transcoding
permission. The UI runner updates watch progress and temporarily changes favorites;
it attempts to restore favorites and sign out, but interrupted runs can leave
account state/session changes. Do not use a primary account.

In Bash, enter credentials without writing the password into shell history:

```bash
export SOFAFIN_SERVER='https://your-server.example'
read -r -p 'Username: ' SOFAFIN_USERNAME
read -r -s -p 'Password: ' SOFAFIN_PASSWORD
export SOFAFIN_USERNAME SOFAFIN_PASSWORD
./build/sofafin-live-check
```

The checker signs in, browses libraries, negotiates direct play and forced
transcoding, and asks external `mpv` to decode up to 30 frames in each mode. It
requires `mpv` on PATH and does not print credentials or media URLs. The media URL
is passed as a child-process argument and may be visible to local process-inspection
tools; run on a trusted machine.

The live UI runner signs in through the interface using isolated settings. It
exercises real library browsing, filters/sort/pagination, series and episodes,
favorite round-trips, direct/transcoded decoding, subtitles, seek/trickplay, and scale.

```sh
SOFAFIN_TEST_HEIGHT=1080 ./build/sofafin-live-ui-tests
SOFAFIN_TEST_HEIGHT=1440 ./build/sofafin-live-ui-tests
SOFAFIN_TEST_HEIGHT=2160 ./build/sofafin-live-ui-tests
unset SOFAFIN_SERVER SOFAFIN_USERNAME SOFAFIN_PASSWORD
```

The runner configures both Qt's virtual display and its window to actual 16:9
pixel dimensions at DPR 1. It does not upscale a low-resolution screenshot.
Video decoding uses a separate libmpv software render surface, so the UI's video
region is black. Set `SOFAFIN_THEME_TEST_TITLE` to a known series with a theme to
enable the optional continuity assertion.

## Screenshots

For a server-free sign-in screenshot:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/sofafin --windowed --smoke-test --screenshot /tmp/sofafin.png
```

Set `SOFAFIN_SCREENSHOT_DIR=/absolute/output/directory` for full-resolution live UI
screenshots. Screenshots can reveal account/library information; inspect them
before uploading. The native test runner can also save fixture screenshots when
these variables name output PNG paths:

| Variable | Screen |
| --- | --- |
| `SOFAFIN_LOGIN_SCREENSHOT` | Sign-in |
| `SOFAFIN_SETTINGS_SCREENSHOT` | Settings |
| `SOFAFIN_TEST_SCREENSHOT` | Home |
| `SOFAFIN_GRID_SCREENSHOT` | Grid |
| `SOFAFIN_SERIES_SCREENSHOT` | Series |
| `SOFAFIN_COMPACT_DETAIL_SCREENSHOT` | Small-window detail |
| `SOFAFIN_DETAIL_SCREENSHOT` | Detail |
| `SOFAFIN_TIMELINE_SCREENSHOT` | Seek preview |
| `SOFAFIN_SKIP_SCREENSHOT` | Skip prompt |
| `SOFAFIN_NEXT_SCREENSHOT` | Next episode |
| `SOFAFIN_PLAYER_SCREENSHOT` | Player controls |
| `SOFAFIN_PAUSE_SCREENSHOT` | Paused screen |
| `SOFAFIN_STOP_SCREENSHOT` | Stop confirmation |

## Hardware release checks

Record OS, Steam client, server, Qt/libmpv versions, GPU/driver, controller, display
resolution, and build commit for each run. Mark untested cells explicitly.

| Environment / scenario | Evidence required |
| --- | --- |
| Linux desktop, native OpenGL | Visible video, correct aspect/size, smooth UI, resize/fullscreen, sound |
| Physical Steam Deck Gaming Mode | Launch, Steam keyboard (including password), focus, controls, overlay, stop/exit |
| Desktop nested gamescope | xcb launch, correct video presentation, no startup crash |
| VA-API / NVDEC where available | Actual hardware decode and software fallback; test unavailable GPU paths separately |
| Controller hotplug | Disconnect/reconnect during browsing and playback without losing usable focus |
| Direct / forced transcode | Start, pause, seek, resume, track/quality changes, clean transcode shutdown |
| Episode transitions | End-of-file, Start now, Close, dismissed prompt, across-season ordering, failed prefetch |
| Idle behavior | Playing prevents system dimming; pause/stop releases it; theme music does not hold it |
| Suspend/resume and network loss | Recover or show a usable error; stop remains available |
| Display/UI scale | 1280×800 Deck, 1080p, 1440p, 4K; 75%, 100%, 150%; scroll/focus and text remain accessible |

HDR and surround passthrough need separate evidence before any support claim.
The [17 October report](usability-2026-10-17.md) is historical live/headless evidence,
not a substitute for this checklist or a statement about the current suite count.
