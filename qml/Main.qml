import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import JFGamescope 1.0

ApplicationWindow {
    id: window
    width: 1280; height: 800
    minimumWidth: 800; minimumHeight: 500
    visible: true
    title: "JF Gamescope"
    color: "#101c29"
    readonly property color accent: Preferences.accent
    property var selectedMedia: ({})
    property var detailMedia: ({})
    property var selectedIndices: ({})
    property int activeTab: 0
    property bool controlsVisible: true
    property bool closingAfterStop: false
    visibility: startWindowed ? Window.Windowed : startFullscreen || Preferences.fullscreen ? Window.FullScreen : Window.Windowed
    onClosing: close => {
        if (!closingAfterStop && Player.active) {
            close.accepted = false
            closingAfterStop = true
            App.stopPlayback()
            quitDelay.start()
        }
    }
    Timer { id: quitDelay; interval: 350; onTriggered: Qt.quit() }
    function focusContent() {
        for (let i = 0; i < shelves.count; ++i) {
            let shelf = shelves.itemAt(i)
            if (shelf && shelf.visible) { shelf.focusFirst(); return }
        }
        if (libraryRepeater.count) libraryRepeater.itemAt(0).forceActiveFocus()
        else homeButton.forceActiveFocus()
    }
    function focusShelf(index, direction) {
        for (let i = index + direction; i >= 0 && i < shelves.count; i += direction) {
            let shelf = shelves.itemAt(i)
            if (shelf.visible) {
                shelf.focusFirst()
                mediaFlick.contentY = Math.max(0, Math.min(mediaFlick.contentHeight - mediaFlick.height, shelf.y - 12))
                return
            }
        }
        if (direction < 0) homeButton.forceActiveFocus()
        else if (App.hasMore) moreButton.forceActiveFocus()
    }
    function activate(media) {
        if (media.folder) App.open(media)
        else { detailMedia = media; detailPopup.open(); Qt.callLater(() => playButton.forceActiveFocus()) }
    }
    function tab(delta) {
        if (!App.signedIn || Player.active) return
        activeTab = (activeTab + delta + 3) % 3
        App.home(["Home", "Movies", "Shows"][activeTab])
    }
    Connections {
        target: App
        function onBrowsingReady() { Qt.callLater(window.focusContent) }
        function onChanged() { if (!App.signedIn) Qt.callLater(() => serverField.forceActiveFocus()) }
    }
    Shortcut { sequence: "F1"; onActivated: { options.open(); Qt.callLater(() => modeSetting.forceActiveFocus()) } }
    Shortcut { sequence: "F11"; onActivated: { Preferences.fullscreen = !Preferences.fullscreen; window.visibility = Preferences.fullscreen ? Window.FullScreen : Window.Windowed } }
    Shortcut { sequence: "PgUp"; onActivated: window.tab(-1) }
    Shortcut { sequence: "PgDown"; onActivated: window.tab(1) }
    Shortcut { sequence: "F"; onActivated: if (App.signedIn && !Player.active) { searchPopup.open(); searchField.forceActiveFocus() } }

    FocusScope {
        id: stage
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: {
            if (Player.active) App.stopPlayback()
            else if (App.canGoBack) App.back()
            else if (App.signedIn && App.title !== "Home") App.home()
            else { options.open(); Qt.callLater(() => modeSetting.forceActiveFocus()) }
        }
        Rectangle { anchors.fill: parent; gradient: Gradient { GradientStop { position: 0; color: "#172b3d" } GradientStop { position: 0.7; color: "#101c29" } } }

        Item {
            id: loginScreen
            anchors.fill: parent
            visible: !App.signedIn && !Player.active
            Rectangle {
                width: parent.width * 0.5; height: parent.height
                gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0; color: "#213f55" } GradientStop { position: 1; color: "#101c29" } }
            }
            Column {
                x: parent.width * 0.075; y: parent.height * 0.2; spacing: 24
                Row { spacing: 12
                    Image { width: 48; height: 48; source: "qrc:/assets/logo.svg" }
                    Column { spacing: 3
                        Text { text: "JF GAMESCOPE"; font.pixelSize: 21; font.weight: Font.Bold; font.letterSpacing: 2; color: "#f0f7fc" }
                        Text { text: "JELLYFIN, FROM YOUR COUCH"; font.pixelSize: 10; font.letterSpacing: 2; color: "#8ca8bf" }
                    }
                }
                Item { width: 1; height: 16 }
                Text { text: "Your library.\nDeck ready."; font.pixelSize: Math.min(58, window.width * 0.046); font.weight: Font.Bold; lineHeight: 1.12; color: "#f2f6fa" }
                Text { width: window.width * 0.34; text: "Pick something good. Settle in.\nYour movies and shows, a button away."; font.pixelSize: 17; lineHeight: 1.45; color: "#92adbf"; wrapMode: Text.WordWrap }
                Row { spacing: 8
                    Repeater { model: ["NATIVE", "CONTROLLER FIRST", "MPV"]
                        Rectangle { required property string modelData; width: badge.implicitWidth + 20; height: 25; color: "#223e50"; radius: 3
                            Text { id: badge; anchors.centerIn: parent; text: modelData; color: "#9cbbce"; font.pixelSize: 9; font.letterSpacing: 1 }
                        }
                    }
                }
            }
            ColumnLayout {
                width: Math.min(400, parent.width * 0.38); anchors { right: parent.right; rightMargin: parent.width * 0.08; verticalCenter: parent.verticalCenter }
                spacing: 13
                Text { text: "Make yourself at home"; font.pixelSize: 28; font.weight: Font.DemiBold; color: "#eff6fc" }
                Text { Layout.fillWidth: true; text: "Connect to your Jellyfin server to get started."; color: "#8ba6bb"; font.pixelSize: 14; wrapMode: Text.WordWrap }
                Item { Layout.preferredHeight: 10 }
                Text { text: "SERVER ADDRESS"; color: "#91aabe"; font.pixelSize: 11; font.letterSpacing: 1.5 }
                TextField {
                    id: serverField
                    objectName: "serverField"
                    Layout.fillWidth: true; implicitHeight: 49
                    text: App.savedServer; placeholderText: "http://192.168.1.10:8096"; color: "white"; placeholderTextColor: "#617e94"; font.pixelSize: 16; padding: 14
                    selectByMouse: true; activeFocusOnTab: true; inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                    background: Rectangle { color: "#0d1b28"; radius: 5; border.width: serverField.activeFocus ? 2 : 1; border.color: serverField.activeFocus ? window.accent : "#2a4255" }
                    Keys.onReturnPressed: keyboard.edit(serverField)
                    KeyNavigation.down: usernameField
                    KeyNavigation.up: optionsButton
                }
                Text { text: "USERNAME"; color: "#91aabe"; font.pixelSize: 11; font.letterSpacing: 1.5 }
                TextField {
                    id: usernameField
                    objectName: "usernameField"
                    Layout.fillWidth: true; implicitHeight: 49; text: App.savedUsername; placeholderText: "Your Jellyfin username"; color: "white"; placeholderTextColor: "#617e94"; font.pixelSize: 16; padding: 14; selectByMouse: true; activeFocusOnTab: true
                    background: Rectangle { color: "#0d1b28"; radius: 5; border.width: usernameField.activeFocus ? 2 : 1; border.color: usernameField.activeFocus ? window.accent : "#2a4255" }
                    Keys.onReturnPressed: keyboard.edit(usernameField)
                    KeyNavigation.up: serverField; KeyNavigation.down: passwordField
                }
                Text { text: "PASSWORD"; color: "#91aabe"; font.pixelSize: 11; font.letterSpacing: 1.5 }
                TextField {
                    id: passwordField
                    objectName: "passwordField"
                    Layout.fillWidth: true; implicitHeight: 49; echoMode: TextInput.Password; placeholderText: "Your password"; color: "white"; placeholderTextColor: "#617e94"; font.pixelSize: 16; padding: 14; activeFocusOnTab: true
                    background: Rectangle { color: "#0d1b28"; radius: 5; border.width: passwordField.activeFocus ? 2 : 1; border.color: passwordField.activeFocus ? window.accent : "#2a4255" }
                    Keys.onReturnPressed: keyboard.edit(passwordField)
                    KeyNavigation.up: usernameField; KeyNavigation.down: rememberButton
                }
                NavButton { id: rememberButton; objectName: "rememberButton"; Layout.fillWidth: true; text: (checked ? "✓" : "○") + "  Keep me signed in"; checkable: true; upTarget: passwordField; downTarget: signIn; implicitHeight: 38; font.pixelSize: 13 }
                NavButton { id: signIn; text: App.busy ? "Connecting…" : "Connect to Jellyfin  →"; primary: true; accent: window.accent; Layout.fillWidth: true; implicitHeight: 52; upTarget: rememberButton; downTarget: optionsButton; enabled: !App.busy; onClicked: { App.login(serverField.text, usernameField.text, passwordField.text, rememberButton.checked); passwordField.clear() } }
                NavButton { id: optionsButton; text: "Options"; Layout.fillWidth: true; implicitHeight: 38; font.pixelSize: 13; upTarget: signIn; downTarget: serverField; onClicked: { options.open(); modeSetting.forceActiveFocus() } }
            }
            Text { anchors { left: parent.left; bottom: parent.bottom; margins: 35 } text: "A  Edit / select     B  Options     D-PAD  Navigate"; color: "#7696ad"; font.pixelSize: 13 }
        }

        Item {
            id: libraryScreen
            anchors.fill: parent
            visible: App.signedIn && !Player.active
            Image { anchors { right: parent.right; top: parent.top } width: parent.width * 0.75; height: 390; source: window.selectedMedia.backdrop || ""; asynchronous: true; fillMode: Image.PreserveAspectCrop; opacity: 0.42 }
            Rectangle { width: parent.width; height: 400; gradient: Gradient { GradientStop { position: 0; color: "#00101c29" } GradientStop { position: 1; color: "#101c29" } } }
            Rectangle { width: parent.width * 0.8; height: 400; gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0; color: "#ee101c29" } GradientStop { position: 1; color: "#00101c29" } } }
            RowLayout {
                id: header
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 32 }
                spacing: 12
                Image { source: "qrc:/assets/logo.svg"; Layout.preferredWidth: 37; Layout.preferredHeight: 37 }
                Text { text: "GAMESCOPE"; color: "#e9f2f9"; font.pixelSize: 16; font.weight: Font.Bold; font.letterSpacing: 2; Layout.rightMargin: 24 }
                NavButton { id: homeButton; objectName: "homeButton"; text: "Home"; implicitWidth: 95; implicitHeight: 40; primary: window.activeTab === 0; accent: window.accent; Keys.onDownPressed: window.focusContent(); onClicked: { window.activeTab = 0; App.home() } }
                NavButton { text: "Movies"; implicitWidth: 95; implicitHeight: 40; primary: window.activeTab === 1; accent: window.accent; onClicked: { window.activeTab = 1; App.home("Movies") } }
                NavButton { text: "Shows"; implicitWidth: 95; implicitHeight: 40; primary: window.activeTab === 2; accent: window.accent; onClicked: { window.activeTab = 2; App.home("Shows") } }
                Item { Layout.fillWidth: true }
                Text { text: App.username; color: "#93aec1"; font.pixelSize: 13; Layout.maximumWidth: 110; elide: Text.ElideRight }
                NavButton { text: "Search"; implicitWidth: 90; implicitHeight: 40; onClicked: { searchPopup.open(); searchField.forceActiveFocus() } }
                NavButton { text: "Options"; implicitWidth: 100; implicitHeight: 40; onClicked: { options.open(); modeSetting.forceActiveFocus() } }
            }
            Column {
                x: 42; y: 116; spacing: 10; width: parent.width - 84
                Text { text: App.title === "Home" ? "READY WHEN YOU ARE" : App.title.toUpperCase(); color: window.accent; font.pixelSize: 11; font.letterSpacing: 3 }
                Text { width: parent.width; text: window.selectedMedia.title || "Your next great watch"; color: "#f5f8fb"; font.pixelSize: Math.min(42, window.width * 0.034); font.weight: Font.Bold; elide: Text.ElideRight }
                Text { text: window.selectedMedia.subtitle || "Movies. Shows. A little time for yourself."; color: "#9db4c6"; font.pixelSize: 14 }
                Text { width: Math.min(parent.width * 0.65, 730); text: window.selectedMedia.overview || "Browse your library with the D-pad. Press A to take a closer look."; color: "#a9bdcb"; font.pixelSize: 14; maximumLineCount: 2; elide: Text.ElideRight; wrapMode: Text.WordWrap; lineHeight: 1.25 }
            }
            Flickable {
                id: mediaFlick
                anchors { left: parent.left; right: parent.right; top: parent.top; bottom: footer.top; leftMargin: 42; rightMargin: 42; topMargin: 290 }
                clip: true; contentHeight: shelfColumn.height + 15; boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                Column {
                    id: shelfColumn
                    width: mediaFlick.width; spacing: 21
                    Column {
                        width: parent.width; spacing: 15; visible: App.title === "Home" && App.libraries.length > 0
                        Text { text: "Your libraries"; color: "#d4e3ef"; font.pixelSize: 20; font.weight: Font.DemiBold }
                        Flow { width: parent.width; spacing: 12
                            Repeater { id: libraryRepeater; model: App.libraries
                                NavButton { required property var modelData; text: modelData.title; accent: window.accent; implicitHeight: 46; onClicked: App.open(modelData); downTarget: shelves.count ? shelves.itemAt(0) : homeButton; upTarget: homeButton }
                            }
                        }
                    }
                    Repeater {
                        id: shelves
                        model: App.shelves
                        MediaShelf {
                            required property var modelData
                            required property int index
                            objectName: "shelf-" + index
                            width: shelfColumn.width
                            title: modelData.title; items: modelData.items; accent: window.accent
                            initialIndex: window.selectedIndices[modelData.title] || 0
                            cardWidth: 156 * Preferences.uiScale; cardHeight: 226 * Preferences.uiScale
                            onSelected: media => { window.selectedMedia = media; window.selectedIndices[title] = itemIndex }
                            onActivated: media => window.activate(media)
                            onMoveUp: window.focusShelf(index, -1)
                            onMoveDown: window.focusShelf(index, 1)
                        }
                    }
                    NavButton { id: moreButton; text: App.busy ? "Loading…" : "Load more titles"; visible: App.hasMore; enabled: !App.busy; onClicked: App.loadMore() }
                    Text { visible: !App.busy && App.shelves.every(s => s.items.length === 0); text: "Nothing here yet. Try another library or a search."; color: "#94adbf"; font.pixelSize: 18; topPadding: 30 }
                    BusyIndicator { running: App.busy; visible: running; width: 40; height: 40 }
                }
            }
            Rectangle {
                id: footer
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 52; color: "#132231"
                Text { anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 42 } text: "A  Select     B  Back     Y  Search     LB / RB  Browse     ☰  Options"; color: "#93aebf"; font.pixelSize: 13 }
                Text { anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 42 } text: "JELLYFIN  /  " + new Date().toLocaleTimeString(Qt.locale(), "hh:mm"); color: "#60849e"; font.pixelSize: 12; font.letterSpacing: 1 }
            }
        }

        FocusScope {
            id: playback
            anchors.fill: parent
            visible: Player.active
            onVisibleChanged: { if (visible) { window.controlsVisible = true; forceActiveFocus(); hideControls.restart() } else window.focusContent() }
            Rectangle { anchors.fill: parent; color: "black" }
            MpvVideo { anchors.fill: parent; player: Player; visible: Player.active }
            Keys.onPressed: event => {
                window.controlsVisible = true; hideControls.restart()
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) Player.togglePause()
                else if (event.key === Qt.Key_Left) App.seekRelative(-10)
                else if (event.key === Qt.Key_Right) App.seekRelative(10)
                else if (event.key === Qt.Key_Down) audioButton.forceActiveFocus()
                else if (event.key === Qt.Key_Escape) App.stopPlayback()
                else if (event.key !== Qt.Key_Up && event.key !== Qt.Key_I) return
                event.accepted = true
            }
            MouseArea { anchors.fill: parent; hoverEnabled: true; onPositionChanged: { window.controlsVisible = true; hideControls.restart() } onClicked: { window.controlsVisible = true; hideControls.restart(); playback.forceActiveFocus(); Player.togglePause() } }
            BusyIndicator { anchors.centerIn: parent; running: Player.loading; visible: running; width: 65; height: 65 }
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 230; visible: window.controlsVisible || Player.paused
                gradient: Gradient { GradientStop { position: 0; color: "#00000000" } GradientStop { position: 1; color: "#ee06101a" } }
                ColumnLayout {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 35 } spacing: 13
                    RowLayout {
                        Text { text: App.currentTitle; color: "white"; font.pixelSize: 26; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: App.playMethod === "DirectPlay" ? "DIRECT PLAY" : "TRANSCODING"; color: window.accent; font.pixelSize: 11; font.letterSpacing: 2 }
                    }
                    Rectangle { Layout.fillWidth: true; height: 4; color: "#4d6576"
                        Rectangle { height: 4; width: parent.width * Math.min(1, Player.duration > 0 ? Player.position / Player.duration : 0); color: window.accent }
                    }
                    RowLayout {
                        Text { text: (Player.paused ? "PAUSED   " : "") + time(Player.position) + " / " + time(Player.duration); color: "#c3d7e4"; font.pixelSize: 14
                            function time(seconds) { let s = Math.max(0, Math.floor(seconds)); return Math.floor(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + s % 60 }
                        }
                        Item { Layout.fillWidth: true }
                        Text { text: "A / X  Pause     ← / →  Seek     B  Stop"; color: "#a4bccd"; font.pixelSize: 13 }
                    }
                    RowLayout {
                        id: playbackButtons
                        spacing: 10
                        NavButton { id: audioButton; text: "Audio: " + Player.audioTrack; implicitWidth: 240; upTarget: playback; onClicked: Player.cycleAudio() }
                        NavButton { text: "Subtitles: " + Player.subtitleTrack; implicitWidth: 240; upTarget: playback; onClicked: Player.cycleSubtitle() }
                        NavButton { text: Player.paused ? "Resume" : "Pause"; upTarget: playback; onClicked: Player.togglePause() }
                        NavButton { text: "Stop playback"; upTarget: playback; onClicked: App.stopPlayback() }
                    }
                }
            }
            Timer { id: hideControls; interval: 4000; onTriggered: if (window.activeFocusItem === playback && !Player.paused) window.controlsVisible = false }
        }
    }

    Keyboard { id: keyboard; parent: Overlay.overlay; accent: window.accent }
    Popup {
        id: detailPopup
        parent: Overlay.overlay; width: Math.min(820, window.width - 50); height: Math.min(500, window.height - 60); x: (window.width - width) / 2; y: (window.height - height) / 2; modal: true; focus: true; padding: 30
        background: Rectangle { color: "#152a3c"; radius: 8; border.color: "#36536a" }
        contentItem: RowLayout {
            spacing: 28
            Rectangle { Layout.preferredWidth: 195; Layout.fillHeight: true; color: "#223e54"; radius: 4
                Image { anchors.fill: parent; source: window.detailMedia.poster || ""; fillMode: Image.PreserveAspectCrop; asynchronous: true }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 15
                Text { Layout.fillWidth: true; text: window.detailMedia.title || ""; wrapMode: Text.WordWrap; color: "white"; font.pixelSize: 29; font.weight: Font.Bold }
                Text { text: window.detailMedia.subtitle || ""; color: window.accent; font.pixelSize: 14 }
                ScrollView { Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    Text { width: parent.width; text: window.detailMedia.overview || "No description available."; wrapMode: Text.WordWrap; color: "#afc5d5"; font.pixelSize: 16; lineHeight: 1.35 }
                }
                NavButton { id: playButton; Layout.fillWidth: true; text: App.busy ? "Preparing…" : window.detailMedia.progress > 0 ? "▶  Resume" : "▶  Play"; primary: true; accent: window.accent; enabled: !App.busy; onClicked: { App.play(window.detailMedia); detailPopup.close() } }
                NavButton { Layout.fillWidth: true; text: "Play from beginning"; visible: window.detailMedia.progress > 0; onClicked: { App.play(window.detailMedia, false); detailPopup.close() } }
                NavButton { Layout.fillWidth: true; text: "Back"; onClicked: detailPopup.close() }
            }
        }
    }
    Popup {
        id: searchPopup
        parent: Overlay.overlay; width: Math.min(650, window.width - 50); height: 240; x: (window.width - width) / 2; y: (window.height - height) / 2; modal: true; focus: true; padding: 28
        background: Rectangle { color: "#152a3c"; radius: 8; border.color: "#36536a" }
        contentItem: ColumnLayout {
            spacing: 15
            Text { text: "Find your next watch"; color: "white"; font.pixelSize: 25; font.weight: Font.DemiBold }
            TextField { id: searchField; Layout.fillWidth: true; implicitHeight: 50; font.pixelSize: 20; placeholderText: "Search movies and shows"; selectByMouse: true; Keys.onReturnPressed: keyboard.edit(searchField); KeyNavigation.down: searchButton }
            NavButton { id: searchButton; text: "Search library"; primary: true; accent: window.accent; Layout.fillWidth: true; upTarget: searchField; onClicked: { App.search(searchField.text); searchPopup.close() } }
        }
    }
    Popup {
        id: options
        objectName: "optionsPopup"
        parent: Overlay.overlay; width: Math.min(690, window.width - 50); height: Math.min(660, window.height - 50); x: (window.width - width) / 2; y: (window.height - height) / 2; modal: true; focus: true; padding: 30
        background: Rectangle { color: "#152a3c"; radius: 8; border.color: "#36536a" }
        contentItem: ColumnLayout {
            spacing: 10
            Text { text: "Make it yours"; color: "white"; font.pixelSize: 29; font.weight: Font.Bold }
            Text { text: "PLAYBACK & DISPLAY"; color: "#89a8be"; font.pixelSize: 11; font.letterSpacing: 2; bottomPadding: 8 }
            NavButton { id: modeSetting; objectName: "modeSetting"; Layout.fillWidth: true; text: "Playback  ·  " + ({auto: "Automatic", direct: "Direct play only", transcode: "Always transcode"})[Preferences.playbackMode]; onClicked: { let modes = ["auto", "direct", "transcode"]; Preferences.playbackMode = modes[(modes.indexOf(Preferences.playbackMode) + 1) % 3] } }
            NavButton { Layout.fillWidth: true; text: "Streaming limit  ·  " + Preferences.bitrate / 1000000 + " Mbps"; onClicked: { let rates = [4000000, 8000000, 20000000, 40000000, 80000000, 120000000]; Preferences.bitrate = rates[(rates.indexOf(Preferences.bitrate) + 1) % rates.length] } }
            NavButton { Layout.fillWidth: true; text: "Hardware decoding  ·  " + (Preferences.hardwareDecoding ? "Automatic" : "Off"); onClicked: Preferences.hardwareDecoding = !Preferences.hardwareDecoding }
            NavButton { Layout.fillWidth: true; text: "Fullscreen  ·  " + (Preferences.fullscreen ? "On" : "Off"); onClicked: { Preferences.fullscreen = !Preferences.fullscreen; window.visibility = Preferences.fullscreen ? Window.FullScreen : Window.Windowed } }
            NavButton { Layout.fillWidth: true; text: "Poster size  ·  " + Math.round(Preferences.uiScale * 100) + "%"; onClicked: Preferences.uiScale = Preferences.uiScale >= 1.24 ? 0.85 : Math.min(1.25, Preferences.uiScale + 0.1) }
            NavButton { Layout.fillWidth: true; text: "Accent  ·  " + ({"#66c0f4": "Steam blue", "#76dfba": "Sea glass", "#f3bb70": "Warm amber"})[Preferences.accent]; onClicked: { let colors = ["#66c0f4", "#76dfba", "#f3bb70"]; Preferences.accent = colors[(colors.indexOf(Preferences.accent) + 1) % 3] } }
            Text { Layout.fillWidth: true; text: "Playback changes apply to the next stream.\nController: " + Controller.name; color: "#8eacbf"; font.pixelSize: 12; wrapMode: Text.WordWrap; topPadding: 4 }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                NavButton { text: "Sign out"; visible: App.signedIn; onClicked: { App.logout(); options.close() } }
                Item { Layout.fillWidth: true }
                NavButton { text: "Quit"; onClicked: window.close() }
                NavButton { text: "Done"; primary: true; accent: window.accent; onClicked: options.close() }
            }
        }
    }
    Popup {
        id: notification
        parent: Overlay.overlay; width: Math.min(700, window.width - 50); height: messageColumn.implicitHeight + 48; x: (window.width - width) / 2; y: 30; modal: true; focus: true; padding: 24
        visible: App.message.length > 0
        closePolicy: Popup.CloseOnEscape
        onClosed: { App.clearMessage(); window.focusContent() }
        onOpened: Qt.callLater(() => dismissMessage.forceActiveFocus())
        background: Rectangle { color: "#293748"; radius: 8; border.color: window.accent }
        contentItem: ColumnLayout {
            id: messageColumn; spacing: 18
            Text { Layout.fillWidth: true; text: App.message; color: "white"; font.pixelSize: 17; wrapMode: Text.WordWrap }
            NavButton { id: dismissMessage; text: "Dismiss"; accent: window.accent; onClicked: notification.close() }
        }
    }
    Component.onCompleted: serverField.forceActiveFocus()
}
