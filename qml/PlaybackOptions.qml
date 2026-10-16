import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    property var sources: []
    property string mediaId: ""
    property int sourceIndex: 0
    property int quality: 0
    property int audio: -2
    property int subtitle: -2
    property bool live: false
    property string namePrefix: "preplay"
    readonly property var source: sources[sourceIndex] || ({})
    readonly property var selection: ({sourceId: source.id || "", quality: quality, audio: audio, subtitle: subtitle})
    readonly property var qualities: [
        {label: "Automatic", index: 0}, {label: "Original", index: -1},
        {label: "80 Mbps", index: 80000000}, {label: "40 Mbps", index: 40000000},
        {label: "20 Mbps", index: 20000000}, {label: "8 Mbps", index: 8000000}, {label: "4 Mbps", index: 4000000}]
    signal focusMoved(var item)
    signal qualitySelected(int value)
    signal audioSelected(int value)
    signal subtitleSelected(int value)
    spacing: 8
    onMediaIdChanged: { sourceIndex = 0; quality = 0; audio = -2; subtitle = -2 }
    function indexFor(model, value) { const index = model.findIndex(track => track.index === value); return Math.max(0, index) }
    function focusQuality() { qualityBox.forceActiveFocus() }
    function focusAudio() { audioBox.forceActiveFocus() }
    function focusSubtitle() { subtitleBox.forceActiveFocus() }
    OptionSelector {
        objectName: panel.namePrefix + "Source"
        Layout.fillWidth: true; implicitHeight: 44
        visible: panel.sources.length > 1
        model: panel.sources; textRole: "name"
        displayText: "Version · " + currentText
        currentIndex: panel.sourceIndex
        onActivated: index => { panel.sourceIndex = index; panel.audio = -2; panel.subtitle = -2 }
        onActiveFocusChanged: if (activeFocus) panel.focusMoved(this)
        Keys.onUpPressed: event => { if (!popup.visible) nextItemInFocusChain(false).forceActiveFocus(); else event.accepted = false }
        Keys.onDownPressed: event => { if (!popup.visible) nextItemInFocusChain(true).forceActiveFocus(); else event.accepted = false }
    }
    OptionSelector {
        id: qualityBox
        objectName: panel.namePrefix + "Quality"
        Layout.fillWidth: true; implicitHeight: 44
        model: panel.qualities; textRole: "label"
        displayText: "Quality · " + currentText
        currentIndex: panel.indexFor(model, panel.quality)
        onActivated: index => { const value = model[index].index; if (!panel.live) panel.quality = value; panel.qualitySelected(value) }
        onActiveFocusChanged: if (activeFocus) panel.focusMoved(this)
        Keys.onUpPressed: event => { if (!popup.visible) nextItemInFocusChain(false).forceActiveFocus(); else event.accepted = false }
        Keys.onDownPressed: event => { if (!popup.visible) nextItemInFocusChain(true).forceActiveFocus(); else event.accepted = false }
    }
    OptionSelector {
        id: audioBox
        objectName: panel.namePrefix + "Audio"
        Layout.fillWidth: true; implicitHeight: 44
        model: [{label: "Automatic", index: -2}].concat(panel.source.audioTracks || [])
        textRole: "label"; displayText: "Audio · " + currentText
        currentIndex: panel.indexFor(model, panel.audio)
        onActivated: index => { const value = model[index].index; if (!panel.live) panel.audio = value; panel.audioSelected(value) }
        onActiveFocusChanged: if (activeFocus) panel.focusMoved(this)
        Keys.onUpPressed: event => { if (!popup.visible) nextItemInFocusChain(false).forceActiveFocus(); else event.accepted = false }
        Keys.onDownPressed: event => { if (!popup.visible) nextItemInFocusChain(true).forceActiveFocus(); else event.accepted = false }
    }
    OptionSelector {
        id: subtitleBox
        objectName: panel.namePrefix + "Subtitle"
        Layout.fillWidth: true; implicitHeight: 44
        model: [{label: "Automatic", index: -2}, {label: "Off", index: -1}].concat(panel.source.subtitleTracks || [])
        textRole: "label"; displayText: "Subtitles · " + currentText
        currentIndex: panel.indexFor(model, panel.subtitle)
        onActivated: index => { const value = model[index].index; if (!panel.live) panel.subtitle = value; panel.subtitleSelected(value) }
        onActiveFocusChanged: if (activeFocus) panel.focusMoved(this)
        Keys.onUpPressed: event => { if (!popup.visible) nextItemInFocusChain(false).forceActiveFocus(); else event.accepted = false }
        Keys.onDownPressed: event => { if (!popup.visible) nextItemInFocusChain(true).forceActiveFocus(); else event.accepted = false }
    }
}
