# Changelog

Notable user-visible changes are recorded here. No public release has been
published from this repository yet. The first preview targets version **0.1.0**.

## Unreleased

### Added

- Native Qt Quick movie/TV client with SDL gamepad, keyboard, and pointer input.
- Jellyfin sign-in, remembered sessions, server libraries, search, filters,
  sorting, pagination, favorites, watched actions, and resume progress.
- Full-screen details, season/episode navigation, artwork/BlurHash placeholders,
  cast, chapters, profile images, and configurable theme songs.
- libmpv direct play and Jellyfin HLS transcoding, media versions, quality/audio/
  subtitle choices, seek previews, trickplay, skip segments, and playback reporting.
- Next-episode countdown/autoplay and speculative prebuffering after full current cache.
- Linux hardware decode fallback avoiding Vulkan probing; playback idle inhibition.
- UI scale, accent/fullscreen preferences, Steam keyboard requests, and stop confirmation.
- GNU GPL v3-or-later license, user/developer documentation, contribution/security
  policies, issue/PR templates, CI, and source-release/checksum tooling.

### Changed

- Application name is Sofafin; legacy JF-Gamescope settings migrate on first launch.
- Desktop application ID is `moe.ckl.sofafin`.
- Application and Jellyfin protocol versions are generated from CMake's project version.
- Installation includes user/developer guides, release notes, and license notices.
- Documented full application testing in gamescope running on CachyOS Deckify.

### Known limitations

- Hardware and distributions beyond CachyOS Deckify running gamescope require
  separate validation. HDR/passthrough are not certified.
- Bundled SteamOS distribution, Quick Connect, persistent queues, Live TV, music
  screens, and playlists are unavailable. Offline downloads are outside current scope.
- No fixed Jellyfin minimum version has been established; optional server metadata
  determines trickplay, themes, and media segments.

See [release gates](docs/releasing.md#release-gates),
[support boundaries](docs/README.md#support-and-limitations), and [BACKLOG.md](BACKLOG.md).
