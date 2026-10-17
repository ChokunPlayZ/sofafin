# Architecture

Sofafin is a single-process C++20/Qt Quick application. QML and SVG resources are
embedded with Qt's resource system. There is no webview or separate backend server.
CMake defines the version once and generates `Version.h`, used by the CLI and
Jellyfin authorization header.

## Component map

| Component | Responsibility |
| --- | --- |
| `src/main.cpp` | Qt application identity, flags, OpenGL selection, object/context setup, QML startup, smoke screenshot |
| `src/Settings.*` | INI persistence, preference validation, device ID, one-time legacy migration, remembered token |
| `src/JellyfinClient.*` | Qt Network transport, authorization, base-path URLs, JSON, timeout, same-origin API redirects |
| `src/Playback.*` | Pure playback policy, device profile, source selection, URL validation, track/media/segment normalization |
| `src/AppController.*` | Authentication, library/history/detail state, user actions, stream negotiation/reporting, themes, next episodes |
| `src/MpvPlayer.*` | libmpv ownership, asynchronous commands/events, cached track state, buffering, speculative playlist entry |
| `src/MpvVideo.*` | libmpv OpenGL renderer inside `QQuickFramebufferObject` |
| `src/ScreenSaverInhibitor.*` | Asynchronous session D-Bus idle-inhibition lifecycle/retry |
| `src/ControllerInput.*` | SDL polling, first recognized controller, key translation/repeat, analog timeline input, Steam keyboard URL |
| `src/BlurHash.*` | BlurHash decoding and QML image provider |
| `qml/Main.qml` | Window, sign-in, shelves/grid, search, global settings, exit handling |
| `qml/DetailView.qml` | Details, season rows, expanded cast/chapters/options, focus restoration |
| `qml/PlayerView.qml`, `qml/SeekBar.qml` | Video UI, focus, draft seeks, prompts, tracks/info, stop confirmation |
| Other `qml/` components | Artwork, shelves, cards, buttons, selectors, loading skeletons |
| `tests/` | Qt Test suites, fake network, local media/HTTP/D-Bus fixtures, opt-in real-server tools |
| `packaging/` | Desktop launcher; executable/icon/desktop installation is in CMake |
| `scripts/` | Documentation link checking and committed-tree source packaging |

`sofafin-core` links Qt Core/Network. `sofafin-native` adds UI, controller,
libmpv, OpenGL, and D-Bus integration. The application and native tests use those
libraries. QML receives `App`, `Player`, `Preferences`, and `Controller` context
objects, plus the registered `Sofafin 1.0` `MpvVideo` type.

## Data and request lifecycle

1. Startup loads preferences and constructs the main video player, controller,
   and application controller. The controller also owns a separate audio-only theme player.
2. A saved session is validated through `Users/Me`; otherwise sign-in posts to
   `Users/AuthenticateByName`. UI loading state stays active during restoration.
3. Library and metadata responses normalize JSON into QVariant maps/lists for QML.
   Request generations discard stale results after navigation or session changes.
4. Playback posts a device profile/options to `Items/{id}/PlaybackInfo`.
   `Playback::negotiate` chooses a valid source and authenticated media URL.
5. The player loads asynchronously. Its event observations update position,
   duration, buffering, track mappings, and EOF state. The controller reports
   start/progress/stop and manages transcode cleanup.
6. Once the active episode is fully cached, next-episode preparation can append
   a speculative stream to mpv's playlist. Autoplay adopts it, while cancellation
   discards it and cleans up the unused server transcode.

Theme requests have their own generation/context so moving within the same series
does not restart the same song. They do not send video playback reports.

## Jellyfin API surface

Paths below are relative to the configured server base path. The client does not
provide a public API of its own.

| Purpose | Requests |
| --- | --- |
| Authentication | `POST Users/AuthenticateByName`, `GET Users/Me`, `POST Sessions/Logout` |
| Libraries and catalog | `GET Users/{user}/Views`, `GET Users/{user}/Items`, `GET Users/{user}/Items/Resume`, `GET Shows/NextUp` |
| Details and episode order | `GET Users/{user}/Items/{id}`, `GET Items/{id}`, `GET Shows/{series}/Seasons`, `GET Shows/{series}/Episodes` |
| User state | POST/DELETE `Users/{user}/FavoriteItems/{id}` and `Users/{user}/PlayedItems/{id}` |
| Negotiation | `POST Items/{id}/PlaybackInfo` |
| Streaming | Direct `Videos/{id}/stream`, or the returned HLS URL; returned subtitle delivery URLs |
| Playback reporting | `POST Sessions/Playing`, `Sessions/Playing/Progress`, `Sessions/Playing/Stopped` |
| Transcode cleanup | `DELETE Videos/ActiveEncodings` with device/play-session identifiers |
| Optional media | `GET MediaSegments/{id}`, `GET Items/{id}/ThemeSongs`, item/user images and trickplay tiles |

See upstream [PlaybackInfo](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/MediaInfoController.cs),
[TV shows](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/TvShowsController.cs),
and [media segment](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/MediaSegmentsController.cs)
controllers for protocol definitions. Feature availability depends on the server.

## Rendering and concurrency constraints

Qt's GUI/event loop owns application state. Network requests are asynchronous;
callbacks must tolerate cancellation/stale generations. SDL is polled every 16 ms.
Video rendering runs through Qt Quick's render integration; libmpv wakeups queue
event processing back to the owning QObject. Render callbacks request Qt updates
without directly manipulating GUI state from the render callback.

Avoid synchronous queries that can stall when a stream changes. Track-list data
is observed/cached and selections are deferred until mappings arrive. The
[historical usability report](usability-2026-10-17.md) records a related 4K freeze.

Do not substitute Vulkan auto-probing for the Linux decoder allowlist. Production
rendering uses [libmpv's render API](https://github.com/mpv-player/mpv/blob/master/include/mpv/render.h)
and [Qt's framebuffer integration](https://doc.qt.io/qt-6/qquickframebufferobject.html).
UI software tests use a different video render path; both need coverage.

## Boundaries to preserve

- Use the server base path consistently; do not discard reverse-proxy paths.
- Keep authenticated API redirects same-origin, reject foreign playback/subtitle
  URLs, and avoid putting token-bearing URLs in user-visible errors/logs.
- Preserve position and pause state across stream changes, including transcoded offsets.
- Finish old reporting/transcode lifecycles before abandoning or replacing a session.
- Keep draft seeking separate from committed playback changes.
- Keep focus reachable with a controller, including popups and scaled/scrolling views.
- Do not apply main-video watch reporting or idle inhibition to theme audio.

See [testing](testing.md) and [CONTRIBUTING.md](../CONTRIBUTING.md) before changing
these boundaries.
