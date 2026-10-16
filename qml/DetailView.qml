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
    property var selectedEpisodes: ({})
    property string activeSeasonId: ""
    property string pendingSeasonId: ""
    property Item scrollFocusItem
    signal closed()
    signal settingsRequested()
    visible: opened && App.signedIn && !Player.active && !App.playbackPending
    onMediaIdChanged: {
        playbackExpanded = false; extrasExpanded = false
        detailScroll.contentY = 0; needsFocus = true; scrollFocusItem = null
    }
    onVisibleChanged: if (visible) focusAction()
    function open() { history = []; selectedEpisodes = ({}); pendingSeasonId = ""; activeSeasonId = ""; opened = true; detailScroll.contentY = 0; needsFocus = true; focusAction() }
    function openAtSeason(seasonId) { open(); pendingSeasonId = seasonId }
    function close() { if (opened) { opened = false; history = []; closed() } }
    function show(item, seasonId) {
        history = history.concat([{media: Object.assign({}, App.detail), seasonId: activeSeasonId}])
        pendingSeasonId = seasonId || ""; activeSeasonId = ""
        App.showDetails(item)
    }
    function back() {
        if (!history.length) { close(); return }
        const previous = history[history.length - 1]
        history = history.slice(0, -1)
        pendingSeasonId = previous.seasonId; activeSeasonId = ""
        App.showDetails(previous.media)
    }
    function focusAction() {
        if (visible && needsFocus && !App.detailLoading && !media.seasonsLoading) {
            needsFocus = false
            Qt.callLater(() => {
                const seasonId = pendingSeasonId; pendingSeasonId = ""
                for (let i = 0; i < seasons.count; ++i) {
                    const section = seasons.itemAt(i)
                    if (section.modelData.id === seasonId && section.episodes.visible) { section.episodes.focusFirst(); return }
                }
                primaryAction.forceActiveFocus()
            })
        }
    }
    function focusSeason(index, direction) {
        for (let i = index; i >= 0 && i < seasons.count; i += direction) {
            const shelf = seasons.itemAt(i).episodes
            if (shelf.visible) { shelf.focusFirst(); return }
        }
        if (direction < 0) primaryAction.forceActiveFocus()
    }
    function restoreFocus() { if (visible && !activeFocus && !App.detailLoading) primaryAction.forceActiveFocus() }
    Connections { target: App; function onDetailsChanged() { page.focusAction() } }
    Keys.onEscapePressed: back()
    function reveal(item) {
        scrollFocusItem = item
        const point = item.mapToItem(detailColumn, 0, 0)
        if (point.y < detailScroll.contentY) detailScroll.contentY = point.y
        else if (point.y + item.height > detailScroll.contentY + detailScroll.height)
            detailScroll.contentY = point.y + item.height - detailScroll.height
    }
    function revealFocusedItem() {
        if (scrollFocusItem && scrollFocusItem.activeFocus && scrollFocusItem.visible) reveal(scrollFocusItem)
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
        width: parent.width - 2 * x
        height: parent.height - y - 32
        clip: true; contentWidth: width; contentHeight: detailColumn.implicitHeight
        onWidthChanged: Qt.callLater(page.revealFocusedItem)
        onHeightChanged: Qt.callLater(page.revealFocusedItem)
        onContentHeightChanged: Qt.callLater(page.revealFocusedItem)
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        ColumnLayout {
            id: detailColumn
            width: detailScroll.width - 14; spacing: 16
            Text { objectName: "detailTitle"; Layout.fillWidth: true; Layout.maximumWidth: 726; Layout.minimumWidth: 0; text: page.media.title || ""; wrapMode: Text.Wrap; color: "white"; font.pixelSize: page.width < 1000 ? 34 : 46; font.weight: Font.Bold }
            Text { Layout.fillWidth: true; Layout.maximumWidth: 726; text: page.media.subtitle || ""; visible: text.length > 0; color: page.accent; font.pixelSize: 14; wrapMode: Text.Wrap }
            Text { Layout.fillWidth: true; Layout.maximumWidth: 726; text: [page.media.rating || "", page.media.communityRating ? "★ " + page.media.communityRating.toFixed(1) : "", (page.media.genres || []).join(" · ")].filter(Boolean).join("   "); color: "#c3d4df"; font.pixelSize: 13; wrapMode: Text.Wrap; visible: text.length > 0 }
            Text { Layout.fillWidth: true; Layout.maximumWidth: 726; text: page.media.tagline || ""; visible: text.length > 0; color: "#c3d4df"; font.pixelSize: 15; font.italic: true; wrapMode: Text.Wrap }
            Text { Layout.fillWidth: true; Layout.maximumWidth: 726; text: page.media.overview || ""; visible: text.length > 0; wrapMode: Text.Wrap; color: "#afc5d5"; font.pixelSize: 16; lineHeight: 1.25 }
            Column {
                Layout.fillWidth: true; Layout.maximumWidth: 726; spacing: 12; visible: App.detailLoading; objectName: "detailLoadingSkeleton"
                SkeletonBlock { width: parent.width; height: 16 }
                SkeletonBlock { width: parent.width * 0.9; height: 16 }
                SkeletonBlock { width: parent.width * 0.65; height: 16 }
                SkeletonBlock { width: 220; height: 52 }
            }
            NavButton {
                id: primaryAction
                visible: !App.detailLoading
                objectName: "playButton"
                Layout.preferredWidth: 220; Layout.minimumWidth: 0; implicitHeight: 52
                text: page.media.folder ? (page.media.type === "Series" ? "Episodes" : "Browse") : App.busy ? "Preparing…" : page.media.progress > 0 ? "Resume" : "Play"
                primary: true; accent: page.accent
                readonly property real resumeProgress: page.media.playable ? Math.max(0, Math.min(1, page.media.progress || 0)) : 0
                Layout.bottomMargin: resumeProgress > 0 ? 8 : 0
                bottomPadding: resumeProgress > 0 ? 20 : 16
                Rectangle {
                    id: resumeTrack
                    objectName: "resumeProgressTrack"
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 12; rightMargin: 12; bottomMargin: -4 }
                    height: 7; radius: 3.5; color: "#405569"
                    visible: primaryAction.resumeProgress > 0
                    Rectangle { objectName: "resumeProgressFill"; height: parent.height; width: parent.width * primaryAction.resumeProgress; radius: parent.radius; color: "#eff7fc" }
                }
                enabled: !App.busy && !App.detailLoading && (!!page.media.folder || !!page.media.playable)
                onActiveFocusChanged: if (activeFocus) page.reveal(this)
                onClicked: {
                    if (page.media.type === "Series") { page.focusSeason(0, 1); return }
                    const item = page.media; const selection = playbackOptions.selection
                    page.close(); if (item.folder) App.open(item); else App.play(item, true, selection)
                }
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
                onClicked: page.show({id: page.media.seriesId, title: page.media.seriesName, type: "Series", folder: true}, page.media.seasonId)
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
            SkeletonShelf { Layout.fillWidth: true; visible: !!page.media.seasonsLoading && !(page.media.seasons || []).length; cardWidth: 263; cardHeight: 148 }
            Text {
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#afc5d5"; font.pixelSize: 16
                visible: page.media.type === "Series" && !App.detailLoading && !page.media.seasonsLoading && !(page.media.seasons || []).length
                text: page.media.seasonsError ? "Could not load seasons. " + page.media.seasonsError : "No episodes available"
            }
            Repeater {
                id: seasons
                model: page.media.seasons || []
                ColumnLayout {
                    required property var modelData
                    required property int index
                    property alias episodes: episodeShelf
                    objectName: "seasonSection-" + index
                    Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: 8
                    MediaShelf {
                        id: episodeShelf
                        objectName: "seasonShelf-" + index
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        title: modelData.title; items: modelData.items; loading: !!modelData.loading; accent: page.accent
                        cardWidth: 156; cardHeight: 148
                        initialIndex: page.selectedEpisodes[modelData.id] || 0
                        activeFocusOnTab: true
                        loadArtwork: page.visible && y + parent.y + height >= detailScroll.contentY && y + parent.y <= detailScroll.contentY + detailScroll.height
                        onSelected: (media, selectedIndex) => { if (activeFocus) { page.activeSeasonId = modelData.id; page.selectedEpisodes[modelData.id] = selectedIndex } }
                        onActiveFocusChanged: if (activeFocus) { page.activeSeasonId = modelData.id; page.reveal(this) }
                        onActivated: media => page.show(media)
                        onMoveUp: page.focusSeason(index - 1, -1)
                        onMoveDown: page.focusSeason(index + 1, 1)
                    }
                    Text { Layout.fillWidth: true; text: modelData.title; visible: !episodeShelf.visible; color: "#e5eef6"; font.pixelSize: 22; font.weight: Font.DemiBold }
                    Text { Layout.fillWidth: true; wrapMode: Text.Wrap; visible: !episodeShelf.visible; text: modelData.error ? "Could not load episodes. " + modelData.error : "No episodes available"; color: "#afc5d5"; font.pixelSize: 14 }
                }
            }
        }
    }
}
