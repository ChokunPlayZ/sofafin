import QtQuick
import QtQuick.Controls

FocusScope {
    id: shelf
    property string title
    property var items: []
    property color accent: "#66c0f4"
    property real cardWidth: 156
    property real cardHeight: 226
    property int initialIndex: 0
    property bool loadArtwork: true
    readonly property int itemIndex: list.currentIndex
    signal activated(var media)
    signal selected(var media, int selectedIndex)
    signal moveUp()
    signal moveDown()
    implicitHeight: cardHeight + 89
    visible: items.length > 0
    function focusFirst() { list.forceActiveFocus(); announce() }
    function announce() { if (items.length && list.currentIndex >= 0) selected(items[list.currentIndex], list.currentIndex) }
    Text { text: shelf.title; color: "#e5eef6"; font.pixelSize: 22; font.weight: Font.DemiBold }
    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 39; bottom: parent.bottom }
        orientation: ListView.Horizontal
        spacing: 18
        clip: true
        cacheBuffer: 0
        model: shelf.items
        currentIndex: Math.min(shelf.initialIndex, Math.max(0, count - 1))
        focus: true
        keyNavigationEnabled: false
        highlightMoveDuration: 140
        preferredHighlightBegin: 0
        preferredHighlightEnd: width - shelf.cardWidth
        highlightRangeMode: ListView.ApplyRange
        onCurrentIndexChanged: shelf.announce()
        onActiveFocusChanged: if (activeFocus) shelf.announce()
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Right) currentIndex = Math.min(count - 1, currentIndex + 1)
            else if (event.key === Qt.Key_Left) currentIndex = Math.max(0, currentIndex - 1)
            else if (event.key === Qt.Key_Up) shelf.moveUp()
            else if (event.key === Qt.Key_Down) shelf.moveDown()
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) shelf.activated(shelf.items[currentIndex])
            else return
            event.accepted = true
        }
        delegate: MediaCard {
            id: card
            required property var modelData
            required property int index
            width: modelData.imageAspect > 1 ? shelf.cardHeight * 16 / 9 : shelf.cardWidth
            height: shelf.cardHeight + 48
            media: modelData; posterHeight: shelf.cardHeight; accent: shelf.accent
            selected: list.activeFocus && list.currentIndex === index
            loadArtwork: shelf.loadArtwork && card.x + card.width >= list.contentX && card.x <= list.contentX + list.width
            onActivated: { list.currentIndex = card.index; list.forceActiveFocus(); shelf.activated(card.modelData) }
        }
    }
}
