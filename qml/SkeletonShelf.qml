import QtQuick

Column {
    id: skeleton
    property bool grid: false
    property real cardWidth: 156
    property real cardHeight: 226
    spacing: 18
    Accessible.role: Accessible.ProgressBar
    Accessible.name: "Loading titles"
    Repeater {
        model: skeleton.grid ? 2 : 1
        Column {
            width: skeleton.width; spacing: 18
            SkeletonBlock { width: 180; height: 22 }
            Row {
                spacing: 18
                Repeater {
                    model: Math.max(1, Math.floor((skeleton.width + 18) / (skeleton.cardWidth + 18)))
                    Column {
                        spacing: 8
                        SkeletonBlock { width: skeleton.cardWidth; height: skeleton.cardHeight }
                        SkeletonBlock { width: skeleton.cardWidth * 0.85; height: 14 }
                        SkeletonBlock { width: skeleton.cardWidth * 0.55; height: 12 }
                    }
                }
            }
        }
    }
}
