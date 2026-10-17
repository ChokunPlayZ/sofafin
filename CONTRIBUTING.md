# Contributing to Sofafin

Sofafin currently focuses on Linux movie and TV playback with controller-friendly
Qt Quick navigation. Read the [backlog](BACKLOG.md) and
[support boundaries](docs/README.md#support-and-limitations) before proposing a
large feature. Use an issue to describe the user problem and discuss scope.

## Development setup

Follow [installation](docs/installation.md) for dependencies, then configure a
Debug build with `BUILD_TESTING=ON`. The [architecture guide](docs/architecture.md)
maps the code, and [testing](docs/testing.md) explains local and opt-in live checks.
The app does not require a server account to compile or run its normal tests.

Use C++20 and the existing four-space C++/QML style. Prefer asynchronous network
and mpv operations; discard stale callbacks using the controller's generation
patterns. Keep keyboard/gamepad focus usable, use accessible names for controls,
and handle missing artwork, optional server features, and empty libraries.

## Submit a change

1. Start a focused branch from the current development branch.
2. Explain the user-visible problem and keep unrelated refactors separate.
3. Add meaningful regression coverage when changing playback, networking, session,
   or input behavior. Documentation-only fixes need link/format checks.
4. Update affected user/reference docs and the Unreleased changelog. Document
   defaults, server-side effects, and limitations when behavior changes.
5. Build and run the relevant [checks](docs/testing.md#local-automated-checks).
6. In the pull request, describe the result, validation, and any untested hardware
   paths. Include sanitized screenshots for UI changes when useful.

Use imperative commit subjects describing one coherent change. There is no
required issue-number format. Do not include credentials, personal server data,
generated build trees, settings, recordings, or private screenshots in commits.

Avoid logging authenticated URLs, tokens, request bodies containing passwords,
or complete server responses. Preserve base-path/same-origin behavior and clean
up playback reports/transcodes when canceling work. See [SECURITY.md](SECURITY.md).

## Report bugs and request features

Use the repository issue templates. Include version/commit, platform, server
version, input/display setup, steps, expected/actual result, and sanitized output.
For playback bugs include the mode and non-sensitive codec/container details.
For security issues, follow private reporting in [SECURITY.md](SECURITY.md).

Discuss changes respectfully, focus on the code and user impact, and avoid
personal attacks or sharing another person's private information.

## License

Original contributions are accepted under **GPL-3.0-or-later**, the same terms as
the project. Submit only material you have permission to contribute. Identify any
third-party code/assets and retain their license notices; do not copy assets or
code from another client without checking those terms. See [LICENSE](LICENSE)
and [THIRD_PARTY.md](THIRD_PARTY.md). No separate contributor agreement is required.
