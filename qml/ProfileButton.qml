import QtQuick
import QtQuick.Controls

NavButton {
    id: control
    implicitWidth: 44; implicitHeight: 44
    padding: 4; flat: true
    text: App.username + " · Settings"
    contentItem: Rectangle {
        color: "#2a4255"; radius: 4; clip: true
        Text {
            objectName: "profileInitial"
            anchors.centerIn: parent
            text: App.username.trim().charAt(0).toUpperCase()
            color: "#edf5fb"; font.pixelSize: 20; font.weight: Font.DemiBold
            visible: avatar.status !== Image.Ready
        }
        Image {
            id: avatar
            objectName: "profileImage"
            anchors.fill: parent
            source: control.visible ? App.profileImage : ""
            sourceSize: Qt.size(96, 96)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            visible: status === Image.Ready
        }
    }
    ToolTip.visible: hovered || activeFocus
    ToolTip.delay: 700
    ToolTip.text: text
}
