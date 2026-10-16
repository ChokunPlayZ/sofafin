import QtQuick
import QtQuick.Controls

NavButton {
    id: control
    property var model: []
    property string textRole: "label"
    property int currentIndex: 0
    readonly property string currentText: model[currentIndex] ? model[currentIndex][textRole] || "" : ""
    property string displayText: currentText
    readonly property alias popup: chooser
    signal activated(int index)
    text: displayText
    value: "▾"
    onClicked: chooser.open()
    Popup {
        id: chooser
        y: control.height + 4; width: control.width
        height: Math.min(320, choices.contentHeight + 16)
        padding: 8; modal: true; focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: { choices.currentIndex = Math.max(0, control.currentIndex); choices.forceActiveFocus(); choices.revealCurrent(); Qt.callLater(choices.revealCurrent) }
        background: Rectangle { radius: 5; color: "#15202d"; border.color: "#425263" }
        contentItem: ListView {
            id: choices
            clip: true; model: control.model; spacing: 4
            boundsBehavior: Flickable.StopAtBounds
            keyNavigationEnabled: false
            function revealCurrent() {
                positionViewAtIndex(currentIndex, ListView.Contain)
                if (currentItem) currentItem.forceActiveFocus()
            }
            function navigate(event) {
                if (event.key === Qt.Key_Up) currentIndex = Math.max(0, currentIndex - 1)
                else if (event.key === Qt.Key_Down) currentIndex = Math.min(count - 1, currentIndex + 1)
                else if (event.key === Qt.Key_Home) currentIndex = 0
                else if (event.key === Qt.Key_End) currentIndex = count - 1
                else return
                event.accepted = true
                revealCurrent()
                Qt.callLater(revealCurrent)
            }
            Keys.onPressed: event => navigate(event)
            delegate: NavButton {
                required property var modelData
                required property int index
                width: choices.width; implicitHeight: 44
                text: modelData[control.textRole] || ""
                primary: index === control.currentIndex; accent: control.accent
                Keys.onPressed: event => choices.navigate(event)
                onClicked: { chooser.close(); control.activated(index) }
            }
            ScrollBar.vertical: ScrollBar {}
        }
    }
}
