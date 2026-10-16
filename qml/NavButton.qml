import QtQuick
import QtQuick.Controls

Button {
    id: control
    property color accent: "#66c0f4"
    property bool primary: false
    property Item upTarget
    property Item downTarget
    property Item leftTarget
    property Item rightTarget
    implicitHeight: 48
    implicitWidth: Math.max(120, label.implicitWidth + 36)
    padding: 16
    activeFocusOnTab: true
    hoverEnabled: true
    font.pixelSize: 16
    contentItem: Text {
        id: label
        text: control.text
        font: control.font
        color: control.primary ? "#0c1721" : "#edf5fb"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 5
        color: control.primary ? control.accent : control.down ? "#354b5d" : control.hovered ? "#2b4053" : "#203244"
        border.color: control.activeFocus ? "#ffffff" : "transparent"
        border.width: 3
        Behavior on color { ColorAnimation { duration: 110 } }
    }
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    Keys.onPressed: event => {
        let target = null
        if (event.key === Qt.Key_Up) target = upTarget || nextItemInFocusChain(false)
        else if (event.key === Qt.Key_Down) target = downTarget || nextItemInFocusChain(true)
        else if (event.key === Qt.Key_Left) target = leftTarget || nextItemInFocusChain(false)
        else if (event.key === Qt.Key_Right) target = rightTarget || nextItemInFocusChain(true)
        else return
        if (target) target.forceActiveFocus()
        event.accepted = true
    }
}
