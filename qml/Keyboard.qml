import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: keyboard
    property var target
    property int mode: 0
    property color accent: "#66c0f4"
    readonly property var characters: (mode === 0 ? "abcdefghijklmnopqrstuvwxyz0123456789.-/:" : mode === 1 ? "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_@!?" : "0123456789!@#$%^&*()-_=+[]{};:'\",.<>/?\\|~").split("")
    width: Math.min(760, parent.width - 40)
    height: Math.min(parent.height - 40, 185 + Math.ceil(characters.length / 10) * 50)
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    modal: true; focus: true; padding: 24
    objectName: "keyboard"
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: "#152637"; radius: 10; border.color: "#3d586d" }
    function edit(field) { target = field; open(); Qt.callLater(() => keys.itemAt(0).forceActiveFocus()) }
    onClosed: if (target) target.forceActiveFocus()
    contentItem: ColumnLayout {
        spacing: 14
        Text { text: "ON-SCREEN KEYBOARD"; color: "#89a4b9"; font.pixelSize: 12; font.letterSpacing: 2 }
        Text { Layout.fillWidth: true; text: keyboard.target ? (keyboard.target.echoMode === TextInput.Password ? "•".repeat(keyboard.target.text.length) : keyboard.target.text) || "Start typing…" : ""; color: "white"; font.pixelSize: 20; elide: Text.ElideLeft }
        GridLayout {
            columns: 10; columnSpacing: 7; rowSpacing: 7
            Layout.fillWidth: true
            Repeater {
                id: keys
                model: keyboard.characters
                NavButton {
                    required property string modelData
                    required property int index
                    objectName: "keyboard-key-" + index
                    Layout.fillWidth: true; implicitWidth: 40; implicitHeight: 43; padding: 4
                    text: modelData; accent: keyboard.accent
                    leftTarget: keys.itemAt(Math.max(0, index - 1))
                    rightTarget: keys.itemAt(Math.min(keys.count - 1, index + 1))
                    upTarget: keys.itemAt(Math.max(0, index - 10))
                    downTarget: index + 10 < keys.count ? keys.itemAt(index + 10) : space
                    onClicked: { if (keyboard.target) keyboard.target.insert(keyboard.target.cursorPosition, modelData) }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            NavButton { id: space; text: "Space"; Layout.fillWidth: true; upTarget: keys.itemAt(keys.count - 1); onClicked: if (keyboard.target) keyboard.target.insert(keyboard.target.cursorPosition, " ") }
            NavButton { text: "⌫ Delete"; Layout.fillWidth: true; onClicked: { if (keyboard.target && keyboard.target.cursorPosition > 0) keyboard.target.remove(keyboard.target.cursorPosition - 1, keyboard.target.cursorPosition) } }
            NavButton { text: keyboard.mode === 0 ? "ABC" : keyboard.mode === 1 ? "#+=" : "abc"; onClicked: { keyboard.mode = (keyboard.mode + 1) % 3; Qt.callLater(() => keys.itemAt(0).forceActiveFocus()) } }
            NavButton { text: "Done"; primary: true; accent: keyboard.accent; onClicked: keyboard.close() }
        }
    }
}
