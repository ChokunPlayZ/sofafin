# User guide

## Sign in and out

Enter a full base URL, such as `http://192.168.1.10:8096` or
`https://media.example.com/jellyfin`, followed by your Jellyfin username and
password. Use the server's root or configured base path, not `/web/` or a browser
page URL. URLs with embedded credentials, queries, or fragments are rejected.
HTTPS certificates must be trusted; HTTP sends credentials without encryption.

Select a field and press A, tap it, or press F2 to request Steam's on-screen
keyboard. Steam must be running and registered to handle `steam://` URLs. Text is
sent to the focused field, including masked password fields. A physical keyboard
works without Steam. Enter advances through the form.

**Keep me signed in** stores a reusable access token, not the password. On the
next launch a loading screen remains until session restoration completes. An
expired session asks you to sign in again. Open Settings and choose **Sign out**
to clear the token and request logout on the server. See [privacy](privacy.md).

## Browse your libraries

Home shows continue watching, next episodes, and recent titles. Top tabs follow
the server's video-library names and order. Use LB/RB or Page Up/Page Down to
change tabs. Libraries and folders open in a poster grid with pagination, filters
for favorites/watched status, and sorting by name, date added, or rating.
Y / F opens search while browsing.

Select a title to open its full-screen details, with artwork, ratings, genres,
overview, playback actions, favorites, and watched/unwatched actions. Cast,
chapters, and playback settings expand independently. Resume shows saved playback
progress. Choose a chapter to start from that point.

A series has one episode row per season. **Episodes** jumps to the rows; opening
an episode shows its own details. B returns to the same season and episode
selection. An episode's series or Season link returns to the appropriate series
location. Skeletons and BlurHash placeholders appear while content loads.

Themes supplied by Jellyfin play on details pages, including a series theme
inherited by its episodes. The same theme continues across related series/episode
navigation. Leaving details or starting video stops it. Theme volume and the
on/off switch are in Settings; missing themes are ignored.

## Controls

| Controller | Keyboard | Action |
| --- | --- | --- |
| D-pad / left stick | Arrow keys | Move focus; preview a seek while the video/timeline has focus |
| A | Enter | Activate selection; pause/resume when video has focus |
| A in a field | F2 | Request Steam keyboard |
| B | Escape | Back, dismiss dialog/controls, cancel seek, or stop confirmation |
| X | Space | Pause / resume video |
| Y | F | Search while browsing |
| LB / RB | Page Up / Page Down | Cycle library tabs |
| Menu / Start | F1 | Global Settings |
| View / Back | I | Media information in the player |
| — | F11 | Toggle fullscreen |

SDL selects the first recognized available controller and supports disconnecting
and reconnecting it. Button labels follow Xbox/Steam conventions. Use a **Gamepad**
Steam Input template so Steam delivers controller events consistently. Mouse/touch
selection and scrolling are also supported.

## Seek and player focus

Move the left stick horizontally to open a draft seek, including when controls
are hidden. Small deflections move slowly, full deflections move faster, and
holding the stick accelerates. D-pad Left/Right and keyboard arrows also open a
preview. Jellyfin trickplay thumbnails appear when available.

- A / Enter commits the preview. B / Escape cancels it.
- Pointer dragging seeks when released.
- Down from the video selects an available skip/next-episode prompt, otherwise the timeline.
- Down from a prompt reaches the timeline; Down again reaches pause, playback settings, and stop.
- Up from the timeline focuses an available prompt, including a dismissed prompt.
  With no prompt, Up hides the controls.

The timeline displays chapter ticks, colored media segments, and a legend. The
preview shows the chapter and segment at the draft position. Download speed,
buffer progress, estimated end time, and episode details appear when available.

## Playback choices and stopping

Expand playback settings on the detail page to choose quality, tracks, and a
media version before starting. The player's playback settings group quality,
audio, subtitles, and media information. Automatic track labels name the resolved
track; long names also appear in tooltips/media info. In-player quality and track
changes keep the current position and pause state. See [playback](playback.md).

B / Escape hides controls and prompts. Press it again, or choose Stop, to open a
confirmation. Playback pauses while the dialog is open. **Keep playing** is
focused initially; B / Escape cancels and restores the previous pause state.
After ten seconds paused without input, the video dims and shows the title.
B on Home opens an exit prompt.

## Skip segments and next episodes

An active intro, outro/credits, recap, preview, or commercial segment can display
a skip button. These ranges come from Jellyfin, or from explicitly named chapters
when the server provides no media segments. See [segment behavior](playback.md#chapters-segments-and-trickplay).

Episodes continue in server order, including across seasons. During the final
30 seconds, a translucent next-episode prompt displays the next title and a
countdown following the current playback position. If a final skip segment
reaches the end, the prompt can start at that segment instead.

- **Start now** switches immediately to the next episode.
- **Close** cancels autoplay for this episode and exits the player when it ends.
- B / Escape hides the prompt while keeping autoplay enabled. Up from the
  timeline restores and focuses it; Down from the video focuses Start now.
- Left/Right moves between Start now and Close. Pausing also pauses the countdown.

The next episode starts from its beginning, retaining the current quality choice
and using its own default tracks. Missing next-episode data, the final episode,
and playback errors do not advance. Prebuffering is described in
[playback](playback.md#buffering-and-next-episode-prefetch).
