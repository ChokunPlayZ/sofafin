# JF Gamescope

A native, controller-first Jellyfin client for Steam Deck, with a Steam Big Picture-inspired interface. C++20, Qt 6 Quick, SDL2 input, and embedded libmpv video. Qt renders the interface directly; there is no browser or webview.

This is an early working foundation. Automated usability checks run against a real Jellyfin server at 1080p, 1440p, and 4K. Physical Steam Deck validation is still needed before a release.

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
| D-pad / left stick | Arrow keys | Move focus; preview a seek from the video or timeline |
| A | Enter / F2 | Select; A or F2 opens Steam keyboard in a field; pause in the player |
| B | Escape | Back; close a dialog; hide player controls and prompts, then confirm stopping; prompt to exit on Home |
| X | Space | Pause in the player |
| Y | F | Search |
| LB / RB | Page Up / Page Down | Home / server libraries |
| Menu | F1 | Settings |
| — | F11 | Toggle fullscreen |

Titles open on a full-screen detail page with backdrop artwork. Each season appears as an episode row below the series details; the Episodes button jumps to these rows. Selecting an episode opens its details, and B returns to the same season and episode selection. An episode's Season link jumps to that row on the series page. The Jellyfin profile image in the top-right corner opens Settings; an initial appears when no image is available.

Theme songs supplied by the server play on detail pages, including inherited series themes for episodes. Adjust **Settings → Theme volume** from 0–100% with Left/Right or the slider; the saved level applies immediately and defaults to 70%. The same theme continues while moving between a series, its seasons, and its episodes. Themes stop when leaving title details or starting video and do not create video playback sessions. Disable them in **Settings → Theme songs**. Missing themes are ignored.

Move the left stick horizontally in the player to open the playhead and preview a seek, even when controls are hidden: a small deflection moves slowly, a full deflection moves faster, and holding it accelerates. Left/Right on the D-pad or keyboard also opens a seek preview. Press A / Enter to commit or B / Escape to cancel. Trickplay thumbnails appear when supplied by the server. Pointer dragging seeks on release. Down selects the available skip or next-episode button, or the playhead when neither is shown; Down from either prompt reaches the playhead, and Down again reaches pause, playback settings, and stop. Up from the timeline focuses the available skip or next-episode button, including a dismissed prompt; without a prompt, it hides the controls. Playback settings groups quality, audio, subtitles, and media info. Title details also has an expandable playback settings row for choosing options before playing; cast and chapters expand separately. B outside a seek preview hides the player controls and prompts. B again, or choosing Stop, opens a confirmation dialog with Keep playing focused and a "Press B / Esc to keep playing" hint. Playback pauses while confirming and returns to its previous pause state when canceled. After ten seconds paused without input, the video dims and shows the title and episode. The controller can be disconnected and reconnected without restarting.

The interface scales automatically for the display resolution. **Settings → UI scale** adjusts the entire interface from 75–150% in 5% steps with Left/Right. This includes text, controls, dialogs, and artwork; settings remain scrollable at larger scales. Automatic audio and subtitle choices name the track being used. Shelf, season, detail, and artwork placeholders show skeletons while data loads, and remembered sessions show a loading screen until authentication completes.

## Implemented

- Server sign-in, optional remembered sessions, and sign-out.
- Server library names and order in the top navigation, with continue watching, next episodes, and recent titles for each video library.
- Library/folder browsing, Movies and Shows catalogs, and search in a responsive poster grid that scrolls down, with pagination, favorites/watched filters, and sorting by name, date added, or rating.
- Full-screen title details with server-fetched ratings, genres, cast/crew, chapter playback, favorites, watched/unwatched actions, and a resume button with playback progress along its lower edge.
- Jellyfin profile images and optional background theme songs, including series themes inherited by episodes.
- Season sections below series details, with episodes in server order, links from an episode to its series and season section, and next-episode details.
- Artwork chosen by image role and owner, landscape episode cards, and lazy loading with the server’s matching BlurHash as the placeholder.
- Scrollable detail pages with controller focus scrolling, including long titles at 800×500.
- Direct play over authenticated HTTP and Jellyfin-negotiated HLS transcoding, selected using a libmpv device profile.
- Resume, pause, seeking, buffer progress with download speed, estimated ending time, and series/episode information.
- Automatic next-episode playback with a countdown, episode details, Start now, and Close to finish the current episode and exit the player.
- Quality, audio, subtitle, and media-version selection before playback; quality and track changes during playback preserve position and pause state.
- Media details for the source container, bitrate, size, codecs, resolution, and tracks.
- Intro, credits, recap, preview, and commercial skip buttons from Jellyfin’s Media Segment API when the server supplies segments.
- Playback start/progress/stop reporting and cleanup of server transcoding jobs.
- Automatic / direct-only / forced-transcoding modes, bitrate limits, hardware decoding, fullscreen, UI scale, and accent colors.

