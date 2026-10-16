# JF Gamescope

A native, controller-first Jellyfin client for Steam Deck, with a Steam Big Picture-inspired interface. C++20, Qt 6 Quick, SDL2 input, and embedded libmpv video. Qt renders the interface directly; there is no browser or webview.

This is an early working foundation. It needs validation on a physical Deck and a reachable Jellyfin server before a release.

## Build and run

Requires CMake 3.22+, a C++20 compiler, Qt 6.5+ (Quick, Quick Controls 2, Network, OpenGL, Test), SDL 2.0.14+, and libmpv client API 2.0+. `ffmpeg` generates the local playback test fixture; the `mpv` executable is used by the optional live server check.

Linux playback startup tests also require the libavutil development headers (`ffmpeg` on Arch/CachyOS; `libavutil-dev` on Debian/Ubuntu) and FFmpeg's `libx264` encoder.

On Arch/CachyOS, the development packages are `base-devel cmake ninja qt6-base qt6-declarative sdl2 mpv ffmpeg`. On Debian/Ubuntu they are `build-essential cmake ninja-build pkg-config qt6-base-dev qt6-declarative-dev qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-templates qml6-module-qtqml-workerscript libsdl2-dev libmpv-dev mpv ffmpeg`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
./build/jf-gamescope --windowed
```

Enter the server's full base URL, username, and password. Press A in an input field, tap the field, or press F2 to open Steam’s on-screen keyboard. Steam must be running; add the app to Steam for Gaming Mode. The keyboard types directly into the focused field, including masked password fields. A physical keyboard also works, and Enter advances or submits the field.

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
| A | Enter / F2 | Select; A or F2 opens Steam keyboard in a field; pause in the player |
| B | Escape | Back; close a dialog; hide player controls, then stop; prompt to exit on Home |
| X | Space | Pause in the player |
| Y | F | Search |
| LB / RB | Page Up / Page Down | Home / server libraries |
| Menu | F1 | Settings |
| — | F11 | Toggle fullscreen |

Titles open on a full-screen detail page with backdrop artwork. B returns to the previous detail page or library. Series opens seasons, selecting a season opens its episodes, and selecting an episode opens its detail page. The Jellyfin profile image in the top-right corner opens Settings; an initial appears when no image is available.

Theme songs supplied by the server play at reduced volume on detail pages, including inherited series themes for episodes. They stop when leaving the page or starting video and do not create video playback sessions. Disable them in **Settings → Theme songs**. Missing themes are ignored.

Press Down in the player to reach pause, playback settings, and stop. Playback settings groups quality, audio, subtitles, and media info. Title details also has an expandable playback settings row for choosing options before playing; cast and chapters expand separately. Up or B hides the player controls; B again stops playback. After ten seconds paused without input, the video dims and shows the title and episode. The controller can be disconnected and reconnected without restarting.

## Implemented

- Server sign-in, optional remembered sessions, and sign-out.
- Server library names and order in the top navigation, with continue watching, next episodes, and recent titles for each video library.
- Library/folder browsing, Movies and Shows catalogs, pagination, search, favorites/watched filters, and sorting by name, date added, or rating.
- Full-screen title details with server-fetched ratings, genres, cast/crew, chapter playback, favorites, and watched/unwatched actions.
- Jellyfin profile images and optional background theme songs, including series themes inherited by episodes.
- Series → seasons → episodes browsing in server order, links from an episode to its series and season, and next-episode details.
- Artwork chosen by image role and owner, landscape episode cards, and lazy loading with the server’s matching BlurHash as the placeholder.
- Scrollable detail pages with controller focus scrolling, including long titles at 800×500.
- Direct play over authenticated HTTP and Jellyfin-negotiated HLS transcoding, selected using a libmpv device profile.
- Resume, pause, seeking, buffer progress, estimated ending time, and series/episode information.
- Quality, audio, subtitle, and media-version selection before playback; quality and track changes during playback preserve position and pause state.
- Media details for the source container, bitrate, size, codecs, resolution, and tracks.
- Intro, credits, recap, preview, and commercial skip buttons from Jellyfin’s Media Segment API when the server supplies segments.
- Playback start/progress/stop reporting and cleanup of server transcoding jobs.
- Automatic / direct-only / forced-transcoding modes, bitrate limits, hardware decoding, fullscreen, poster size, and accent colors.

Direct play is preferred across the media versions Jellyfin returns. Direct-only mode shows an error if none is playable. Automatic mode permits server transcoding; forced-transcoding mode disables video/audio stream copying. Transcoded seeking requests a fresh stream at the target position so playback progress remains on the source timeline. Global playback settings apply to the next stream. Player quality and track selections apply immediately. Automatic uses the global playback mode and bitrate limit, Original requires direct play, and a specific bitrate requests transcoding.

On Linux, hardware decoding tries VA-API and NVDEC, with software fallback when neither is available. Vulkan decoding is excluded: mpv's automatic probing can abort inside Steam/Gamescope's injected Vulkan WSI layer at playback startup. Older mpv versions that only accept one decoder use `vaapi-copy` with software fallback. The application does not change Steam's overlay or global graphics settings.

Authentication and stream negotiation use Qt Network with verified TLS and same-origin API redirects. Playback and external subtitle URLs are restricted to the server's origin. Passwords are never saved. If **Keep me signed in** is selected, the access token is stored in the user's `JF-Gamescope/client.ini` settings file with owner-only permissions; it is not encrypted. Sign out clears that token and ends the server session. Treat media URLs containing tokens as credentials when sharing logs.

## Validation

```sh
ctest --test-dir build --output-on-failure
```

Tests cover playback policy, media-version selection, base paths, token handling, malformed responses, expired/stale sessions, controller-style UI navigation, Steam keyboard requests and focus, artwork/BlurHash decoding and lazy loading, full-screen episode/series/season navigation, favorites and watched updates, small-screen detail bounds, profile image loading, theme audio playback and cancellation, server library tabs, exit/overlay controls, pause-screen timing, playback selections and segment handling, browsing/search/pagination, and actual libmpv decoding/pause/seek/resume of a generated local clip. The player test uses libmpv's software render API to run without a display; the application uses the OpenGL render API. The loopback HTTP test reports a skip if the execution environment forbids sockets.

The Linux `playback-startup` regression test starts and restarts an H.264 clip with hardware decoding enabled. A test-only libavutil guard makes hardware devices unavailable and terminates the child if it probes Vulkan, verifying software fallback without requiring a GPU or Gamescope. The old automatic decoder policy fails this test.

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

Known limits: no Quick Connect, automatic next-episode playback, HDR certification, or bundled distribution yet. The actual Steam keyboard overlay, OpenGL surface, hardware decoding, Gamescope integration, and physical controller mappings require hardware validation. Live TV, music, and playlists are deferred in [BACKLOG.md](BACKLOG.md); downloads are out of scope.

## Source layout

`src/JellyfinClient.*` owns the authenticated API transport. `src/Playback.*` builds device capabilities and chooses a negotiated source. `src/AppController.*` manages browsing and playback sessions. `src/MpvPlayer.*` owns libmpv and observes its events; `src/MpvVideo.*` renders into a Qt Quick OpenGL framebuffer. `src/ControllerInput.*` translates SDL gamepad events into native focus navigation. `qml/` contains the native interface.

Protocol/rendering references: [Jellyfin Media Segments API](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/MediaSegmentsController.cs), [playback selection fields](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Models/MediaInfoDtos/PlaybackInfoDto.cs), [mpv cache and track properties](https://mpv.io/manual/stable/), [Jellyfin transcoding](https://jellyfin.org/docs/general/post-install/transcoding/), [Jellyfin PlaybackInfo controller](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/MediaInfoController.cs), [libmpv render API](https://github.com/mpv-player/mpv/blob/master/include/mpv/render.h), [Qt framebuffer integration](https://doc.qt.io/qt-6/qquickframebufferobject.html).

Steam keyboard integration follows the [SDL Steam Deck implementation](https://github.com/libsdl-org/SDL/blob/SDL2/src/video/x11/SDL_x11keyboard.c). Episode ordering and navigation follow Jellyfin’s [TV Shows API](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/TvShowsController.cs). Theme songs use Jellyfin’s [theme media endpoint](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/LibraryController.cs); profile pictures use its [user image endpoint](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/ImageController.cs).
