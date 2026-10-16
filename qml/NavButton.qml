import QtQuick
import QtQuick.Controls

Button {
    id: control
    property color accent: "#66c0f4"
    property bool primary: false
    property string value: ""
    property Item upTarget
    property Item downTarget
    property Item leftTarget
    property Item rightTarget
    signal navigatedUp()
    implicitHeight: 48
    implicitWidth: Math.max(100, label.implicitWidth + valueLabel.implicitWidth + (value.length ? 60 : 36))
    padding: 16
    activeFocusOnTab: true
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45
    ToolTip.visible: hovered && text.length > 45
    ToolTip.text: text
    ToolTip.delay: 600
    Accessible.name: text + (value.length ? ": " + value : "")
    font.pixelSize: 16
    contentItem: Item {
        implicitWidth: label.implicitWidth + (control.value.length ? valueLabel.implicitWidth + 16 : 0)
        implicitHeight: Math.max(label.implicitHeight, valueLabel.implicitHeight)
        Text {
            id: label
            anchors { left: parent.left; right: control.value.length ? valueLabel.left : parent.right; verticalCenter: parent.verticalCenter; rightMargin: control.value.length ? 16 : 0 }
            text: control.text
            font: control.font
            color: control.primary ? "#0c1721" : control.checked ? control.accent : "#edf5fb"
            horizontalAlignment: control.value.length ? Text.AlignLeft : Text.AlignHCenter
            elide: Text.ElideRight
        }
        Text {
            id: valueLabel
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            width: Math.min(implicitWidth, parent.width * 0.6)
            text: control.value
            visible: text.length > 0
            font: control.font
            color: control.accent
            elide: Text.ElideRight
        }
    }
    background: Rectangle {
        radius: 5
        color: control.primary ? control.accent : control.down ? "#354b5d" : control.hovered ? "#2b4053" : control.checked ? "#263f51" : control.flat ? "transparent" : "#203244"
        border.color: control.activeFocus ? "#ffffff" : "transparent"
        border.width: 3
        Behavior on color { ColorAnimation { duration: 110 } }
    }
    Keys.onReturnPressed: { if (checkable) checked = !checked; clicked() }
    Keys.onEnterPressed: { if (checkable) checked = !checked; clicked() }
    Keys.onPressed: event => {
        let target = null
        if (event.key === Qt.Key_Up) { target = upTarget || nextItemInFocusChain(false); navigatedUp() }
        else if (event.key === Qt.Key_Down) target = downTarget || nextItemInFocusChain(true)
        else if (event.key === Qt.Key_Left) target = leftTarget || nextItemInFocusChain(false)
        else if (event.key === Qt.Key_Right) target = rightTarget || nextItemInFocusChain(true)
        else return
        if (target) target.forceActiveFocus()
        event.accepted = true
    }
}
