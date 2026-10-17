import QtQuick
import QtQuick.Controls

FocusScope {
    id: seek
    objectName: "playerSeekBar"
    property color accent: "#66c0f4"
    property bool seeking: false
    property real previewPosition: 0
    property real heldSeconds: 0
    property real lastTick: 0
    property int direction: 0
    property real lastKeyTime: 0
    property int keyRepeats: 0
    readonly property real axis: activeFocus && visible ? Controller.horizontalAxis : 0
    readonly property real speed: axis === 0 ? 0 : (4 + 116 * Math.pow((Math.abs(axis) - 0.2) / 0.8, 2)) * (1 + Math.min(3, heldSeconds / 2))
    readonly property real position: seeking ? previewPosition : Player.position
    readonly property real fraction: Player.duration > 0 ? Math.max(0, Math.min(1, position / Player.duration)) : 0
    readonly property var frame: seeking ? App.trickplayFrame(previewPosition) : ({})
    readonly property var chapters: Player.duration > 0 ? (App.currentItem.chapters || [])
        .filter(chapter => Number.isFinite(chapter.ticks) && chapter.ticks >= 0 && chapter.ticks / 10000000 < Player.duration)
        .sort((a, b) => a.ticks - b.ticks) : []
    readonly property var segments: Player.duration > 0 ? App.mediaSegments
        .filter(segment => segment.start < Player.duration) : []
    readonly property var segmentTypes: [...new Set(segments.map(segment => segment.type))]
    readonly property string contextLabel: {
        let chapterName = ""
        for (let i = 0; i < chapters.length && chapters[i].ticks / 10000000 <= position; ++i)
            chapterName = chapters[i].name || "Chapter " + (i + 1)
        const segment = segments.find(segment => position >= segment.start && position < segment.end)
        return [chapterName, segment ? segmentName(segment.type) : ""].filter(Boolean).join(" · ")
    }
    signal activity()
    signal moveUp()
    signal moveDown()
    signal backRequested()
    implicitHeight: markerLegend.visible ? 40 + markerLegend.implicitHeight : 36
    activeFocusOnTab: true
    enabled: Player.active && Player.duration > 0 && !App.playbackPending
    Accessible.role: Accessible.Slider
    Accessible.name: "Playback position"
    Accessible.description: "Select to seek. Move the stick to preview, select to play, Back to cancel."
    onActiveFocusChanged: {
        Controller.timelineMode = activeFocus && visible
        if (!activeFocus) cancel()
        else activity()
    }
    onVisibleChanged: if (!visible) { Controller.timelineMode = false; cancel() }
    Component.onDestruction: Controller.timelineMode = false
    function time(seconds) {
        const s = Math.max(0, Math.floor(seconds))
        return Math.floor(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + s % 60
    }
    function segmentName(type) {
        return ({Intro: "Intro", Outro: "Credits", Recap: "Recap", Preview: "Preview", Commercial: "Commercial"})[type] || type
    }
    function segmentColor(type) {
        return ({Intro: "#f2c66d", Outro: "#ba9cf5", Recap: "#68d4bb", Preview: "#ed9bc4", Commercial: "#f59a73"})[type] || "#c3d7e4"
    }
    function begin() {
        if (!enabled || seeking) return
        previewPosition = Player.position; seeking = true; activity()
    }
    function cancel() { seeking = false; heldSeconds = 0; direction = 0 }
    function commit() {
        if (!seeking) return
        const target = previewPosition
        cancel(); App.seekRelative(target - Player.position); activity()
    }
    function nudge(seconds) {
        begin()
        previewPosition = Math.max(0, Math.min(Math.max(0, Player.duration - 1), previewPosition + seconds))
        activity()
    }
    function nudgeKey(sign) {
        const now = Date.now()
        keyRepeats = now - lastKeyTime < 400 && direction === sign ? keyRepeats + 1 : 0
        lastKeyTime = now; direction = sign
        nudge(sign * Math.min(60, 5 * (1 + Math.floor(keyRepeats / 6))))
    }
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { if (seeking) commit(); else begin() }
        else if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
            nudgeKey(event.key === Qt.Key_Left ? -1 : 1)
        } else if (event.key === Qt.Key_Escape) { if (seeking) { cancel(); activity() } else backRequested() }
        else if (event.key === Qt.Key_Up) { cancel(); moveUp() }
        else if (event.key === Qt.Key_Down) { cancel(); moveDown() }
        else return
        event.accepted = true
    }
    Timer {
        interval: 50; repeat: true; running: seek.activeFocus && seek.visible && seek.enabled && seek.axis !== 0
        onRunningChanged: { seek.lastTick = Date.now(); if (!running) { seek.heldSeconds = 0; seek.direction = 0 } }
        onTriggered: {
            const now = Date.now(), dt = Math.min(0.1, (now - seek.lastTick) / 1000), sign = seek.axis < 0 ? -1 : 1
            seek.lastTick = now
            if (seek.direction !== sign) seek.heldSeconds = 0
            seek.direction = sign; seek.heldSeconds += dt
            seek.nudge(sign * seek.speed * dt)
        }
    }
    Item {
        id: track
        objectName: "timelineTrack"
        width: parent.width; height: seek.activeFocus ? 8 : 5; y: 18 - height / 2
        Rectangle { anchors.fill: parent; radius: height / 2; color: "#454b53" }
        Rectangle { objectName: "bufferProgress"; height: parent.height; radius: height / 2; width: parent.width * Math.min(1, Player.duration > 0 ? Player.bufferedPosition / Player.duration : 0); color: "#9ba8b4" }
        Rectangle { height: parent.height; radius: height / 2; width: parent.width * seek.fraction; color: seek.accent }
        Repeater {
            model: seek.segments
            delegate: Rectangle {
                required property var modelData
                required property int index
                objectName: "timelineSegment-" + index
                x: track.width * modelData.start / Player.duration
                width: track.width * (Math.min(Player.duration, modelData.end) - modelData.start) / Player.duration
                height: track.height; radius: 2
                color: seek.segmentColor(modelData.type)
                Accessible.role: Accessible.StaticText
                Accessible.name: seek.segmentName(modelData.type) + ": " + seek.time(modelData.start) + " to " + seek.time(Math.min(Player.duration, modelData.end))
            }
        }
        Repeater {
            model: seek.chapters
            delegate: Rectangle {
                required property var modelData
                required property int index
                objectName: "timelineChapter-" + index
                x: Math.max(0, Math.min(track.width - width, track.width * modelData.ticks / 10000000 / Player.duration - width / 2))
                anchors.verticalCenter: parent.verticalCenter
                width: 3; height: track.height + 6; radius: 1
                color: "#edf5fb"; border.color: "#15202d"; border.width: 0.5
                Accessible.role: Accessible.StaticText
                Accessible.name: (modelData.name || "Chapter " + (index + 1)) + ": " + seek.time(modelData.ticks / 10000000)
            }
        }
    }
    Flow {
        id: markerLegend
        objectName: "timelineLegend"
        y: 36; width: parent.width; spacing: 12
        visible: seek.chapters.length > 0 || seek.segments.length > 0
        Row {
            visible: seek.chapters.length > 0; spacing: 6
            Rectangle { width: 3; height: 10; color: "#edf5fb"; anchors.verticalCenter: parent.verticalCenter }
            Text { text: "Chapters"; color: "#c3d7e4"; font.pixelSize: 12 }
        }
        Repeater {
            model: seek.segmentTypes
            delegate: Row {
                required property string modelData
                spacing: 6
                Rectangle { width: 12; height: 5; radius: 2; color: seek.segmentColor(modelData); anchors.verticalCenter: parent.verticalCenter }
                Text { text: seek.segmentName(modelData); color: "#c3d7e4"; font.pixelSize: 12 }
            }
        }
    }
    Rectangle {
        x: Math.max(0, Math.min(parent.width - width, parent.width * seek.fraction - width / 2)); y: 18 - height / 2
        width: seek.activeFocus ? 22 : 12; height: width; radius: width / 2
        color: seek.accent; border.color: "white"; border.width: seek.activeFocus ? 3 : 0
    }
    Rectangle {
        id: preview
        objectName: "trickplayPreview"
        visible: seek.seeking
        width: 278; height: (seek.frame.url ? imageClip.height + 8 : 0) + label.implicitHeight + 16
        x: Math.max(0, Math.min(seek.width - width, seek.width * seek.fraction - width / 2))
        anchors.bottom: parent.top; anchors.bottomMargin: 12
        radius: 6; color: "#15202d"; border.color: "#60798d"
        Item {
            id: imageClip
            x: 4; y: 4; width: 270; height: seek.frame.url ? width * seek.frame.height / seek.frame.width : 0
            clip: true; visible: !!seek.frame.url
            SkeletonBlock { anchors.fill: parent; visible: tile.status === Image.Loading }
            Image {
                id: tile
                objectName: "trickplayImage"
                source: seek.frame.url || ""
                asynchronous: true; cache: true
                width: imageClip.width * (seek.frame.columns || 1); height: imageClip.height * (seek.frame.rows || 1)
                x: -imageClip.width * (seek.frame.column || 0); y: -imageClip.height * (seek.frame.row || 0)
                visible: status === Image.Ready
            }
            Text { anchors.centerIn: parent; visible: tile.status === Image.Error; text: "Preview unavailable"; color: "#c3d7e4"; font.pixelSize: 14 }
        }
        Text {
            id: label
            objectName: "seekPreviewLabel"
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 8 }
            horizontalAlignment: Text.AlignHCenter
            text: seek.time(seek.previewPosition) + (seek.speed > 0 ? "  ·  " + Math.round(seek.speed) + "s / sec" : "")
                + (seek.contextLabel ? "\n" + seek.contextLabel : "") + "\nSelect to play · Back to cancel"
            maximumLineCount: 3; elide: Text.ElideRight
            color: "white"; font.pixelSize: 16
        }
    }
    MouseArea {
        anchors.fill: parent
        function updatePosition(x) { seek.previewPosition = Math.max(0, Math.min(Player.duration - 1, x / width * Player.duration)); seek.activity() }
        onPressed: mouse => { seek.forceActiveFocus(); seek.begin(); updatePosition(mouse.x) }
        onPositionChanged: mouse => { if (pressed) updatePosition(mouse.x) }
        onReleased: seek.commit()
        onCanceled: seek.cancel()
    }
    Text {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.top; bottomMargin: 8 }
        visible: seek.activeFocus && !seek.seeking
        text: "Select to seek · Stick / ← → to preview · Select to play · Back to cancel"
        color: "#c3d7e4"; font.pixelSize: 13
    }
}
