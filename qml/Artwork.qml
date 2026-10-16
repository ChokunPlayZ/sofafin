import QtQuick

Item {
    id: artwork
    property string url: ""
    property string blurHash: ""
    property bool load: true
    property int fillMode: Image.PreserveAspectCrop
    readonly property int status: fullImage.status
    clip: true
    Rectangle { anchors.fill: parent; color: "#22394d" }
    Image {
        anchors.fill: parent
        source: artwork.load && artwork.blurHash ? "image://blurhash/" + artwork.blurHash.split("").map(c => c.charCodeAt(0).toString(16).padStart(2, "0")).join("") : ""
        sourceSize: Qt.size(32, 24)
        fillMode: artwork.fillMode
        asynchronous: true
        visible: fullImage.status !== Image.Ready
    }
    Image {
        id: fullImage
        anchors.fill: parent
        source: artwork.load ? artwork.url : ""
        sourceSize: Qt.size(Math.max(1, Math.ceil(artwork.width * Screen.devicePixelRatio)), Math.max(1, Math.ceil(artwork.height * Screen.devicePixelRatio)))
        asynchronous: true
        cache: true
        fillMode: artwork.fillMode
        visible: status === Image.Ready
    }
}
