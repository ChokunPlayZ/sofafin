# Playback behavior

## Direct play, transcoding, and quality

Sofafin posts a libmpv device profile to Jellyfin's PlaybackInfo API and uses the
returned media sources. Automatic mode prefers a directly playable version across
the returned sources before considering HLS transcoding. Direct-only mode fails
if no suitable direct source exists. Forced transcoding disables audio/video
stream copying and requests H.264/AAC HLS with stereo audio.

| Quality selection | Policy |
| --- | --- |
| Automatic | Use global playback mode and streaming limit |
| Original | Require direct play |
| 4 / 8 / 20 / 40 / 80 Mbps | Request server transcoding at that bitrate |

The streaming limit is a negotiation request, not a client download-speed limiter.
Actual codec support depends on the linked libmpv/FFmpeg and available decoder
drivers. The profile advertises common video containers/codecs and text/embedded
subtitle formats; it does not certify every combination on every GPU.

Select a media version in detail-page playback settings before starting. A
specified version is respected during negotiation. In-player quality and track
changes renegotiate when necessary and preserve source position and pause state.
Transcoded seeks request a new stream at the chosen source position, keeping the
timeline on the original media's clock. Previewing a seek does not repeatedly
restart a transcoder.

## Tracks and subtitles

Automatic uses the source's Jellyfin default tracks. Subtitle Off explicitly
disables subtitles. Track indexes come from Jellyfin and are mapped to observed
libmpv tracks; source indexes are not assumed to equal mpv track IDs.

The profile includes external SRT, ASS, SSA, SubRip, and VTT, plus embedded PGS/DVD
subtitles. Delivery and burn-in behavior also depend on the server's negotiated
response. External subtitle URLs must have the server's origin. Track names and
resolved automatic choices appear in playback settings; media info includes
container, bitrate, size, codecs, resolution, and source tracks.

The application requests stereo audio. It exposes theme-song volume separately;
there is no main-video volume or audio-passthrough preference. Use system audio
controls for video volume.

## Buffering and next-episode prefetch

Each video player/cache can prefetch up to one hour, bounded by **1 GiB forward**
cache and **256 MiB backward** cache. These are ceilings consumed as media arrives,
not upfront allocations. Startup, committed seeks, and underruns target five
seconds of buffered media, or less if the cache fills or the stream ends.
The seek buffering indicator stays visible until playback is ready. Network
speed and buffer progress are displayed when mpv reports them.

Only after the current episode is cached to its end does Sofafin negotiate and
prebuffer the next episode. A cache stopped at its memory limit is not considered
fully buffered. Autoplay and Start now reuse the prepared session/cache. Both
episode caches may coexist, so total memory can exceed one cache's limits.

Closing the next-episode prompt, stopping, selecting another title, or changing
playback options discards speculative playback and requests cleanup of unused
transcodes. Failed or unfinished prefetch falls back to a normal next-episode
load. Cache sizes and autoplay behavior currently have no global UI preference.

## Chapters, segments, and trickplay

Chapters load when playback starts, including when starting from shelves. The
timeline marks chapter starts and server-supplied Intro, Outro, Recap, Preview,
and Commercial ranges. Active ranges can offer a skip button.

When Jellyfin's Media Segment API returns no ranges, explicitly named chapters
such as Intro, Opening, OP, Outro, ED, and Credits can define skip ranges ending
at the following chapter or title end. Other names remain chapter markers.
Servers without segment data can still play media normally. Thumbnails require
Jellyfin-generated trickplay images; missing trickplay does not prevent seeking.

Next-episode ordering, countdown, cancellation, and focus are described in the
[user guide](usage.md#skip-segments-and-next-episodes).

## Hardware decoding and rendering

The application embeds libmpv's OpenGL render API in a Qt Quick framebuffer.
Normal video playback requires that native renderer. `QT_QUICK_BACKEND=software`
is for headless UI validation; test executables use a separate software mpv render
surface and do not validate the production video surface.

On Linux, Automatic hardware decoding tries VA-API, VA-API copy, NVDEC, and NVDEC
copy, falling back to software. Older mpv versions that reject decoder lists use
`vaapi-copy` with software fallback. Vulkan decoding is deliberately excluded:
automatic probing can abort in Gamescope's injected Vulkan WSI layer. The app does
not alter Steam's graphics settings or overlay.

HDR output and surround passthrough are not certified. See
[troubleshooting](troubleshooting.md) if decoding fails or the video surface is black.

## Session reporting and idle behavior

Sofafin reports playback start, progress (every ten seconds), and stop to Jellyfin.
It requests cleanup of transcodes when sessions end or speculative streams are
discarded. Theme songs use a separate audio-only player and do not create video
watch-progress sessions. Favorites and watched actions also update your server account.

Active, unpaused video requests idle inhibition through the session D-Bus
`org.freedesktop.ScreenSaver` service. It stays active during loading, buffering,
and seeking; pause, stop, end, error, and exit release it. Theme audio does not
inhibit idle. Missing/restarting services are retried without blocking playback.
Gaming Mode's response depends on the installed Steam client and its support for
media-app idle inhibition; verify this on the actual device.
