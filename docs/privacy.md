# Privacy and local data

Sofafin connects to the Jellyfin server you configure. There is no project-hosted
analytics, advertising, crash-upload service, account service, or automatic updater
in the application. Checking for/installing updates is manual.

## Stored data

The [settings file](configuration.md#settings-file) stores preferences, the server
URL, username, and a generated device UUID. If Keep me signed in is selected, it
also stores the user ID and an access token. Passwords are used for authentication
and are not saved to disk by Sofafin.

The INI file is **not encrypted**. Sofafin attempts owner-only file permissions on
writes. A process running as your user, an administrator, or a backup containing
the file can still read the token. There is no system-keyring integration yet.

Playback caching uses libmpv's in-memory cache; offline downloads and a persistent
media-download library are not implemented. Qt, the operating system, graphics
drivers, and the configured server can maintain their own caches and logs.

## Network behavior

The app sends your credentials to Jellyfin's authentication endpoint and uses
the returned token for subsequent requests. Its authorization identifies the
client name, app version, device label (`Steam Deck`), and generated device ID.
The device label is fixed, including on desktop Linux.

Library, search, metadata, artwork, profile image, theme song, trickplay, subtitle,
and video requests go to the configured server. Playback start/progress/stop,
favorite changes, and watched/unwatched changes update that account. The server
operator can see these requests and your client IP address.

Qt API requests verify TLS and follow same-origin redirects. Playback negotiation
and external subtitle setup reject URLs outside the configured origin (scheme,
host, and effective port). Subsequent media fetching is handled by libmpv/FFmpeg;
this is not a network sandbox for a malicious server's redirects or playlists.
Connect only to servers you trust. HTTP connections are unencrypted; use HTTPS
with a trusted certificate for remote access.

Media and artwork URLs can contain tokens in query parameters. Treat the entire
URL as a credential. Opening Steam's keyboard delegates a `steam://open/keyboard`
URL to the desktop URL handler; Sofafin does not put the field's text in that URL.

## Sign out and erase data

Sign out requests server logout and removes the locally remembered token/user ID.
It retains preferences, server address, username, and device ID. A network failure
can prevent server-side logout; revoke the device/session from Jellyfin if needed.

Close Sofafin before removing its settings file to erase all local configuration.
Deleting a local token does not revoke copies of it or automatically terminate a
server-side session. Keep settings backups and test screenshots private when they
contain account information or personal library names.

Use the process in [SECURITY.md](../SECURITY.md) for vulnerabilities and the
[testing guide](testing.md#live-server-checks) for disposable-account checks.
