import QtQuick

FocusScope {
    id: grid
    property string title
    property var items: []
    property color accent: "#66c0f4"
    property real preferredCardWidth: 156
    property bool loadArtwork: true
    property real viewportTop: 0
    property real viewportHeight: height
    property var savedIndices: ({})
    readonly property int columns: Math.max(1, Math.floor((width + 18) / (preferredCardWidth + 18)))
    readonly property real cardWidth: Math.max(1, (width - (columns - 1) * 18) / columns)
    readonly property real cardHeight: Math.min(cardWidth * 226 / 156, Math.max(100, viewportHeight - 48))
    readonly property int itemIndex: view.currentIndex
    signal activated(var media)
    signal selected(var media, int selectedIndex)
    signal moveUp()
    signal moveDown()
    signal revealRequested(real top, real itemHeight)
    implicitHeight: items.length ? 39 + Math.ceil(items.length / columns) * view.cellHeight - 18 : 0
    visible: items.length > 0
    onItemsChanged: Qt.callLater(restoreSelection)
    onTitleChanged: Qt.callLater(restoreSelection)
    onColumnsChanged: Qt.callLater(revealCurrent)
    onViewportHeightChanged: Qt.callLater(revealCurrent)
    function focusFirst() { view.forceActiveFocus(); announce(); revealCurrent() }
    function announce() { if (view.currentIndex >= 0 && view.currentIndex < items.length) selected(items[view.currentIndex], view.currentIndex) }
    function restoreSelection() {
        view.currentIndex = Math.min(savedIndices[title] || 0, Math.max(0, items.length - 1))
        if (view.activeFocus) { announce(); revealCurrent() }
    }
    function select(index) {
        view.currentIndex = Math.max(0, Math.min(items.length - 1, index))
        savedIndices[title] = view.currentIndex
        announce(); revealCurrent()
    }
    function revealCurrent() {
        if (view.activeFocus && view.currentIndex >= 0)
            revealRequested(39 + Math.floor(view.currentIndex / columns) * view.cellHeight, cardHeight + 48)
    }
    Text { text: grid.title; color: "#e5eef6"; font.pixelSize: 22; font.weight: Font.DemiBold }
    GridView {
        id: view
        objectName: "libraryGridView"
        x: 0; y: 39; width: grid.width + 18; height: Math.max(0, grid.height - y)
        cellWidth: grid.cardWidth + 18; cellHeight: grid.cardHeight + 66
        model: grid.items; interactive: false; focus: true; keyNavigationEnabled: false
        onActiveFocusChanged: if (activeFocus) { grid.announce(); grid.revealCurrent() }
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Right) grid.select(currentIndex + 1)
            else if (event.key === Qt.Key_Left) grid.select(currentIndex - 1)
            else if (event.key === Qt.Key_Up) {
                if (currentIndex < grid.columns) grid.moveUp()
                else grid.select(currentIndex - grid.columns)
            } else if (event.key === Qt.Key_Down) {
                if (Math.floor(currentIndex / grid.columns) >= Math.ceil(count / grid.columns) - 1) grid.moveDown()
                else grid.select(currentIndex + grid.columns)
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) grid.activated(grid.items[currentIndex])
            else return
            event.accepted = true
        }
        delegate: MediaCard {
            required property var modelData
            required property int index
            objectName: "gridCard-" + index
            width: grid.cardWidth; height: grid.cardHeight + 48; posterHeight: grid.cardHeight
            media: modelData; accent: grid.accent
            selected: view.activeFocus && view.currentIndex === index
            loadArtwork: grid.loadArtwork && Math.floor(index / grid.columns) * view.cellHeight + height + view.y >= grid.viewportTop && Math.floor(index / grid.columns) * view.cellHeight + view.y <= grid.viewportTop + grid.viewportHeight
            onActivated: { grid.select(index); view.forceActiveFocus(); grid.activated(modelData) }
        }
    }
}