The player timeline marks chapter starts with white ticks and media segments with colored ranges. A legend identifies each segment type; the seek preview shows the chapter name and segment at the preview position. Chapters load when playback starts, including when playing directly from a shelf. When the Media Segment API returns no segments, explicitly named chapters such as Opening, OP, Intro, Outro, ED, or Credits provide skip ranges ending at the next chapter (or the end of the title). Other chapter names remain timeline markers without skip buttons.

Series episodes automatically continue in server episode order, including across seasons. During the final 30 seconds, the player shows a compact, translucent “Next Episode in x seconds” prompt with the next episode's season, number, and title below it. If the last skip segment reaches the end of the episode, this prompt appears at that segment's start in place of the skip button. The countdown follows playback, including pauses and seeks. Choose **Start now** to continue immediately, or **Close** to cancel autoplay for the current episode and exit the player when it finishes. B / Escape hides the prompt while keeping autoplay enabled, including after seeking or changing quality. Down from the video selects Start now; Left/Right selects Close. Up from the timeline restores and focuses the prompt. The next episode starts from the beginning with the current quality choice and its own default tracks. The last episode, missing next-episode data, and playback failures do not advance.

Direct play is preferred across the media versions Jellyfin returns. Direct-only mode shows an error if none is playable. Automatic mode permits server transcoding; forced-transcoding mode disables video/audio stream copying. Transcoded seeking requests a fresh stream at the target position so playback progress remains on the source timeline. Global playback settings apply to the next stream. Player quality and track selections apply immediately. Automatic uses the global playback mode and bitrate limit, Original requires direct play, and a specific bitrate requests transcoding.

Video playback continuously prefetches up to an hour ahead, limited to 1 GiB of forward cache plus 256 MiB for cached backward seeks. Memory is used as data arrives. Startup, seeks, and underruns target five seconds of buffered media before resuming, or less when the cache fills or the stream ends. The buffering indicator appears immediately when a seek is committed and stays until playback is ready; network streams also show the current download speed when available. Exiting playback resets the timeline and track labels, and a new title opens on black until its first frame is ready.

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

The opt-in native usability runner signs in through the UI, browses the real libraries, filters/sorts/paginates, navigates series and episodes, checks favorite round-trips, decodes direct and transcoded video, changes subtitles, previews stick/pointer seeks with trickplay, and checks scale settings and screenshots. It uses isolated settings and signs out afterward. Playback reporting updates the disposable account's watch progress. Set `JF_SCREENSHOT_DIR` to save full-resolution screenshots; `JF_THEME_TEST_TITLE` optionally selects a known series with a theme for a continuity assertion.

```sh
# Uses JF_SERVER, JF_USERNAME and JF_PASSWORD from the environment.
JF_TEST_HEIGHT=1080 ./build/jf-live-ui-tests
JF_TEST_HEIGHT=1440 ./build/jf-live-ui-tests
JF_TEST_HEIGHT=2160 ./build/jf-live-ui-tests
```

The runner configures Qt's virtual screen and window to the selected 16:9 resolution at DPR 1; it does not resize a low-resolution screenshot. UI rendering uses Qt's software backend and video decoding uses libmpv's software render API. Physical gamepad/Steam overlay, audio output, and native GPU presentation remain hardware checks. See [the usability report](docs/usability-2026-10-17.md).

For a headless screenshot of the sign-in screen:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/jf-gamescope --windowed --smoke-test --screenshot /tmp/jf-gamescope.png
```

Known limits: no Quick Connect, HDR certification, or bundled distribution yet. The actual Steam keyboard overlay, OpenGL surface, hardware decoding, Gamescope integration, and physical controller mappings require hardware validation. Live TV, music, and playlists are deferred in [BACKLOG.md](BACKLOG.md); downloads are out of scope.

## Source layout

`src/JellyfinClient.*` owns the authenticated API transport. `src/Playback.*` builds device capabilities and chooses a negotiated source. `src/AppController.*` manages browsing and playback sessions. `src/MpvPlayer.*` owns libmpv and observes its events; `src/MpvVideo.*` renders into a Qt Quick OpenGL framebuffer. `src/ControllerInput.*` translates SDL gamepad events into native focus navigation. `qml/` contains the native interface.

Protocol/rendering references: [Jellyfin Media Segments API](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/MediaSegmentsController.cs), [playback selection fields](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Models/MediaInfoDtos/PlaybackInfoDto.cs), [mpv cache and track properties](https://mpv.io/manual/stable/), [Jellyfin transcoding](https://jellyfin.org/docs/general/post-install/transcoding/), [Jellyfin PlaybackInfo controller](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/MediaInfoController.cs), [libmpv render API](https://github.com/mpv-player/mpv/blob/master/include/mpv/render.h), [Qt framebuffer integration](https://doc.qt.io/qt-6/qquickframebufferobject.html).

Steam keyboard integration follows the [SDL Steam Deck implementation](https://github.com/libsdl-org/SDL/blob/SDL2/src/video/x11/SDL_x11keyboard.c). Episode ordering and navigation follow Jellyfin’s [TV Shows API](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/TvShowsController.cs). Theme songs use Jellyfin’s [theme media endpoint](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/LibraryController.cs); profile pictures use its [user image endpoint](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/ImageController.cs).
