import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import JFGamescope 1.0

FocusScope {
    id: playback
    objectName: "playbackView"
    property color accent: "#66c0f4"
    property bool controlsVisible: true
    property bool pauseScreenVisible: false
    property bool popupClosing: false
    readonly property bool isPaused: Player.active && Player.paused
    readonly property var media: App.currentItem
    readonly property string heading: media.seriesName || App.currentTitle
    readonly property string episode: media.type === "Episode"
        ? (media.seasonNumber !== undefined && media.seasonNumber !== null && media.episodeNumber !== undefined && media.episodeNumber !== null
           ? "S" + media.seasonNumber + " · E" + media.episodeNumber + "  " : "") + App.currentTitle : ""
    readonly property var segment: {
        const position = Player.position
        return App.mediaSegments.find(s => position >= s.start && position < s.end) || ({})
    }
    readonly property var finalSegment: {
        const segments = App.mediaSegments
        const last = segments.length ? segments[segments.length - 1] : ({})
        // Segment providers and the decoded duration can differ by a fraction of a second.
        return Player.duration > 0 && Math.abs(last.end - Player.duration) <= 1 ? last : ({})
    }
    readonly property real nextEpisodeStart: finalSegment.start !== undefined
        ? finalSegment.start : Math.max(0, Player.duration - 30)
    readonly property int nextEpisodeSeconds: Math.max(0, Math.ceil(Player.duration - Player.position))
    readonly property string nextEpisodeInfo: {
        const next = App.nextEpisode
        const number = next.seasonNumber !== undefined && next.seasonNumber !== null
            && next.episodeNumber !== undefined && next.episodeNumber !== null
            ? "S" + next.seasonNumber + " · E" + next.episodeNumber + "  " : ""
        return number + (next.title || "")
    }
    property date clockTime: new Date()
    readonly property string endingTime: Player.duration > 0
        ? new Date(clockTime.getTime() + Math.max(0, Player.duration - Player.position) * 1000).toLocaleTimeString(Qt.locale(), "hh:mm") : ""
    visible: Player.active || App.playbackPending
    onVisibleChanged: {
        pauseScreenVisible = false
        if (visible) { controlsVisible = true; focusVideo(); hideControls.restart() }
        else { pauseDelay.stop(); hideControls.stop() }
    }
    onIsPausedChanged: {
        pauseScreenVisible = false
        if (isPaused) { pauseDelay.restart(); controlsVisible = true }
        else { pauseDelay.stop(); if (visible) hideControls.restart() }
    }
    onActiveFocusChanged: if (activeFocus && isPaused && !pauseScreenVisible) pauseDelay.restart()
    function time(seconds) {
        const s = Math.max(0, Math.floor(seconds))
        const minutes = Math.floor(s / 60) % 60
        return (s >= 3600 ? Math.floor(s / 3600) + ":" + (minutes < 10 ? "0" : "") : "")
            + (s >= 3600 ? minutes : Math.floor(s / 60)) + ":" + (s % 60 < 10 ? "0" : "") + s % 60
    }
    function transferRate(bytesPerSecond) {
        if (bytesPerSecond >= 1000000) return (bytesPerSecond / 1000000).toFixed(1) + " MB/s"
        if (bytesPerSecond >= 1000) return (bytesPerSecond / 1000).toFixed(0) + " kB/s"
        return Math.max(0, bytesPerSecond).toFixed(0) + " B/s"
    }
    function activity() {
        pauseScreenVisible = false; controlsVisible = true; hideControls.restart()
        if (isPaused) pauseDelay.restart()
    }
    function focusVideo() {
        const item = playback.Window.window.activeFocusItem
        if (item && item !== playback) item.focus = false
        playback.forceActiveFocus()
    }
    function hideOverlay() { controlsVisible = false; pauseScreenVisible = false; focusVideo(); if (isPaused) pauseDelay.restart() }
    function back() {
        if (pauseScreenVisible || controlsVisible) hideOverlay()
        else App.stopPlayback()
    }
    function settings(section) {
        activity(); trackPopup.open()
        Qt.callLater(() => {
            if (section === "audio") tracks.focusAudio()
            else if (section === "subtitle") tracks.focusSubtitle()
            else tracks.focusQuality()
        })
    }
    Connections {
        target: Controller
        function onAxisChanged() {
            if (Controller.horizontalAxis === 0 || !playback.visible || !seekBar.enabled
                || playback.popupClosing || trackPopup.visible || infoPopup.visible) return
            const window = playback.Window.window
            if (!window || !window.active || window.activeFocusItem !== playback) return
            playback.activity()
            // Claim this axis event before it becomes a navigation key.
            seekBar.forceActiveFocus()
            seekBar.begin()
        }
    }
    Keys.onPressed: event => {
        if (popupClosing || trackPopup.visible || infoPopup.visible) { event.accepted = true; return }
        if (App.playbackPending && event.key !== Qt.Key_Escape) { event.accepted = true; return }
        if (event.key === Qt.Key_Escape) back()
        else if (event.key === Qt.Key_Up) hideOverlay()
        else {
            activity()
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) Player.togglePause()
            else if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
                if (seekBar.enabled) {
                    seekBar.forceActiveFocus()
                    seekBar.nudgeKey(event.key === Qt.Key_Left ? -1 : 1)
                }
            } else if (event.key === Qt.Key_Down) {
                if (nextEpisodePanel.visible) nextEpisodeNow.forceActiveFocus()
                else if (skipButton.visible) skipButton.forceActiveFocus()
                else seekBar.forceActiveFocus()
            }
            else if (event.key === Qt.Key_I) infoPopup.open()
            else return
        }
        event.accepted = true
    }
    Shortcut { sequence: "Space"; enabled: playback.visible && playback.activeFocus; onActivated: { playback.activity(); Player.togglePause() } }
    Rectangle { anchors.fill: parent; color: "black" }
    // Keep the video framebuffer at display resolution when the UI is scaled.
    MpvVideo {
        objectName: "videoSurface"
        readonly property real uiFactor: playback.Window.window ? playback.Window.window.uiFactor : 1
        width: playback.width * uiFactor; height: playback.height * uiFactor
        scale: 1 / uiFactor; transformOrigin: Item.TopLeft
        player: Player; visible: Player.active
    }
    // Qt retains the framebuffer while hidden. Cover it until the new file
    // has produced its first frame, including a stop/start within one UI tick.
    Rectangle {
        objectName: "videoBlank"
        anchors.fill: parent; color: "black"
        visible: !Player.hasFrame || App.playbackPending
    }
    MouseArea {
        anchors.fill: parent; hoverEnabled: true
        onPositionChanged: playback.activity()
        onClicked: { playback.activity(); playback.focusVideo(); Player.togglePause() }
    }
    Column {
        objectName: "bufferIndicator"
        anchors.centerIn: parent; spacing: 8
        visible: (Player.loading || App.playbackPending) && !playback.pauseScreenVisible
        BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: parent.visible; width: 64; height: 64 }
        Text {
            objectName: "bufferStatus"
            anchors.horizontalCenter: parent.horizontalCenter
            text: App.playbackPending ? "Preparing…" : "Buffering"
                + (Player.bufferPercent > 0 ? " " + Player.bufferPercent + "%" : "")
                + (Player.networkSpeed >= 0 ? " · " + playback.transferRate(Player.networkSpeed) : "")
            color: "white"; font.pixelSize: 16
        }
    }
    Rectangle {
        id: pauseScreen
        objectName: "pauseScreen"
        anchors.fill: parent; color: "#c4080c12"
        visible: playback.pauseScreenVisible
        Column {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: Math.max(32, playback.width * 0.065) }
            spacing: 14
            Text { text: "Paused"; color: "#b8bdc5"; font.pixelSize: 17 }
            Text { width: parent.width; text: playback.heading; color: "white"; font.pixelSize: Math.min(46, playback.width * 0.045); font.weight: Font.Bold; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight }
            Text { width: parent.width; text: playback.episode; visible: text.length > 0 && !seekBar.seeking; color: "#d6dae0"; font.pixelSize: 20; wrapMode: Text.Wrap }
            Text { width: Math.min(parent.width, 720); text: playback.media.overview || ""; visible: text.length > 0; color: "#adb4bf"; font.pixelSize: 16; maximumLineCount: 3; elide: Text.ElideRight; wrapMode: Text.Wrap }
            Text { text: playback.time(Player.position) + " / " + playback.time(Player.duration); color: "#adb4bf"; font.pixelSize: 14 }
        }
    }
    Rectangle {
        id: overlay
        objectName: "playerOverlay"
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: overlayColumn.implicitHeight + 95
        visible: playback.controlsVisible && !playback.pauseScreenVisible
        gradient: Gradient { GradientStop { position: 0; color: "#00000000" } GradientStop { position: 0.45; color: "#d906101a" } GradientStop { position: 1; color: "#f5080c12" } }
        ColumnLayout {
            id: overlayColumn
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 28 }
            spacing: 10
            Text { objectName: "playerTitle"; visible: !seekBar.seeking; Layout.fillWidth: true; text: playback.heading; color: "white"; font.pixelSize: 25; font.weight: Font.DemiBold; elide: Text.ElideRight }
            Text { objectName: "playerEpisode"; Layout.fillWidth: true; text: playback.episode; visible: text.length > 0 && !seekBar.seeking; color: "#c3cbd4"; font.pixelSize: 16; elide: Text.ElideRight }
            SeekBar {
                id: seekBar
                z: 2
                Layout.fillWidth: true
                accent: playback.accent
                onActivity: playback.activity()
                onMoveUp: playback.hideOverlay()
                onMoveDown: pauseButton.forceActiveFocus()
            }
            RowLayout {
                Layout.fillWidth: true
                Text { text: (seekBar.seeking ? "Seek to " : Player.paused ? "Paused · " : "") + playback.time(seekBar.position) + " / " + playback.time(Player.duration); color: "#c3d7e4"; font.pixelSize: 14 }
                Item { Layout.fillWidth: true }
                Text { objectName: "endingTime"; visible: playback.endingTime.length > 0; text: "Ends at " + playback.endingTime; color: "#c3d7e4"; font.pixelSize: 14 }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                NavButton { id: pauseButton; objectName: "playerPause"; text: Player.paused ? "Resume" : "Pause"; primary: true; accent: playback.accent; implicitWidth: 120; upTarget: seekBar; onClicked: { playback.activity(); Player.togglePause() } }
                Item { Layout.fillWidth: true }
                NavButton { objectName: "playerSettings"; text: "Playback settings"; flat: true; upTarget: seekBar; onClicked: playback.settings("quality") }
                NavButton { text: "Stop"; flat: true; implicitWidth: 90; upTarget: seekBar; onClicked: App.stopPlayback() }
            }
        }
    }
    NavButton {
        id: skipButton
        objectName: "skipSegmentButton"
        anchors { right: parent.right; bottom: parent.bottom; margins: 28; bottomMargin: overlay.visible ? overlay.height + 12 : 35 }
        visible: !!playback.segment.type && !playback.pauseScreenVisible && !seekBar.seeking && !App.playbackPending
            && !nextEpisodePanel.visible
            && !(App.nextEpisode.id && playback.segment.start === playback.finalSegment.start)
        text: "Skip " + ({Intro: "intro", Outro: "credits", Recap: "recap", Preview: "preview", Commercial: "commercial"})[playback.segment.type]
        primary: true; accent: playback.accent
        upTarget: playback; downTarget: seekBar
        onNavigatedUp: playback.hideOverlay()
        onClicked: { playback.activity(); App.skipSegment(); playback.focusVideo() }
        onVisibleChanged: if (!visible && activeFocus) playback.focusVideo()
    }
    Rectangle {
        id: nextEpisodePanel
        objectName: "nextEpisodePrompt"
        anchors { right: parent.right; bottom: parent.bottom; margins: 28; bottomMargin: overlay.visible ? overlay.height + 12 : 35 }
        width: Math.min(440, playback.width - 56)
        height: nextEpisodeColumn.implicitHeight + 32
        radius: 8; color: "#ed15202d"; border.color: "#425263"
        visible: Player.active && Player.duration > 0 && App.autoplayNextEpisode && !App.playbackPending
            && !seekBar.seeking && Player.position >= playback.nextEpisodeStart
        onVisibleChanged: if (!visible && (nextEpisodeNow.activeFocus || nextEpisodeClose.activeFocus)) playback.focusVideo()
        Keys.onEscapePressed: { App.cancelNextEpisode(); playback.focusVideo() }
        ColumnLayout {
            id: nextEpisodeColumn
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
            spacing: 10
            Text {
                objectName: "nextEpisodeCountdown"
                Layout.fillWidth: true
                text: "Next Episode in " + playback.nextEpisodeSeconds + " seconds"
                color: "white"; font.pixelSize: 20; font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }
            Text {
                objectName: "nextEpisodeInfo"
                Layout.fillWidth: true
                text: playback.nextEpisodeInfo
                color: "#c3cbd4"; font.pixelSize: 16
                wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                NavButton {
                    id: nextEpisodeNow
                    objectName: "nextEpisodeNow"
                    text: "Start now"; primary: true; accent: playback.accent
                    Layout.fillWidth: true
                    upTarget: playback; downTarget: seekBar; rightTarget: nextEpisodeClose; leftTarget: nextEpisodeClose
                    onNavigatedUp: playback.hideOverlay()
                    onClicked: App.playNextEpisode()
                }
                NavButton {
                    id: nextEpisodeClose
                    objectName: "nextEpisodeClose"
                    text: "Close"
                    upTarget: playback; downTarget: seekBar; leftTarget: nextEpisodeNow; rightTarget: nextEpisodeNow
                    onNavigatedUp: playback.hideOverlay()
                    onClicked: { App.cancelNextEpisode(); playback.focusVideo() }
                }
            }
        }
    }
    Timer { id: hideControls; interval: 4000; onTriggered: if (playback.Window.window.activeFocusItem === playback && !trackPopup.visible && !infoPopup.visible && !Player.paused) playback.controlsVisible = false }
    Timer { id: pauseDelay; objectName: "pauseScreenTimer"; interval: 10000; onTriggered: if (playback.isPaused && playback.Window.window.activeFocusItem === playback && !trackPopup.visible && !infoPopup.visible) { playback.controlsVisible = false; playback.pauseScreenVisible = true; playback.focusVideo() } }
    Timer { interval: 1000; repeat: true; running: playback.visible; onTriggered: playback.clockTime = new Date() }
    Popup {
        id: trackPopup
        objectName: "playerTrackPopup"
        parent: Overlay.overlay
        width: Math.min(520, parent.width - 48); height: trackColumn.implicitHeight + 48
        x: (parent.width - width) / 2; y: (parent.height - height) / 2
        modal: true; focus: true; padding: 24; closePolicy: Popup.CloseOnEscape
        onAboutToHide: playback.popupClosing = true
        onClosed: Qt.callLater(() => { playback.hideOverlay(); playback.popupClosing = false })
        background: Rectangle { color: "#15202d"; radius: 8; border.color: "#425263" }
        contentItem: ColumnLayout {
            id: trackColumn; spacing: 16
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Playback"; color: "white"; font.pixelSize: 24; font.weight: Font.DemiBold; Layout.fillWidth: true }
                NavButton { objectName: "playerInfo"; text: "Info"; flat: true; implicitWidth: 70; onClicked: infoPopup.open() }
                IconButton { text: "Close"; iconName: "close"; onClicked: trackPopup.close() }
            }
            PlaybackOptions {
                id: tracks
                namePrefix: "playing"; live: true; Layout.fillWidth: true
                sources: [App.currentSource]
                quality: App.playbackOptions.quality || 0
                audio: App.playbackOptions.audio === undefined ? -2 : App.playbackOptions.audio
                subtitle: App.playbackOptions.subtitle === undefined ? -2 : App.playbackOptions.subtitle
                enabled: !App.playbackPending
                onQualitySelected: value => { App.setQuality(value); trackPopup.close() }
                onAudioSelected: value => { App.setPlaybackTrack(false, value); trackPopup.close() }
                onSubtitleSelected: value => { App.setPlaybackTrack(true, value); trackPopup.close() }
            }
        }
    }
    Popup {
        id: infoPopup
        objectName: "mediaInfoPopup"
        parent: Overlay.overlay
        width: Math.min(680, parent.width - 48); height: Math.min(520, parent.height - 48)
        x: (parent.width - width) / 2; y: (parent.height - height) / 2
        modal: true; focus: true; padding: 24; closePolicy: Popup.CloseOnEscape
        onOpened: infoDone.forceActiveFocus()
        onAboutToHide: playback.popupClosing = true
        onClosed: Qt.callLater(() => { if (trackPopup.visible) tracks.focusQuality(); else playback.hideOverlay(); playback.popupClosing = false })
        background: Rectangle { color: "#15202d"; radius: 8; border.color: "#425263" }
        contentItem: ColumnLayout {
            spacing: 16
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Media info"; color: "white"; font.pixelSize: 24; font.weight: Font.DemiBold; Layout.fillWidth: true }
                IconButton { id: infoDone; text: "Close"; iconName: "close"; onClicked: infoPopup.close() }
            }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
                Text { width: parent.width; text: (App.playMethod === "DirectPlay" ? "Direct play" : "Transcoding") + "\n" + (App.currentSource.mediaInfo || "") + "\n\nAudio: " + Player.audioTrack + "\nSubtitles: " + Player.subtitleTrack; color: "#c3cbd4"; font.pixelSize: 16; wrapMode: Text.Wrap }
            }
        }
    }
}
