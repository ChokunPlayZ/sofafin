import QtQuick

Item {
    id: card
    property var media: ({})
    property color accent: "#66c0f4"
    property real posterHeight: 226
    property bool selected: false
    property bool loadArtwork: true
    signal activated()
    implicitHeight: posterHeight + 48
    Rectangle {
        width: parent.width; height: card.posterHeight; radius: 5; color: "#22394d"
        gradient: Gradient { GradientStop { position: 0; color: "#304e65" } GradientStop { position: 1; color: "#142435" } }
        Text { anchors.centerIn: parent; text: (card.media.title || "?").charAt(0); color: "#53718b"; font.pixelSize: 76; font.weight: Font.Light }
        Artwork { anchors.fill: parent; url: card.media.poster || ""; blurHash: card.media.posterBlurHash || ""; load: card.loadArtwork }
        Rectangle {
            visible: card.media.played || card.media.favorite
            anchors { left: parent.left; top: parent.top; margins: 8 }
            width: statusLabel.implicitWidth + 16; height: 25; radius: 3; color: "#db101c29"
            Text { id: statusLabel; anchors.centerIn: parent; text: (card.media.played ? "✓ " : "") + (card.media.favorite ? "♥" : ""); color: card.accent; font.pixelSize: 14 }
        }
        Rectangle { anchors.fill: parent; color: "transparent"; radius: 5; border.width: card.selected ? 4 : 0; border.color: card.accent }
        Rectangle {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right; margins: 6 }
            height: 3; color: "#547083"; visible: card.media.progress > 0
            Rectangle { height: parent.height; width: parent.width * Math.min(1, card.media.progress || 0); color: card.accent }
        }
    }
    Text { y: card.posterHeight + 10; width: parent.width; text: card.media.title || ""; color: card.selected ? "white" : "#c5d5e2"; elide: Text.ElideRight; font.pixelSize: 15; font.weight: Font.DemiBold }
    Text { y: card.posterHeight + 31; width: parent.width; text: card.media.subtitle || ""; color: "#7892a6"; elide: Text.ElideRight; font.pixelSize: 11 }
    MouseArea { anchors.fill: parent; onClicked: card.activated() }
}
