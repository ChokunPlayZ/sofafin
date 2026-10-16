import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: page
    objectName: "detailView"
    property color accent: "#66c0f4"
    readonly property var media: App.detail
    readonly property string mediaId: media.id || ""
    property bool opened: false
    property bool playbackExpanded: false
    property bool extrasExpanded: false
    property bool needsFocus: false
    property var history: []
    signal closed()
    signal settingsRequested()
    visible: opened && App.signedIn && !Player.active && !App.playbackPending
    onMediaIdChanged: {
        playbackExpanded = false; extrasExpanded = false
        detailScroll.contentY = 0; needsFocus = true
    }
    onVisibleChanged: if (visible) focusAction()
    function open() { history = []; opened = true; detailScroll.contentY = 0; needsFocus = true; focusAction() }
    function close() { if (opened) { opened = false; history = []; closed() } }
    function show(item) { history = history.concat([Object.assign({}, App.detail)]); App.showDetails(item) }
    function back() {
        if (!history.length) { close(); return }
        const previous = history[history.length - 1]
        history = history.slice(0, -1)
        App.showDetails(previous)
    }
    function focusAction() {
        if (visible && needsFocus && !App.detailLoading) {
            needsFocus = false
            Qt.callLater(() => primaryAction.forceActiveFocus())
        }
    }
    function restoreFocus() { if (visible && !activeFocus && !App.detailLoading) primaryAction.forceActiveFocus() }
    Connections { target: App; function onDetailsChanged() { page.focusAction() } }
    Keys.onEscapePressed: back()
    function reveal(item) {
        const point = item.mapToItem(detailColumn, 0, 0)
        if (point.y < detailScroll.contentY) detailScroll.contentY = point.y
        else if (point.y + item.height > detailScroll.contentY + detailScroll.height)
            detailScroll.contentY = point.y + item.height - detailScroll.height
    }
    Rectangle { anchors.fill: parent; color: "#101923" }
    Artwork {
        anchors { top: parent.top; right: parent.right; bottom: parent.bottom }
        width: parent.width * 0.85
        url: page.media.backdrop || page.media.poster || ""
        blurHash: page.media.backdropBlurHash || page.media.posterBlurHash || ""
        load: page.visible; opacity: 0.8
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "#fa101923" }
            GradientStop { position: 0.55; color: "#dc101923" }
            GradientStop { position: 1; color: "#30101923" }
        }
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: "#30101923" }
            GradientStop { position: 0.7; color: "#00101923" }
            GradientStop { position: 1; color: "#f5101923" }
        }
    }
    RowLayout {
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 28 }
        spacing: 16
        IconButton { id: backButton; objectName: "detailBackButton"; text: "Back"; iconName: "back"; downTarget: primaryAction; onClicked: page.back() }
        Image { source: "qrc:/assets/logo.svg"; Layout.preferredWidth: 36; Layout.preferredHeight: 36 }
        Item { Layout.fillWidth: true }
        ProfileButton { onClicked: page.settingsRequested() }
    }
    Flickable {
        id: detailScroll
        objectName: "detailScroll"
        x: page.width < 1000 ? 32 : 64; y: 110
        width: Math.min(740, parent.width - 2 * x)
        height: parent.height - y - 32
        clip: true; contentWidth: width; contentHeight: detailColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        ColumnLayout {
            id: detailColumn
            width: detailScroll.width - 14; spacing: 16
            Text { objectName: "detailTitle"; Layout.fillWidth: true; Layout.minimumWidth: 0; text: page.media.title || ""; wrapMode: Text.Wrap; color: "white"; font.pixelSize: page.width < 1000 ? 34 : 46; font.weight: Font.Bold }
            Text { Layout.fillWidth: true; text: page.media.subtitle || ""; visible: text.length > 0; color: page.accent; font.pixelSize: 14; wrapMode: Text.Wrap }
            Text { Layout.fillWidth: true; text: [page.media.rating || "", page.media.communityRating ? "★ " + page.media.communityRating.toFixed(1) : "", (page.media.genres || []).join(" · ")].filter(Boolean).join("   "); color: "#c3d4df"; font.pixelSize: 13; wrapMode: Text.Wrap; visible: text.length > 0 }
            Text { Layout.fillWidth: true; text: page.media.tagline || ""; visible: text.length > 0; color: "#c3d4df"; font.pixelSize: 15; font.italic: true; wrapMode: Text.Wrap }
            Text { Layout.fillWidth: true; text: page.media.overview || ""; visible: text.length > 0; wrapMode: Text.Wrap; color: "#afc5d5"; font.pixelSize: 16; lineHeight: 1.25 }
            BusyIndicator { running: App.detailLoading; visible: running; implicitWidth: 32; implicitHeight: 32 }
            NavButton {
                id: primaryAction
                objectName: "playButton"
                Layout.preferredWidth: 220; Layout.minimumWidth: 0; implicitHeight: 52
                text: page.media.folder ? (page.media.type === "Series" ? "Seasons" : "Browse") : App.busy ? "Preparing…" : page.media.progress > 0 ? "Resume" : "Play"
                primary: true; accent: page.accent
                enabled: !App.busy && !App.detailLoading && (!!page.media.folder || !!page.media.playable)
                onActiveFocusChanged: if (activeFocus) page.reveal(this)
                onClicked: { const item = page.media; const selection = playbackOptions.selection; page.close(); if (item.folder) App.open(item); else App.play(item, true, selection) }
            }
            NavButton {
                objectName: "restartButton"
                Layout.preferredWidth: 220; Layout.minimumWidth: 0
                text: "Start over"; flat: true; visible: page.media.progress > 0 && page.media.playable
                enabled: !App.busy && !App.detailLoading
                onActiveFocusChanged: if (activeFocus) page.reveal(this)
                onClicked: { const item = page.media; const selection = playbackOptions.selection; page.close(); App.play(item, false, selection) }
            }
            RowLayout {
                Layout.fillWidth: false; Layout.preferredWidth: 340; Layout.maximumWidth: detailColumn.width; spacing: 8
                NavButton {
                    objectName: "favoriteButton"
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    text: (page.media.favorite ? "♥" : "♡") + "  Favorite"
                    checked: !!page.media.favorite; accent: page.accent; flat: true
                    Accessible.checkable: true; Accessible.checked: checked
                    enabled: !App.updatingUserData && !App.detailLoading
                    onActiveFocusChanged: if (activeFocus) page.reveal(this)
                    onClicked: App.setFavorite(page.media, !page.media.favorite)
                }
                NavButton {
                    objectName: "watchedButton"
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    text: (page.media.played ? "✓" : "○") + "  Watched"
                    checked: !!page.media.played; accent: page.accent; flat: true
                    Accessible.checkable: true; Accessible.checked: checked
                    enabled: !App.updatingUserData && !App.detailLoading
                    onActiveFocusChanged: if (activeFocus) page.reveal(this)
                    onClicked: App.setPlayed(page.media, !page.media.played)
                }
            }
            NavButton {
                id: playbackToggle
                objectName: "preplaySettingsButton"
                Layout.fillWidth: false; Layout.preferredWidth: 360; Layout.maximumWidth: detailColumn.width; Layout.minimumWidth: 0
                text: "Playback settings"; value: page.playbackExpanded ? "−" : "+"; flat: true
                visible: !!page.media.playable && !App.detailLoading
                onActiveFocusChanged: if (activeFocus) page.reveal(this)
                onClicked: { page.playbackExpanded = !page.playbackExpanded; page.reveal(this) }
            }
            PlaybackOptions {
                id: playbackOptions
                Layout.fillWidth: false; Layout.preferredWidth: 520; Layout.maximumWidth: detailColumn.width; Layout.minimumWidth: 0
                visible: playbackToggle.visible && page.playbackExpanded
                sources: page.media.mediaSources || []
                mediaId: page.media.id || ""
                onFocusMoved: item => page.reveal(item)
            }
            Text { Layout.fillWidth: true; text: playbackOptions.source.mediaInfo || ""; visible: playbackOptions.visible && text.length > 0; color: "#afc5d5"; font.pixelSize: 13; wrapMode: Text.Wrap }
            NavButton {
                objectName: "seriesButton"
                Layout.fillWidth: true; Layout.minimumWidth: 0
                text: "Series"; value: page.media.seriesName || ""; flat: true
                visible: !!page.media.seriesId
                onActiveFocusChanged: if (activeFocus) page.reveal(this)
                onClicked: page.show({id: page.media.seriesId, title: page.media.seriesName, type: "Series", folder: true})
            }
            NavButton {
                objectName: "seasonButton"
                Layout.fillWidth: true; Layout.minimumWidth: 0
                text: "Season"; value: page.media.seasonName || ""; flat: true
                visible: page.media.type === "Episode" && !!page.media.seasonId
                onActiveFocusChanged: if (activeFocus) page.reveal(this)
                onClicked: { const item = page.media; page.close(); App.openSeason(item) }
            }
            NavButton {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                text: "Next episode"; value: page.media.nextEpisode ? page.media.nextEpisode.title : ""; flat: true
                visible: !!page.media.nextEpisode
                onActiveFocusChanged: if (activeFocus) page.reveal(this)
                onClicked: page.show(page.media.nextEpisode)
            }
            NavButton {
                objectName: "detailExtrasButton"
                Layout.fillWidth: true; Layout.minimumWidth: 0
                text: (page.media.people || []).length && (page.media.chapters || []).length ? "Cast & chapters" : (page.media.people || []).length ? "Cast & crew" : "Chapters"
                value: page.extrasExpanded ? "−" : "+"; flat: true
                visible: (page.media.people || []).length > 0 || (page.media.chapters || []).length > 0
                onActiveFocusChanged: if (activeFocus) page.reveal(this)
                onClicked: page.extrasExpanded = !page.extrasExpanded
            }
            Text { Layout.fillWidth: true; text: (page.media.people || []).map(p => p.name + (p.role ? " · " + p.role : "")).join("\n"); visible: page.extrasExpanded && text.length > 0; wrapMode: Text.Wrap; color: "#afc5d5"; font.pixelSize: 14; lineHeight: 1.4 }
            Repeater {
                model: page.media.chapters || []
                NavButton {
                    required property var modelData
                    required property int index
                    objectName: "chapterButton-" + index
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    text: modelData.name || "Chapter"; flat: true; visible: page.extrasExpanded
                    enabled: !App.busy && !App.detailLoading
                    onActiveFocusChanged: if (activeFocus) page.reveal(this)
                    onClicked: { const item = Object.assign({}, page.media); item.positionTicks = modelData.ticks; const selection = playbackOptions.selection; page.close(); App.play(item, true, selection) }
                }
            }
        }
    }
}
