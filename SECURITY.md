# Security policy

## Supported versions

Sofafin is an early preview. Security fixes target the current development tree
and, once published, the latest preview release. No older release branch has a
promised maintenance period or response-time commitment.

## Report a vulnerability

Use the repository host's **Security → Report a vulnerability** private reporting
feature when available. Include the affected version/commit, impact, reproduction
steps, and a minimal sanitized example. Do not post working access tokens,
passwords, private server addresses, or a full settings file.

If private reporting is unavailable, open an issue asking for a private contact
channel without including the exploit or sensitive details. Maintainers must
enable private reporting or publish a monitored security contact before release.
No email address is currently designated in this repository.

## Current security boundaries

- Passwords are not persisted. Remembered sessions store an unencrypted token in
  a user INI file with attempted owner-only permissions; there is no keyring yet.
- API requests use verified TLS, same-origin redirects, and a transfer timeout.
  HTTP is supported and provides no transport encryption.
- Negotiated playback and external subtitle URLs are checked against the server
  origin. libmpv/FFmpeg subsequently fetch media and playlists; the app is not a
  sandbox for untrusted media servers or their network redirects.
- Media/artwork URLs may contain tokens. Logs, process arguments, crash reports,
  proxy logs, screenshots, and settings backups can expose private information.
- The app normally runs with your desktop user's permissions. Keep Qt, SDL2,
  libmpv, FFmpeg, graphics drivers, and OS packages updated.

If a token is exposed, revoke its session/device on Jellyfin. Removing a local
file or redacting a later copy does not invalidate already copied credentials.
See [privacy](docs/privacy.md) and [troubleshooting](docs/troubleshooting.md).
