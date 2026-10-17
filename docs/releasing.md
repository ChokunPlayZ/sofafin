# Releasing Sofafin

This is the maintainer runbook. Version **0.1.0** is the first planned preview;
the changelog does not claim it has shipped. Preparing these files does not
publish a repository, push a tag, or create a hosted release.

## Release gates

- [ ] Review the final source and Git history for credentials, private settings,
  token-bearing URLs, personal test data, and accidental generated files.
- [ ] Confirm `LICENSE` contains GNU GPL v3 and the project grant is
  `GPL-3.0-or-later`; retain third-party notices for the actual distribution.
- [ ] Choose/configure the canonical public repository and issue tracker. Enable
  private vulnerability reporting or add a monitored contact to `SECURITY.md`.
  No remote URL is assumed by the repository files.
- [ ] Build/test Debug and Release; review skips, not only CTest's overall exit code.
- [ ] Run the optional direct/transcoding and UI checks against a disposable
  server account; record Jellyfin version and results.
- [ ] Complete the [hardware matrix](testing.md#hardware-release-checks), especially
  physical Steam Deck, Steam keyboard, native OpenGL, audio, controller hotplug,
  Gamescope, and idle/suspend behavior. Do not make unsupported hardware claims.
- [ ] Verify the source archive, checksum, installation, and build without `.git`.
- [ ] Update README status, supported/tested environments, docs, and release notes.
- [ ] Select the intended audience/distribution. The current deliverable is a
  source preview; it is not a self-contained SteamOS install.

If hardware checks remain incomplete, keep the development preview status and
explicit limitations. Do not describe it as a validated Steam Deck stable release.
CI alone cannot close these hardware gates.

## Versioning and changelog

Use `MAJOR.MINOR.PATCH` in `project(Sofafin VERSION ...)` in `CMakeLists.txt`.
CMake generates the version used by `--version` and the Jellyfin client header;
do not hard-code another runtime version. Update documentation examples when the
release version changes.

Before tagging, move Unreleased notes into a `VERSION - YYYY-MM-DD` section using
the actual release date, add a new Unreleased section, and describe remaining
limits. Do not invent release dates or server/hardware compatibility claims.
During the 0.x preview period, behavior/settings may change; document migration
or compatibility impact in every release.

Commit the reviewed release files before making the tag. The packaging script
archives a selected **committed** tree and intentionally excludes local changes.

## Validate the release candidate

```sh
cmake -S . -B build-release-check -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-release-check --parallel 4
ctest --test-dir build-release-check --output-on-failure
python3 scripts/check-docs.py
desktop-file-validate packaging/moe.ckl.sofafin.desktop
DESTDIR="$PWD/build-release-check/stage" cmake --install build-release-check
python3 scripts/check-docs.py --root build-release-check/stage/usr/share/doc/Sofafin
QT_QPA_PLATFORM=offscreen build-release-check/stage/usr/bin/sofafin --version
```

Repeat with a Debug build, or review both CI matrix results. Read verbose test
output for skips. Run the staged executable on a native desktop for actual video,
then carry out the live and hardware checks in [testing](testing.md). Include the
build commit and sanitized test evidence in the release record.

## Create a source archive

After the release files are committed and the working tree is clean:

```sh
git status --short
git tag -a v0.1.0 -m 'Sofafin 0.1.0 preview'
python3 scripts/package-source.py --ref v0.1.0
```

Use a signed tag instead if you maintain a signing key. These commands are a
maintainer example, not actions run automatically by CI. The script needs Git and
Python 3 and creates:

- `dist/sofafin-0.1.0.tar.gz`, rooted at `sofafin-0.1.0/`.
- `dist/sofafin-0.1.0.tar.gz.sha256`, containing the archive's SHA-256 digest.

It reads the version and GPL license from the selected commit, rejects a numeric
release tag that disagrees with the CMake version, and refuses to overwrite an
existing output. Use `--output-dir /another/directory` for a separate candidate.
Because `git archive` is used, untracked settings, build directories, local `.env`
files, and Git metadata are excluded. Accidentally committed secrets are still
included, so a source/history review remains necessary.

Verify and rebuild the actual artifact:

```sh
cd dist
sha256sum --check sofafin-0.1.0.tar.gz.sha256
tar -tzf sofafin-0.1.0.tar.gz
tar -xzf sofafin-0.1.0.tar.gz
cd sofafin-0.1.0
python3 scripts/check-docs.py
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --parallel 4
QT_QPA_PLATFORM=offscreen ./build/sofafin --version
```

Inspect the archive list for source, QML, assets, tests, packaging, scripts,
documentation, policies, and `LICENSE`, with no build/test-account artifacts.
Build the archive with `BUILD_TESTING=ON` as well if it differs from the tested
checkout. A checksum detects file changes; it is not an author signature.

## Publish

After review and authorization to publish:

1. Make the reviewed repository available at the canonical public URL and verify
   contribution, issue, and private security-reporting routes.
2. Push the reviewed commit and intended tag to that repository. Never retag a
   release users may already have downloaded.
3. Create a release from the exact tag, mark previews as prereleases, and attach
   the verified source tarball and checksum sidecar.
4. Include the changelog, build/install link, tested platforms and server versions,
   remaining limits, and whether any hardware gates remain open.
5. Download the public artifacts, verify checksums, and check the release page's
   links as a reader without repository write access.

The CI workflow only uploads development artifacts; it does not publish or have
repository write permissions. No automatic hosted-release workflow is enabled.

## Downstream packaging

- Build with `-DBUILD_TESTING=OFF` for the shipping executable after testing the
  candidate with tests enabled. Use CMake's GNU install directories and `DESTDIR`.
- Ship the `sofafin` executable, `moe.ckl.sofafin.desktop`, matching hicolor icon,
  project license, notices, and documentation. Do not ship test executables or the
  test-only hardware guard. The desktop file starts `sofafin --fullscreen`.
- Declare runtime dependencies on matching Qt libraries, QML modules, SVG/platform
  plugins, SDL2, libmpv, and platform graphics/audio libraries. Qt D-Bus is required.
  The normal app does not need the `mpv` or `ffmpeg` command-line executables.
- Keep the corresponding GPL source available for the binaries you distribute,
  including local patches/build material as applicable. Include the actual bundled
  dependency notices/source obligations; see [THIRD_PARTY.md](../THIRD_PARTY.md).
- Test the package in a clean target environment. A working binary on the build
  machine does not establish Qt/glibc/libmpv compatibility on SteamOS.
- Do not label a bare staged install tarball as a portable bundle. Flatpak/AppImage
  need separate manifests, dependency/license inventory, sandbox permissions, and
  physical device validation before they are supported release formats.

## After release

Open the next Unreleased development period, triage incoming regressions, retain
test evidence, and document critical fixes in the changelog. Publish a new patch
version for changed release artifacts. Keep limitations accurate as physical
validation and server compatibility evidence expand.
