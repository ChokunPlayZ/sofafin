# Sofafin documentation

For a first installation, read [installation](installation.md), then the
[user guide](usage.md). For an existing installation, use
[configuration](configuration.md), [playback](playback.md), or
[troubleshooting](troubleshooting.md).

| Guide | Contents |
| --- | --- |
| [Installation](installation.md) | Requirements, distro packages, build, install, Steam Deck, upgrades, uninstall |
| [User guide](usage.md) | Sign-in, navigation, controller and keyboard controls, details, seeking, autoplay |
| [Configuration](configuration.md) | Preferences, defaults, settings files, CLI, environment variables |
| [Playback](playback.md) | Negotiation, tracks, versions, segments, cache, decoding, idle inhibition |
| [Troubleshooting](troubleshooting.md) | Common failures and useful diagnostics |
| [Privacy](privacy.md) | Stored data, network requests, account changes, credential handling |
| [Architecture](architecture.md) | Components, lifecycles, API surface, design constraints |
| [Testing](testing.md) | Test suites, live-server checks, screenshots, hardware matrix |
| [Releasing](releasing.md) | Release gates, versioning, source archives, publication, downstream packaging |
| [Usability report, 17 October 2026](usability-2026-10-17.md) | Historical live-server validation and its hardware limitations |

Project policies: [contributing](../CONTRIBUTING.md), [security](../SECURITY.md),
[third-party dependencies](../THIRD_PARTY.md), [changelog](../CHANGELOG.md), and
[backlog](../BACKLOG.md).

## Support and limitations

The current target is Linux movie and TV playback with Qt Quick and OpenGL.
The application identifies itself as `Sofafin`, its executable is `sofafin`, and
its desktop ID is `moe.ckl.sofafin`. Older source history uses `JF-Gamescope`.

- The app is fully tested in gamescope running on CachyOS Deckify. Other hardware
  and distributions require separate validation.
- Headless software rendering validates the UI and decoding; it does not validate
  the application's native OpenGL video surface or audio output.
- Qt 6.5 is the declared minimum. CI uses Debian 13's packaged Qt; it does not
  establish compatibility with every Qt release or Linux distribution.
- There is no fixed, verified minimum Jellyfin server version yet. Basic movie/TV
  APIs are required; optional server features determine segments, themes, and trickplay.
- Only username/password sign-in is implemented. Quick Connect, SSO/browser
  sign-in, and multiple simultaneous server profiles are unavailable.
- HDR output, surround passthrough, Windows, and macOS are not validated. The
  player requests stereo audio and does not expose passthrough controls.
- Flatpak/AppImage and other bundled distribution formats are pending.
- Persistent playback queues, Live TV, music screens, and playlists are deferred.
  Downloads/offline viewing are outside the current scope.

The interface is currently English. See [BACKLOG.md](../BACKLOG.md) for product
direction and [testing](testing.md#hardware-release-checks) for outstanding checks.
