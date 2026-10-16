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
    readonly property real uiFactor: Math.max(1, Math.min(width / 1280, height / 720)) * Preferences.uiScale
    onUiFactorChanged: Qt.callLater(() => { if (options.opened && uiScaleSetting.activeFocus) options.reveal(uiScaleSetting) })
    readonly property bool gamepad: Controller.name !== "Keyboard"
    property var selectedMedia: ({})
    property var selectedIndices: ({})
    property int activeTab: 0
    property alias controlsVisible: playback.controlsVisible
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
        if (Player.active || App.playbackPending) return
        if (options.opened || notification.opened || exitPrompt.opened || searchPopup.opened) return
        if (detailView.opened) { detailView.restoreFocus(); return }
        if (libraryGrid.visible) { libraryGrid.focusFirst(); return }
        for (let i = 0; i < shelves.count; ++i) {
            let shelf = shelves.itemAt(i)
            if (shelf && shelf.visible && !shelf.loading) { shelf.focusFirst(); return }
        }
        homeButton.forceActiveFocus()
    }
    function focusShelf(index, direction) {
        for (let i = index + direction; i >= 0 && i < shelves.count; i += direction) {
            let shelf = shelves.itemAt(i)
            if (shelf.visible && !shelf.loading) {
                shelf.focusFirst()
                mediaFlick.contentY = Math.max(0, Math.min(mediaFlick.contentHeight - mediaFlick.height, shelf.y - 12))
                return
            }
        }
        if (direction < 0) {
            if (App.canFilter) { mediaFlick.contentY = 0; filterButton.forceActiveFocus() }
            else homeButton.forceActiveFocus()
        }
        else if (App.hasMore) moreButton.forceActiveFocus()
    }
    function activate(media) {
        if (media.type === "Season") { App.openSeries(media); detailView.openAtSeason(media.id) }
        else if (media.type === "CollectionFolder" || media.type === "Folder") App.open(media)
        else { App.showDetails(media); detailView.open() }
    }
    function edit(field) {
        field.forceActiveFocus()
        const position = field.mapToItem(window.contentItem, 0, 0)
        if (!Controller.showKeyboard(Math.round(position.x), Math.round(position.y), Math.round(field.width), Math.round(field.height)))
            App.notify("Could not open the Steam keyboard. Start Steam and try again, or use a physical keyboard.")
    }
    function tab(delta) {
        if (!App.signedIn || Player.active || App.playbackPending || detailView.opened) return
        activeTab = (activeTab + delta + App.libraries.length + 1) % (App.libraries.length + 1)
        if (activeTab === 0) App.home()
        else App.openLibrary(App.libraries[activeTab - 1])
    }
    Connections {
        target: App
        function onBrowsingReady() { if (!Player.active && !App.playbackPending) Qt.callLater(window.focusContent) }
        function onChanged() { if (!App.signedIn) { detailView.close(); if (!App.authenticating) Qt.callLater(() => serverField.forceActiveFocus()) } else if (App.title === "Home") window.activeTab = 0 }
    }
    Connections {
        target: Controller
        function onKeyboardRequested() { if (window.activeFocusItem) window.edit(window.activeFocusItem) }
    }
    Shortcut { sequence: "F2"; onActivated: { const field = window.activeFocusItem; if (field && field.cursorPosition !== undefined) window.edit(field) } }
    Shortcut { sequence: "F1"; onActivated: { options.open(); Qt.callLater(() => modeSetting.forceActiveFocus()) } }
    Shortcut { sequence: "F11"; onActivated: { Preferences.fullscreen = !Preferences.fullscreen; window.visibility = Preferences.fullscreen ? Window.FullScreen : Window.Windowed } }
    Shortcut { sequence: "PgUp"; onActivated: window.tab(-1) }
    Shortcut { sequence: "PgDown"; onActivated: window.tab(1) }
    Shortcut { sequence: "F"; onActivated: if (App.signedIn && !Player.active && !App.playbackPending && !detailView.opened) { searchPopup.open(); searchField.forceActiveFocus() } }

    Overlay.overlay.transform: Scale { xScale: window.uiFactor; yScale: window.uiFactor }
    Overlay.overlay.width: stage.width
    Overlay.overlay.height: stage.height

    FocusScope {
        id: stage
        width: window.width / window.uiFactor; height: window.height / window.uiFactor
        scale: window.uiFactor; transformOrigin: Item.TopLeft
        focus: true
        Keys.onEscapePressed: {
            if (Player.active || App.playbackPending) playback.back()
            else if (detailView.opened) detailView.back()
            else if (App.canGoBack) App.back()
            else if (App.signedIn && App.title !== "Home") { window.activeTab = 0; App.home() }
            else exitPrompt.open()
        }
        Rectangle { anchors.fill: parent; gradient: Gradient { GradientStop { position: 0; color: "#172b3d" } GradientStop { position: 0.7; color: "#101c29" } } }

        Item {
            id: loginScreen
            anchors.fill: parent
            objectName: "loginScreen"
            visible: !App.authenticating && !App.signedIn && !Player.active && !App.playbackPending
            IconButton {
                id: optionsButton
                anchors { right: parent.right; top: parent.top; margins: 24 }
                text: "Settings"; iconName: "settings"
                downTarget: serverField; upTarget: signIn
                onClicked: { options.open(); modeSetting.forceActiveFocus() }
            }
            ColumnLayout {
                width: Math.min(400, parent.width - 80); anchors.centerIn: parent
                spacing: 10
                RowLayout {
                    spacing: 12; Layout.bottomMargin: 20
                    Image { source: "qrc:/assets/logo.svg"; Layout.preferredWidth: 40; Layout.preferredHeight: 40 }
                    Text { text: "JF Gamescope"; color: "#eff6fc"; font.pixelSize: 27; font.weight: Font.DemiBold }
                }
                Text { text: "Server"; color: "#91aabe"; font.pixelSize: 13 }
                TextField {
                    id: serverField
                    objectName: "serverField"
                    Layout.fillWidth: true; implicitHeight: 46
                    text: App.savedServer; placeholderText: "http://192.168.1.10:8096"; color: "white"; placeholderTextColor: "#617e94"; font.pixelSize: 16; padding: 14
                    selectByMouse: true; activeFocusOnTab: true; inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                    background: Rectangle { color: "#0d1b28"; radius: 5; border.width: serverField.activeFocus ? 2 : 1; border.color: serverField.activeFocus ? window.accent : "#2a4255" }
                    Keys.onReturnPressed: usernameField.forceActiveFocus()
                    TapHandler { onTapped: window.edit(serverField) }
                    KeyNavigation.down: usernameField
                    KeyNavigation.up: optionsButton
                }
                Text { text: "Username"; color: "#91aabe"; font.pixelSize: 13 }
                TextField {
                    id: usernameField
                    objectName: "usernameField"
                    Layout.fillWidth: true; implicitHeight: 46; text: App.savedUsername; color: "white"; font.pixelSize: 16; padding: 14; selectByMouse: true; activeFocusOnTab: true
                    background: Rectangle { color: "#0d1b28"; radius: 5; border.width: usernameField.activeFocus ? 2 : 1; border.color: usernameField.activeFocus ? window.accent : "#2a4255" }
                    Keys.onReturnPressed: passwordField.forceActiveFocus()
                    TapHandler { onTapped: window.edit(usernameField) }
                    KeyNavigation.up: serverField; KeyNavigation.down: passwordField
                }
                Text { text: "Password"; color: "#91aabe"; font.pixelSize: 13 }
                TextField {
                    id: passwordField
                    objectName: "passwordField"
                    Layout.fillWidth: true; implicitHeight: 46; echoMode: TextInput.Password; color: "white"; font.pixelSize: 16; padding: 14; activeFocusOnTab: true
                    background: Rectangle { color: "#0d1b28"; radius: 5; border.width: passwordField.activeFocus ? 2 : 1; border.color: passwordField.activeFocus ? window.accent : "#2a4255" }
                    Keys.onReturnPressed: rememberButton.forceActiveFocus()
                    TapHandler { onTapped: window.edit(passwordField) }
                    KeyNavigation.up: usernameField; KeyNavigation.down: rememberButton
                }
                NavButton { id: rememberButton; objectName: "rememberButton"; Layout.fillWidth: true; text: (checked ? "✓" : "○") + "  Keep me signed in"; checkable: true; flat: true; upTarget: passwordField; downTarget: signIn; implicitHeight: 38; font.pixelSize: 13 }
                NavButton { id: signIn; objectName: "signInButton"; text: App.busy ? "Connecting…" : "Sign in"; primary: true; accent: window.accent; Layout.fillWidth: true; implicitHeight: 52; upTarget: rememberButton; downTarget: optionsButton; enabled: !App.busy; onClicked: { App.login(serverField.text, usernameField.text, passwordField.text, rememberButton.checked); passwordField.clear() } }
            }
        }

        Item {
            id: libraryScreen
            objectName: "libraryScreen"
            anchors.fill: parent
            visible: App.signedIn && !Player.active && !App.playbackPending && !detailView.opened
            Artwork { anchors { right: parent.right; top: parent.top } width: parent.width * 0.75; height: 390; url: window.selectedMedia.backdrop || ""; blurHash: window.selectedMedia.backdropBlurHash || ""; load: libraryScreen.visible; opacity: 0.42 }
            Rectangle { width: parent.width; height: 400; gradient: Gradient { GradientStop { position: 0; color: "#00101c29" } GradientStop { position: 1; color: "#101c29" } } }
            Rectangle { width: parent.width * 0.8; height: 400; gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0; color: "#ee101c29" } GradientStop { position: 1; color: "#00101c29" } } }
            RowLayout {
                id: header
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 32 }
                spacing: 8
                IconButton { text: "Back"; iconName: "back"; visible: App.title !== "Home"; onClicked: { if (App.canGoBack) App.back(); else { window.activeTab = 0; App.home() } } }
                Image { source: "qrc:/assets/logo.svg"; Layout.preferredWidth: 37; Layout.preferredHeight: 37 }
                NavButton { id: homeButton; objectName: "homeButton"; text: "Home"; implicitWidth: 95; implicitHeight: 40; primary: window.activeTab === 0; accent: window.accent; Keys.onDownPressed: window.focusContent(); onClicked: { window.activeTab = 0; App.home() } }
                Flickable {
                    Layout.fillWidth: true; Layout.minimumWidth: 100; Layout.preferredHeight: 44
                    contentWidth: libraryTabs.width; clip: true; boundsBehavior: Flickable.StopAtBounds
                    Row {
                        id: libraryTabs; spacing: 8
                        Repeater {
                            model: App.libraries
                            NavButton {
                                required property var modelData
                                required property int index
                                objectName: "libraryTab-" + index
                                text: modelData.title; implicitHeight: 40; implicitWidth: Math.min(200, Math.max(95, implicitContentWidth + 32))
                                primary: window.activeTab === index + 1; accent: window.accent
                                Keys.onDownPressed: window.focusContent()
                                onActiveFocusChanged: if (activeFocus) { const view = libraryTabs.parent; view.contentX = Math.max(0, Math.min(view.contentWidth - view.width, x)) }
                                onClicked: { window.activeTab = index + 1; App.openLibrary(modelData) }
                            }
                        }
                    }
                }
                IconButton { text: "Search"; iconName: "search"; onClicked: { searchPopup.open(); searchField.forceActiveFocus() } }
                ProfileButton { objectName: "profileButton"; onClicked: { options.open(); modeSetting.forceActiveFocus() } }
            }
            Column {
                visible: !App.canFilter
                x: 42; y: stage.height < 620 ? 92 : 116; spacing: 10; width: parent.width - 84
                Text { width: parent.width; text: window.selectedMedia.title || ""; color: "#f5f8fb"; font.pixelSize: Math.min(stage.height < 620 ? 28 : 42, stage.width * 0.034); font.weight: Font.Bold; elide: Text.ElideRight }
                Text { width: parent.width; elide: Text.ElideRight; text: window.selectedMedia.subtitle || ""; color: "#9db4c6"; font.pixelSize: 14 }
                Text { visible: stage.height >= 620; width: Math.min(parent.width * 0.65, 730); text: window.selectedMedia.overview || ""; color: "#a9bdcb"; font.pixelSize: 14; maximumLineCount: 2; elide: Text.ElideRight; wrapMode: Text.WordWrap; lineHeight: 1.25 }
            }
            Flickable {
                id: mediaFlick
                objectName: "mediaFlick"
                anchors { left: parent.left; right: parent.right; top: parent.top; bottom: footer.top; leftMargin: 42; rightMargin: 42; topMargin: App.canFilter ? 116 : Math.min(260, stage.height * 0.36) }
                clip: true; contentHeight: shelfColumn.height + 15; boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                Column {
                    id: shelfColumn
                    width: mediaFlick.width; spacing: 21
                    Row {
                        id: browseTools
                        spacing: 10; visible: App.canFilter
                        NavButton { id: filterButton; objectName: "filterButton"; flat: true; text: App.libraryFilter === "All" ? "All titles" : App.libraryFilter; Keys.onDownPressed: window.focusContent(); onClicked: { let filters = ["All", "Favorites", "Unwatched", "Watched"]; App.setLibraryFilter(filters[(filters.indexOf(App.libraryFilter) + 1) % filters.length]) } }
                        NavButton { flat: true; text: "Sort"; value: App.sortOrder; Keys.onDownPressed: window.focusContent(); onClicked: { let sorts = ["Name", "Recently added", "Rating"]; App.setSortOrder(sorts[(sorts.indexOf(App.sortOrder) + 1) % sorts.length]) } }
                    }
                    Repeater {
                        id: shelves
                        model: App.canFilter ? [] : App.shelves
                        MediaShelf {
                            required property var modelData
                            required property int index
                            objectName: "shelf-" + index
                            width: shelfColumn.width
                            title: modelData.title; items: modelData.items; loading: !!modelData.loading; accent: window.accent
                            initialIndex: window.selectedIndices[modelData.title] || 0
                            cardWidth: 156; cardHeight: Math.min(226, Math.max(100, mediaFlick.height - 89))
                            loadArtwork: libraryScreen.visible && y + shelfColumn.y + height >= mediaFlick.contentY && y + shelfColumn.y <= mediaFlick.contentY + mediaFlick.height
                            onSelected: (media, selectedIndex) => { window.selectedMedia = media; window.selectedIndices[title] = selectedIndex }
                            onActivated: media => window.activate(media)
                            onMoveUp: window.focusShelf(index, -1)
                            onMoveDown: window.focusShelf(index, 1)
                        }
                    }
                    MediaGrid {
                        id: libraryGrid
                        objectName: "libraryGrid"
                        width: shelfColumn.width - 14
                        title: App.title; items: App.canFilter && App.shelves.length ? App.shelves[0].items : []
                        accent: window.accent; preferredCardWidth: 156
                        loadArtwork: libraryScreen.visible
                        viewportTop: mediaFlick.contentY - y; viewportHeight: mediaFlick.height
                        onSelected: (media, selectedIndex) => { window.selectedMedia = media; window.selectedIndices[title] = selectedIndex }
                        onActivated: media => window.activate(media)
                        onMoveUp: { mediaFlick.contentY = 0; filterButton.forceActiveFocus() }
                        onMoveDown: if (App.hasMore) moreButton.forceActiveFocus()
                        onRevealRequested: (top, itemHeight) => {
                            const position = y + top
                            if (position < mediaFlick.contentY) mediaFlick.contentY = position
                            else if (position + itemHeight > mediaFlick.contentY + mediaFlick.height)
                                mediaFlick.contentY = position + itemHeight - mediaFlick.height
                        }
                    }
                    NavButton {
                        id: moreButton; objectName: "moreButton"
                        text: App.busy ? "Loading…" : "Load more"; visible: App.hasMore; enabled: !App.busy
                        upTarget: libraryGrid.visible ? libraryGrid : null
                        onActiveFocusChanged: if (activeFocus) mediaFlick.contentY = Math.max(0, y + height - mediaFlick.height)
                        onClicked: App.loadMore()
                    }
                    Text { visible: !App.busy && App.shelves.every(s => s.items.length === 0); text: "No titles"; color: "#94adbf"; font.pixelSize: 18; topPadding: 30 }
                    SkeletonShelf { width: shelfColumn.width; visible: App.canFilter && App.busy; grid: true }
                }
            }
            Rectangle {
                id: footer
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 40; color: "transparent"
                Text { anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 42 } text: window.gamepad ? "A  Select     B  " + (App.title === "Home" ? "Exit" : "Back") + "     Y  Search     LB / RB  Browse     ☰  Settings" : "Enter  Select     Esc  " + (App.title === "Home" ? "Exit" : "Back") + "     F  Search     F1  Settings"; color: "#93aebf"; font.pixelSize: 12 }
            }
        }

        Rectangle {
            objectName: "authLoadingScreen"
            anchors.fill: parent; color: "#101c29"; visible: App.authenticating
            Column {
                anchors.centerIn: parent; spacing: 22
                Image { source: "qrc:/assets/logo.svg"; width: 64; height: 64; anchors.horizontalCenter: parent.horizontalCenter }
                Text { text: "Connecting to Jellyfin…"; color: "#eff6fc"; font.pixelSize: 24 }
                BusyIndicator { running: parent.parent.visible; width: 48; height: 48; anchors.horizontalCenter: parent.horizontalCenter }
            }
        }

        DetailView {
            id: detailView
            anchors.fill: parent
            accent: window.accent
            onSettingsRequested: { options.open(); modeSetting.forceActiveFocus() }
            onClosed: { App.closeDetails(); Qt.callLater(window.focusContent) }
        }
        PlayerView {
            id: playback
            anchors.fill: parent
            accent: window.accent
            onVisibleChanged: if (!visible) Qt.callLater(window.focusContent)
        }
    }

    Popup {
        id: searchPopup
        parent: Overlay.overlay; width: Math.min(650, stage.width - 50); height: 114; x: (stage.width - width) / 2; y: (stage.height - height) / 2; modal: true; focus: true; padding: 28
        background: Rectangle { color: "#152a3c"; radius: 8; border.color: "#36536a" }
        contentItem: RowLayout {
            spacing: 12
            TextField {
                id: searchField; objectName: "searchField"
                Layout.fillWidth: true; implicitHeight: 50; font.pixelSize: 18; placeholderText: "Movies, shows, and episodes"; selectByMouse: true
                Accessible.name: "Search library"
                Keys.onReturnPressed: { App.search(text); searchPopup.close() }
                TapHandler { onTapped: window.edit(searchField) }
                KeyNavigation.down: searchButton
            }
            NavButton { id: searchButton; text: "Search"; primary: true; accent: window.accent; upTarget: searchField; leftTarget: searchField; onClicked: { App.search(searchField.text); searchPopup.close() } }
        }
    }
    Popup {
        id: options
        objectName: "optionsPopup"
        parent: Overlay.overlay; width: Math.min(620, stage.width - 50); height: Math.min(610, stage.height - 50); x: (stage.width - width) / 2; y: (stage.height - height) / 2; modal: true; focus: true; padding: 24
        function reveal(item) {
            const point = item.mapToItem(settingsColumn, 0, 0)
            if (point.y < settingsScroll.contentY) settingsScroll.contentY = point.y
            else if (point.y + item.height > settingsScroll.contentY + settingsScroll.height)
                settingsScroll.contentY = point.y + item.height - settingsScroll.height
        }
        background: Rectangle { color: "#152a3c"; radius: 8; border.color: "#36536a" }
        contentItem: ColumnLayout {
            spacing: 16
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Settings"; color: "white"; font.pixelSize: 27; font.weight: Font.DemiBold; Layout.fillWidth: true }
                IconButton { text: "Close"; iconName: "close"; downTarget: modeSetting; onClicked: options.close() }
            }
            Flickable {
                id: settingsScroll
                objectName: "settingsScroll"
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true; contentHeight: settingsColumn.implicitHeight; boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                ColumnLayout {
                    id: settingsColumn
                    width: settingsScroll.width - 12; spacing: 6
                    NavButton { id: modeSetting; objectName: "modeSetting"; Layout.fillWidth: true; text: "Playback"; value: ({auto: "Automatic", direct: "Direct only", transcode: "Transcode"})[Preferences.playbackMode]; flat: true; onActiveFocusChanged: if (activeFocus) options.reveal(this); onClicked: { let modes = ["auto", "direct", "transcode"]; Preferences.playbackMode = modes[(modes.indexOf(Preferences.playbackMode) + 1) % 3] } }
                    NavButton { Layout.fillWidth: true; text: "Streaming limit"; value: Preferences.bitrate / 1000000 + " Mbps"; flat: true; onActiveFocusChanged: if (activeFocus) options.reveal(this); onClicked: { let rates = [4000000, 8000000, 20000000, 40000000, 80000000, 120000000]; Preferences.bitrate = rates[(rates.indexOf(Preferences.bitrate) + 1) % rates.length] } }
                    NavButton { Layout.fillWidth: true; text: "Hardware decoding"; value: Preferences.hardwareDecoding ? "Automatic" : "Off"; flat: true; onActiveFocusChanged: if (activeFocus) options.reveal(this); onClicked: Preferences.hardwareDecoding = !Preferences.hardwareDecoding }
                    NavButton { objectName: "themeSongsSetting"; Layout.fillWidth: true; text: "Theme songs"; value: Preferences.themeSongs ? "On" : "Off"; flat: true; onActiveFocusChanged: if (activeFocus) options.reveal(this); onClicked: Preferences.themeSongs = !Preferences.themeSongs }
                    ColumnLayout {
                        id: themeVolumeRow
                        Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 16; spacing: 0
                        visible: Preferences.themeSongs
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: "Theme volume"; color: "#edf5fb"; font.pixelSize: 16 }
                            Text { text: Preferences.themeSongVolume + "%"; color: window.accent; font.pixelSize: 16 }
                        }
                        Slider {
                            id: themeVolume
                            objectName: "themeVolumeSetting"
                            Layout.fillWidth: true; implicitHeight: 38
                            from: 0; to: 100; stepSize: 5; value: Preferences.themeSongVolume
                            activeFocusOnTab: true
                            Accessible.name: "Theme song volume"
                            onMoved: Preferences.themeSongVolume = value
                            onActiveFocusChanged: if (activeFocus) options.reveal(themeVolumeRow)
                            Keys.onUpPressed: themeVolume.nextItemInFocusChain(false).forceActiveFocus()
                            Keys.onDownPressed: fullscreenSetting.forceActiveFocus()
                            background: Rectangle {
                                x: themeVolume.leftPadding; y: themeVolume.topPadding + (themeVolume.availableHeight - height) / 2
                                width: themeVolume.availableWidth; height: 5; radius: 2.5; color: "#354b5d"
                                Rectangle { width: parent.width * themeVolume.visualPosition; height: parent.height; radius: parent.radius; color: window.accent }
                            }
                            handle: Rectangle {
                                x: themeVolume.leftPadding + themeVolume.visualPosition * (themeVolume.availableWidth - width)
                                y: themeVolume.topPadding + (themeVolume.availableHeight - height) / 2
                                width: 18; height: 18; radius: 9; color: window.accent
                                border.width: themeVolume.activeFocus ? 3 : 0; border.color: "white"
                            }
                        }
                    }
                    NavButton { id: fullscreenSetting; Layout.fillWidth: true; text: "Fullscreen"; value: Preferences.fullscreen ? "On" : "Off"; flat: true; onActiveFocusChanged: if (activeFocus) options.reveal(this); onClicked: { Preferences.fullscreen = !Preferences.fullscreen; window.visibility = Preferences.fullscreen ? Window.FullScreen : Window.Windowed } }
                    NavButton { Layout.fillWidth: true; id: uiScaleSetting; objectName: "uiScaleSetting"; text: "UI scale"; value: Math.round(Preferences.uiScale * 100) + "%"; flat: true; onActiveFocusChanged: if (activeFocus) options.reveal(this); Keys.onLeftPressed: Preferences.uiScale = Math.max(0.75, Math.round((Preferences.uiScale - 0.05) * 100) / 100); Keys.onRightPressed: Preferences.uiScale = Math.min(1.5, Math.round((Preferences.uiScale + 0.05) * 100) / 100); onClicked: Preferences.uiScale = Preferences.uiScale >= 1.49 ? 0.75 : Math.round((Preferences.uiScale + 0.05) * 100) / 100 }
                    NavButton { objectName: "accentSetting"; Layout.fillWidth: true; text: "Accent"; value: ({"#66c0f4": "Steam blue", "#76dfba": "Sea glass", "#f3bb70": "Warm amber"})[Preferences.accent]; flat: true; onActiveFocusChanged: if (activeFocus) options.reveal(this); onClicked: { let colors = ["#66c0f4", "#76dfba", "#f3bb70"]; Preferences.accent = colors[(colors.indexOf(Preferences.accent) + 1) % 3] } }
                    Text { Layout.fillWidth: true; text: "Streaming changes apply to the next video."; color: "#8eacbf"; font.pixelSize: 12; wrapMode: Text.WordWrap; topPadding: 8 }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                NavButton { text: "Sign out"; value: App.username; visible: App.signedIn; Layout.maximumWidth: 360; onClicked: { App.logout(); options.close() } }
                Item { Layout.fillWidth: true }
                NavButton { text: "Exit"; flat: true; onClicked: { options.close(); exitPrompt.open() } }
            }
        }
    }
    Popup {
        id: exitPrompt
        objectName: "exitPrompt"
        parent: Overlay.overlay
        width: Math.min(440, stage.width - 48); height: 180
        x: (stage.width - width) / 2; y: (stage.height - height) / 2
        modal: true; focus: true; padding: 24; closePolicy: Popup.CloseOnEscape
        onOpened: cancelExit.forceActiveFocus()
        onClosed: if (App.signedIn) window.focusContent(); else serverField.forceActiveFocus()
        background: Rectangle { color: "#15202d"; radius: 8; border.color: "#425263" }
        contentItem: ColumnLayout {
            spacing: 24
            Text { text: "Exit JF Gamescope?"; color: "white"; font.pixelSize: 24; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                NavButton { id: cancelExit; objectName: "cancelExit"; text: "Cancel"; Layout.fillWidth: true; onClicked: exitPrompt.close() }
                NavButton { text: "Exit"; Layout.fillWidth: true; primary: true; accent: window.accent; onClicked: window.close() }
            }
        }
    }
    Popup {
        id: notification
        parent: Overlay.overlay; width: Math.min(700, stage.width - 50); height: messageColumn.implicitHeight + 48; x: (stage.width - width) / 2; y: 30; modal: true; focus: true; padding: 24
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
    Component.onCompleted: if (!App.authenticating) serverField.forceActiveFocus()
}
