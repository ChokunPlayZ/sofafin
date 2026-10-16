import QtQuick

Rectangle {
    radius: 5
    color: "#294153"
    opacity: 0.65
    SequentialAnimation on opacity {
        running: visible; loops: Animation.Infinite
        NumberAnimation { from: 0.45; to: 0.85; duration: 800; easing.type: Easing.InOutSine }
        NumberAnimation { from: 0.85; to: 0.45; duration: 800; easing.type: Easing.InOutSine }
    }
}
