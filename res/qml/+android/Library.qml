import "." as Skin
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls 2.15
import QtQuick.Shapes 1.6
import "Theme"
import "Library" as LibraryComponent
import "Library/Streaming" as StreamingComponent

Item {
    id: root

    property var sidebar: librarySources.sidebar()
    property int librarySourceMode: 0 // 0: Local, 1: Streaming Hub
    // Remembered across restarts, see Mixxx.MobileLibrary
    readonly property bool showSidebar: Mixxx.MobileLibrary.sidebarVisible

    function applySearch() {
        if (root.sidebar && root.sidebar.tracklist && root.librarySourceMode === 0) {
            root.sidebar.tracklist.search(searchField.text);
        }
    }
    function focusSearch() {
        if (root.librarySourceMode === 0) {
            root.applySearch();
            searchField.forceActiveFocus(Qt.ShortcutFocusReason);
            searchField.selectAll();
        }
    }

    LibraryComponent.SourceTree {
        id: librarySources
    }

    // The controller's browse knob moves through the sidebar or the tracks,
    // pressing it switches between them and opens playlists and folders
    LibraryComponent.Control {
        id: libraryControl

        // Only the library view that is on screen follows the controller
        enabled: root.visible

        onFocusWidgetChanged: {
            if (!root.visible) {
                return;
            }
            switch (focusWidget) {
            case Skin.FocusedWidgetControl.WidgetKind.LibraryView:
                trackList.focusList();
                break;
            case Skin.FocusedWidgetControl.WidgetKind.Sidebar:
                root.librarySourceMode = 0;
                Mixxx.MobileLibrary.sidebarVisible = true;
                if (browser.currentRow() < 0) {
                    browser.selectRow(0);
                }
                break;
            }
        }
        onActivateSidebarItem: {
            if (browser.activateCurrent()) {
                focusWidget = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
            }
        }
        onCollapseSidebarItem: browser.collapseCurrent()
        onLoadSelectedTrack: (group, play) => {
            trackList.loadSelectedTrack(group, play);
        }
        onLoadSelectedTrackIntoNextAvailableDeck: play => {
            trackList.loadSelectedTrackIntoNextAvailableDeck(play);
        }
        onMoveSidebar: offset => {
            browser.moveSelection(offset);
        }
        onMoveVertical: offset => {
            trackList.moveSelectionVertical(offset);
        }
    }

    Connections {
        target: root.sidebar

        // Playlists, crates and folders bring their own track list
        function onTracklistChanged() {
            if (searchField.text.length > 0) {
                root.applySearch();
            }
        }
    }

    // ================= SOURCE SELECTOR TAB BAR =================
    Rectangle {
        id: sourceSelectorBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 38
        color: "#181b22"
        border.color: "#282d3b"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 3
            spacing: 6

            Skin.Button {
                Layout.fillHeight: true
                Layout.preferredWidth: 120
                activeColor: "#00e5ff"
                backgroundColor: root.librarySourceMode === 0 ? "#1e3a47" : "#222733"
                normalColor: root.librarySourceMode === 0 ? "#00e5ff" : "#90a4ae"
                font.bold: true
                font.pixelSize: 11
                text: qsTr("LOCAL MUSIC")
                onClicked: root.librarySourceMode = 0
            }

            Skin.Button {
                Layout.fillHeight: true
                Layout.preferredWidth: 140
                activeColor: "#00ffff"
                backgroundColor: root.librarySourceMode === 1 ? "#004d40" : "#222733"
                normalColor: root.librarySourceMode === 1 ? "#00ffff" : "#80deea"
                font.bold: true
                font.pixelSize: 11
                text: "STREAMING HUB"
                onClicked: root.librarySourceMode = 1
            }

            Item { Layout.fillWidth: true }

            // Shows or hides the sidebar (playlists, crates, history...).
            // A toggle rather than a tab: always the same label, lit while
            // the sidebar is shown.
            Skin.Button {
                visible: root.librarySourceMode === 0
                Layout.fillHeight: true
                Layout.preferredWidth: 130
                activeColor: "#ffffff"
                normalColor: "#90a4ae"
                highlight: root.showSidebar
                font.bold: true
                font.pixelSize: 10
                text: qsTr("SIDEBAR")
                onClicked: Mixxx.MobileLibrary.sidebarVisible = !Mixxx.MobileLibrary.sidebarVisible
            }
        }
    }

    // ================= CONTENT CONTAINER =================
    Item {
        id: sourceContentContainer
        anchors.top: sourceSelectorBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        // 1. LOCAL LIBRARY
        Item {
            id: localLibraryArea
            anchors.fill: parent
            visible: root.librarySourceMode === 0

            TextField {
                id: searchField
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                placeholderText: qsTr("Search your local collection...")
                onTextChanged: root.applySearch()
            }

            SplitView {
                id: librarySplitView
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: searchField.bottom
                orientation: Qt.Horizontal

                handle: Rectangle {
                    id: handleDelegate
                    property color handleColor: SplitHandle.pressed || SplitHandle.hovered ? Theme.panelSplitterHandleActive : Theme.panelSplitterHandle
                    property int handleSize: SplitHandle.pressed || SplitHandle.hovered ? 6 : 5
                    clip: true
                    color: Theme.panelSplitterBackground
                    implicitHeight: 8
                    implicitWidth: 8

                    containmentMask: Item {
                        height: librarySplitView.height
                        width: 8
                        x: (handleDelegate.width - width) / 2
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        Repeater {
                            model: 3
                            Rectangle {
                                color: handleColor
                                height: handleSize
                                radius: handleSize
                                width: handleSize
                            }
                        }
                    }
                }

                SplitView {
                    id: sideBarSplitView
                    visible: root.showSidebar
                    SplitView.maximumWidth: 400
                    SplitView.minimumWidth: root.showSidebar ? 140 : 0
                    SplitView.preferredWidth: root.showSidebar ? Math.min(220, root.width * 0.25) : 0
                    orientation: Qt.Vertical

                    handle: Rectangle {
                        id: handleDelegateVertical
                        property color handleColor: SplitHandle.pressed || SplitHandle.hovered ? Theme.panelSplitterHandleActive : Theme.panelSplitterHandle
                        property int handleSize: SplitHandle.pressed || SplitHandle.hovered ? 6 : 5
                        clip: true
                        color: Theme.panelSplitterBackground
                        implicitHeight: 8
                        implicitWidth: 8

                        containmentMask: Item {
                            height: 8
                            width: sideBarSplitView.width
                            x: (handleDelegateVertical.width - width) / 2
                        }

                        RowLayout {
                            anchors.centerIn: parent
                            Repeater {
                                model: 3
                                Rectangle {
                                    color: handleColor
                                    height: handleSize
                                    radius: handleSize
                                    width: handleSize
                                }
                            }
                        }
                    }

                    LibraryComponent.Browser {
                        id: browser

                        onStorageAccessNeeded: storageAccessPopup.open()

                        highlighted: libraryControl.sidebarFocused
                        SplitView.fillHeight: true
                        SplitView.minimumHeight: 100
                        SplitView.preferredHeight: 500
                        model: root.sidebar
                    }
                }

                LibraryComponent.TrackList {
                    id: trackList

                    highlighted: libraryControl.sidebarFocused === false && root.showSidebar
                    SplitView.fillHeight: true
                    clip: true
                    model: root.sidebar ? root.sidebar.tracklist : null
                }
            }
        }

        // 2. MULTI-SERVICE STREAMING HUB (TIDAL, SOUNDCLOUD, BEATPORT, NAVIDROME, AUDIUS, URL)
        StreamingComponent.StreamingHub {
            id: streamingHubArea
            anchors.fill: parent
            visible: root.librarySourceMode === 1
        }
    }

    Popup {
        id: storageAccessPopup

        anchors.centerIn: parent
        width: Math.min(460, root.width - 20)
        modal: true
        padding: 16

        background: Rectangle {
            color: Theme.darkGray2
            border.color: Theme.darkGray3
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                Layout.fillWidth: true
                color: Theme.white
                font.bold: true
                font.pixelSize: 15
                text: qsTr("Permission to read USB")
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textColor
                font.pixelSize: 13
                text: qsTr("To read Rekordbox and Engine DJ playlists from a USB drive, Android has to grant Mixxx access to all files. Enable it on the screen that opens, then tap the section again.")
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8

                Skin.Button {
                    Layout.preferredHeight: 44
                    Layout.preferredWidth: 110
                    text: qsTr("NOT NOW")
                    onClicked: storageAccessPopup.close()
                }
                Skin.Button {
                    Layout.preferredHeight: 44
                    Layout.preferredWidth: 150
                    activeColor: Theme.accentColor
                    backgroundColor: "#1565c0"
                    normalColor: "#ffffff"
                    font.bold: true
                    text: qsTr("GRANT PERMISSION")
                    onClicked: {
                        storageAccessPopup.close();
                        Mixxx.MobileLibrary.requestFullStorageAccess();
                    }
                }
            }
        }
    }
}
