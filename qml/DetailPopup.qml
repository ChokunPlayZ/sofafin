import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: popup
    objectName: "detailPopup"
    property color accent: "#66c0f4"
    readonly property var media: App.detail
    property bool needsFocus: false
    width: Math.min(960, parent.width - 40)
    height: Math.min(720, parent.height - 40)
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    modal: true; focus: true; padding: 24
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: "#152a3c"; radius: 8; border.color: "#36536a" }
    onOpened: { detailScroll.contentY = 0; needsFocus = true; focusAction() }
    function focusAction() {
        if (visible && needsFocus && !App.detailLoading) {
            needsFocus = false
            Qt.callLater(() => primaryAction.forceActiveFocus())
        }
    }
    Connections { target: App; function onDetailsChanged() { popup.focusAction() } }
    function reveal(item) {
        const point = item.mapToItem(detailColumn, 0, 0)
        if (point.y < detailScroll.contentY) detailScroll.contentY = point.y
        else if (point.y + item.height > detailScroll.contentY + detailScroll.height)
            detailScroll.contentY = point.y + item.height - detailScroll.height
    }
    contentItem: ColumnLayout {
        spacing: 16
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 24
            Item {
                Layout.preferredWidth: popup.availableWidth * 0.28
                Layout.fillHeight: true
                Artwork {
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                    height: Math.min(parent.height, width / Math.max(0.3, popup.media.imageAspect || 2 / 3))
                    url: popup.media.poster || ""; blurHash: popup.media.posterBlurHash || ""
                    fillMode: Image.PreserveAspectFit
                    load: popup.visible
                }
            }
            Flickable {
                id: detailScroll
                objectName: "detailScroll"
                Layout.fillWidth: true; Layout.fillHeight: true
                Layout.minimumWidth: 0; Layout.minimumHeight: 0
                clip: true; contentWidth: width; contentHeight: detailColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                ColumnLayout {
                    id: detailColumn
                    width: detailScroll.width - 14; spacing: 12
                    Text { objectName: "detailTitle"; Layout.fillWidth: true; Layout.minimumWidth: 0; text: popup.media.title || ""; wrapMode: Text.Wrap; color: "white"; font.pixelSize: 28; font.weight: Font.Bold }
                    Text { Layout.fillWidth: true; text: popup.media.subtitle || ""; color: popup.accent; font.pixelSize: 14; wrapMode: Text.Wrap }
                    Text { Layout.fillWidth: true; text: [popup.media.rating || "", popup.media.communityRating ? "★ " + popup.media.communityRating.toFixed(1) : "", (popup.media.genres || []).join(" · ")].filter(Boolean).join("   "); color: "#c3d4df"; font.pixelSize: 13; wrapMode: Text.Wrap; visible: text.length > 0 }
                    Text { Layout.fillWidth: true; text: popup.media.tagline || ""; visible: text.length > 0; color: "#c3d4df"; font.pixelSize: 15; font.italic: true; wrapMode: Text.Wrap }
                    Text { Layout.fillWidth: true; text: popup.media.overview || ""; visible: text.length > 0; wrapMode: Text.Wrap; color: "#afc5d5"; font.pixelSize: 16; lineHeight: 1.25 }
                    BusyIndicator { running: App.detailLoading; visible: running; implicitWidth: 32; implicitHeight: 32 }
                    NavButton {
                        id: primaryAction
                        objectName: "playButton"
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        text: popup.media.folder ? (popup.media.type === "Series" ? "Seasons" : "Browse") : App.busy ? "Preparing…" : popup.media.progress > 0 ? "▶  Resume" : "▶  Play"
                        primary: true; accent: popup.accent
                        enabled: !App.busy && !App.detailLoading && (!!popup.media.folder || !!popup.media.playable)
                        onActiveFocusChanged: if (activeFocus) popup.reveal(this)
                        onClicked: { const item = popup.media; const selection = playbackOptions.selection; popup.close(); if (item.folder) App.open(item); else App.play(item, true, selection) }
                    }
                    NavButton {
                        objectName: "restartButton"
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        text: "Play from beginning"; visible: popup.media.progress > 0 && popup.media.playable
                        enabled: !App.busy && !App.detailLoading
                        onActiveFocusChanged: if (activeFocus) popup.reveal(this)
                        onClicked: { const item = popup.media; const selection = playbackOptions.selection; popup.close(); App.play(item, false, selection) }
                    }
                    PlaybackOptions {
                        id: playbackOptions
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        visible: !!popup.media.playable && !App.detailLoading
                        sources: popup.media.mediaSources || []
                        mediaId: popup.media.id || ""
                        onFocusMoved: item => popup.reveal(item)
                    }
                    Text { text: "Media info"; visible: !!playbackOptions.source.mediaInfo && !!popup.media.playable; color: popup.accent; font.pixelSize: 14; topPadding: 8 }
                    Text { Layout.fillWidth: true; text: playbackOptions.source.mediaInfo || ""; visible: text.length > 0 && !!popup.media.playable; color: "#afc5d5"; font.pixelSize: 13; wrapMode: Text.Wrap }
                    NavButton {
                        objectName: "seriesButton"
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        text: "Go to series: " + (popup.media.seriesName || "")
                        visible: !!popup.media.seriesId
                        onActiveFocusChanged: if (activeFocus) popup.reveal(this)
                        onClicked: { const item = popup.media; popup.close(); App.openSeries(item) }
                    }
                    NavButton {
                        objectName: "seasonButton"
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        text: "Go to season: " + (popup.media.seasonName || "")
                        visible: popup.media.type === "Episode" && !!popup.media.seasonId
                        onActiveFocusChanged: if (activeFocus) popup.reveal(this)
                        onClicked: { const item = popup.media; popup.close(); App.openSeason(item) }
                    }
                    NavButton {
                        objectName: "favoriteButton"
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        text: popup.media.favorite ? "♥  Remove from favorites" : "♡  Add to favorites"
                        enabled: !App.updatingUserData && !App.detailLoading
                        onActiveFocusChanged: if (activeFocus) popup.reveal(this)
                        onClicked: App.setFavorite(popup.media, !popup.media.favorite)
                    }
                    NavButton {
                        objectName: "watchedButton"
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        text: popup.media.played ? "✓  Mark as unwatched" : "Mark as watched"
                        enabled: !App.updatingUserData && !App.detailLoading
                        onActiveFocusChanged: if (activeFocus) popup.reveal(this)
                        onClicked: App.setPlayed(popup.media, !popup.media.played)
                    }
                    NavButton {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        text: "Next episode: " + (popup.media.nextEpisode ? popup.media.nextEpisode.title : "")
                        visible: !!popup.media.nextEpisode
                        onActiveFocusChanged: if (activeFocus) popup.reveal(this)
                        onClicked: { const next = popup.media.nextEpisode; App.showDetails(next); detailScroll.contentY = 0; primaryAction.forceActiveFocus() }
                    }
                    Text { text: "CAST & CREW"; visible: (popup.media.people || []).length > 0; color: popup.accent; font.pixelSize: 12; font.letterSpacing: 2; topPadding: 12 }
                    Text { Layout.fillWidth: true; text: (popup.media.people || []).map(p => p.name + (p.role ? " · " + p.role : "")).join("\n"); visible: text.length > 0; wrapMode: Text.Wrap; color: "#afc5d5"; font.pixelSize: 14; lineHeight: 1.4 }
                    Repeater {
                        model: popup.media.chapters || []
                        NavButton {
                            required property var modelData
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            text: "▶  " + (modelData.name || "Chapter")
                            enabled: !App.busy && !App.detailLoading
                            onActiveFocusChanged: if (activeFocus) popup.reveal(this)
                            onClicked: { const item = Object.assign({}, popup.media); item.positionTicks = modelData.ticks; const selection = playbackOptions.selection; popup.close(); App.play(item, true, selection) }
                        }
                    }
                }
            }
        }
        NavButton { objectName: "detailBackButton"; text: "Back"; Layout.fillWidth: true; onClicked: popup.close() }
    }
}
