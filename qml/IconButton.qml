import QtQuick
import QtQuick.Controls

NavButton {
    id: control
    property string iconName
    implicitWidth: 44
    implicitHeight: 44
    padding: 8
    flat: true
    contentItem: Image {
        source: "qrc:/assets/" + control.iconName + ".svg"
        sourceSize: Qt.size(24, 24)
        fillMode: Image.PreserveAspectFit
    }
    ToolTip.visible: hovered || activeFocus
    ToolTip.delay: 700
    ToolTip.text: text
}
