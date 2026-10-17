# Usability validation — 17 October 2026

Tested against the supplied Jellyfin server with the disposable testing account. Credentials were supplied through the process environment and are absent from the source and report. Each native run used isolated settings. Favorite changes were restored; playback reporting updated the test account's watch progress.

## Display and input coverage

| Screen and window | UI scales checked | Final live flow |
| --- | --- | --- |
| 1920×1080 | 75%, 100%, 150% | Passed — 42.8 s |
| 2560×1440 | 75%, 100%, 150% | Passed — 43.8 s |
| 3840×2160 | 75%, 100%, 150% | Passed — 59.8 s |

Qt's offscreen screen geometry, application window, and PNG dimensions match each resolution at DPR 1. All 45 saved screenshots were checked for those exact dimensions. Navigation uses native Qt keyboard events and an SDL virtual gamepad, including analog axis values. The live runner decodes actual server video with libmpv's software render API on a separate thread. It checks that the video surface retains the display's full dimensions independently of UI scaling.

## Scenarios exercised

- Sign in through the fields and button, loading screen, home shelves, and available server library tabs. Music stays outside the supported video libraries.
- Browse Anime, Movies, and Series with real artwork and metadata; navigate poster grids; paginate; switch favorites filters and rating sort; search.
- Open a series, reach a season, select an episode, return to the same season/episode selection, and reopen the episode.
- Decode a real series theme and verify that the player does not reload it when entering an episode in that series. The theme case used **2.5 Dimensional Seduction**.
- Toggle and restore a favorite, expand playback settings, and inspect automatic audio/subtitle labels with real track names.
- Decode direct playback; pause; select the timeline; preview with arrows and a virtual stick; compare slow/full deflection speed; load a real trickplay sheet; confirm a seek; drag the playhead with the pointer.
- Turn subtitles off, restore automatic subtitles, request 8 Mbps transcoding, and seek while preserving pause state.
- Open settings at each scale, reach the scale control, adjust it, verify its bounds, and return to browsing.

The standalone live checker also authenticated, browsed four server libraries, and decoded both direct and transcoded samples with external mpv.

## Findings and changes

| Finding | Result |
| --- | --- |
| Every detail navigation stopped and restarted the theme | Preserve the theme while browsing the same series; compare song identity before loading. Stale requests, leaving details, disabling themes, and starting video still cancel it. |
| The scale preference changed posters only | Scale the full interface and popups, save 75–150% in 5% steps, retain focus when adjusting, and adapt the home layout at larger scales. Keep video at native display resolution and request sharper artwork. |
| Timeline was clickable but unavailable to controller focus | Add a focusable playhead, draft seeking, variable stick speed, hold acceleration, thumbnail previews, explicit confirm/cancel, and pointer dragging. Previewing does not repeatedly restart a transcoded stream. |
| Automatic tracks hid the selected language/track | Display the resolved default alongside Automatic, including Off for subtitles. Long labels retain full tooltip and media-info text. |
| Empty shelves appeared while data was pending | Add skeletons for shelves, grids, title details, season rows, and artwork without BlurHash. Publish individual season results as they arrive. |
| A remembered session briefly showed sign-in fields | Initialize authentication state before QML is loaded and show a dedicated loading screen through session restoration. |
| Missing episodes were selected by the live checker | Exclude missing entries in playable catalog, resume/next-up, and checker queries. |
| Trickplay requests returned 401 with `api_key` | Use the server-supported `ApiKey` parameter for authenticated trickplay sheets. |
| Paused-screen timer could dismiss a seek preview | Show it only when focus has returned to the video. Keep seek instructions and preview clear of title text. |
| A 4K stream-change run froze in a synchronous mpv track-list query | Consume observed/asynchronous track data, cache stream mappings, and defer selections until tracks arrive. The final 4K flow completed successfully. |

During investigation, one further 4K run timed out waiting for transcoded loading after the track-query fix; a subsequent full run passed. This was not reproduced in the successful final run or standalone transcoding check. Stream-start latency remains worth monitoring on this server.

## Automated checks

`ctest --test-dir build --output-on-failure`: **all four suites passed** (native UI, core, playback startup, UI smoke). New checks cover pending skeletons, successful/expired session restoration, theme continuity, analog speed/acceleration, deferred seek commit/cancel, trickplay tile boundaries, and scale-control visibility while adjusting. Existing real libmpv tests verify audio/subtitle selection, decoding, pause, seek, and restart.

Both the development application and release application were built. The release executable is `build-release/sofafin`.

The opt-in integration runner is `build/sofafin-live-ui-tests`; see the README for environment variables and resolution commands. Screenshots and per-resolution logs from this run were saved on the testing host, including `1080p-home.png`, `1080p-tracks.png`, `2160p-seeking.png`, and scale screenshots at all three resolutions.

## Hardware limits

These are headless tests at actual pixel dimensions. Video is decoded into a separate test surface, so the video area in UI screenshots is black. Physical Steam Deck controls, Steam's keyboard overlay, speaker output, native OpenGL/Gamescope presentation, GPU decoding, HDR, and TV overscan still need hardware validation.
