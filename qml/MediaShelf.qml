import QtQuick
import QtQuick.Controls

FocusScope {
    id: shelf
    property string title
    property var items: []
    property color accent: "#66c0f4"
    property real cardWidth: 156
    property real cardHeight: 226
    signal activated(var media)
    signal selected(var media)
    signal moveUp()
    signal moveDown()
    implicitHeight: cardHeight + 89
    visible: items.length > 0
    function focusFirst() { list.forceActiveFocus(); announce() }
    function announce() { if (items.length && list.currentIndex >= 0) selected(items[list.currentIndex]) }
    Text { text: shelf.title; color: "#e5eef6"; font.pixelSize: 22; font.weight: Font.DemiBold }
    Text { anchors.right: parent.right; text: items.length + " TITLES"; color: "#72899b"; font.pixelSize: 11; font.letterSpacing: 2; y: 8 }
    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 39; bottom: parent.bottom }
        orientation: ListView.Horizontal
        spacing: 18
        clip: true
        model: shelf.items
        currentIndex: 0
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
        delegate: Item {
            id: card
            required property var modelData
            required property int index
            width: shelf.cardWidth
            height: shelf.cardHeight + 48
            readonly property bool selected: list.activeFocus && list.currentIndex === index
            Rectangle {
                id: poster
                width: parent.width; height: shelf.cardHeight; radius: 5; color: "#22394d"
                gradient: Gradient { GradientStop { position: 0; color: "#304e65" } GradientStop { position: 1; color: "#142435" } }
                Text { anchors.centerIn: parent; text: (card.modelData.title || "?").charAt(0); color: "#53718b"; font.pixelSize: 76; font.weight: Font.Light }
                Image { anchors.fill: parent; source: card.modelData.poster || ""; asynchronous: true; fillMode: Image.PreserveAspectCrop }
                Rectangle { anchors.fill: parent; color: "transparent"; radius: 5; border.width: card.selected ? 4 : 0; border.color: shelf.accent }
                Rectangle { anchors { bottom: parent.bottom; left: parent.left; right: parent.right; margins: 6 } height: 3; color: "#547083"; visible: card.modelData.progress > 0
                    Rectangle { height: parent.height; width: parent.width * Math.min(1, card.modelData.progress || 0); color: shelf.accent }
                }
                Rectangle { visible: card.modelData.folder; anchors { right: parent.right; top: parent.top; margins: 9 } width: 54; height: 22; radius: 3; color: "#c0101c27"
                    Text { anchors.centerIn: parent; text: "OPEN"; font.pixelSize: 10; font.bold: true; color: "white" }
                }
            }
            Text { y: shelf.cardHeight + 10; width: parent.width; text: card.modelData.title || ""; color: card.selected ? "white" : "#c5d5e2"; elide: Text.ElideRight; font.pixelSize: 15; font.weight: Font.DemiBold }
            Text { y: shelf.cardHeight + 31; width: parent.width; text: card.modelData.subtitle || ""; color: "#7892a6"; elide: Text.ElideRight; font.pixelSize: 11 }
            MouseArea { anchors.fill: parent; onClicked: { list.currentIndex = card.index; list.forceActiveFocus(); shelf.activated(card.modelData) } }
        }
    }
}
