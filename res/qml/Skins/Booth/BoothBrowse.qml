import "../.." as Skin
import "../../Library" as LibraryComponent
import "../../Library/Streaming" as StreamingComponent
import "." as Booth
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls

// BROWSE of the booth skin, laid out like the browser of a player: the
// sources on the left, the tracks in the middle and the selected track on
// the right. The data, the track menu, + ALL, COPY and the controller
// browsing are the ones of the shared library (Library/), only the columns
// and the look are the skin's (BoothSourceTree.qml).
//
// LOAD 1/2 open it with a target deck (loadTarget): a double tap loads the
// track into that deck.
Item {
    id: root

    // The deck LOAD 1/2 opened the browser for, or ""
    property string loadTarget: ""
    property bool acceptsController: true
    property bool compact: false
    property bool streaming: false
    readonly property var sidebar: sources.sidebar()

    // Filled by the cells of the selected row
    property url cover: ""
    property string title: ""
    property string artist: ""
    property string bpm: ""
    property string key: ""
    property string added: ""

    function loadSelected(tableView) {
        if (root.loadTarget.length > 0) {
            tableView.loadSelectedTrack(root.loadTarget, false);
        } else {
            tableView.loadSelectedTrackIntoNextAvailableDeck(false);
        }
    }
    function applySearch() {
        if (root.sidebar && root.sidebar.tracklist && !root.streaming) {
            root.sidebar.tracklist.search(searchField.text);
        }
    }

    Booth.BoothSourceTree {
        id: sources

        infoTarget: root
    }

    // The controller's browse knob, as in the shared library
    LibraryComponent.Control {
        id: libraryControl

        enabled: root.visible && root.acceptsController

        onActivateSidebarItem: {
            if (browser.activateCurrent()) {
                focusWidget = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
            }
        }
        onCollapseSidebarItem: browser.collapseCurrent()
        onFocusWidgetChanged: {
            if (!enabled) {
                return;
            }
            switch (focusWidget) {
            case Skin.FocusedWidgetControl.WidgetKind.LibraryView:
                trackList.focusList();
                break;
            case Skin.FocusedWidgetControl.WidgetKind.Sidebar:
                root.streaming = false;
                if (browser.currentRow() < 0) {
                    browser.selectRow(0);
                }
                break;
            }
        }
        onLoadSelectedTrack: (group, play) => {
            trackList.loadSelectedTrack(group, play);
        }
        onLoadSelectedTrackIntoNextAvailableDeck: play => {
            if (root.loadTarget.length > 0) {
                trackList.loadSelectedTrack(root.loadTarget, play);
            } else {
                trackList.loadSelectedTrackIntoNextAvailableDeck(play);
            }
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

        function onTracklistChanged() {
            root.cover = "";
            root.title = "";
            root.artist = "";
            root.bpm = "";
            root.key = "";
            root.added = "";
            if (searchField.text.length > 0) {
                root.applySearch();
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    // ---- Header: where we are, the load target, search, streaming ----
    Rectangle {
        id: header

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: "#1c1e22"
        height: root.compact ? 30 : 38

        Row {
            id: headerLeft

            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: "#d8dadd"
                font.pixelSize: root.compact ? 12 : 14
                text: root.streaming ? qsTr("STREAMING") : (browser.activeLabel.length > 0 ? browser.activeLabel : qsTr("BROWSE"))
            }
            // LOAD 1/2: the deck the tracks go to
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: root.loadTarget === "[Channel2]" ? "#e8650f" : "#1f6fd1"
                height: loadText.implicitHeight + 6
                visible: root.loadTarget.length > 0
                width: loadText.implicitWidth + 14

                Text {
                    id: loadText

                    anchors.centerIn: parent
                    color: "#ffffff"
                    font.bold: true
                    font.pixelSize: root.compact ? 11 : 12
                    text: qsTr("LOAD TO DECK %1").arg(root.loadTarget === "[Channel2]" ? 2 : 1)
                }
            }
        }
        TextField {
            id: searchField

            anchors.left: headerLeft.right
            anchors.leftMargin: 12
            anchors.right: streamingTab.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            color: "#ffffff"
            font.pixelSize: root.compact ? 12 : 14
            height: parent.height - 6
            placeholderText: qsTr("SEARCH")
            placeholderTextColor: "#6c7078"
            visible: !root.streaming

            background: Rectangle {
                border.color: searchField.activeFocus ? "#d8dadd" : "#3a3c40"
                border.width: 1
                color: "#000000"
            }

            onTextChanged: root.applySearch()
        }
        Rectangle {
            id: streamingTab

            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            border.color: root.streaming ? "#d4e157" : "#5c6068"
            border.width: 1
            color: "#000000"
            height: parent.height - 8
            width: streamingText.implicitWidth + 20

            Text {
                id: streamingText

                anchors.centerIn: parent
                color: root.streaming ? "#d4e157" : "#9a9ea6"
                font.bold: true
                font.pixelSize: root.compact ? 10 : 12
                text: qsTr("STREAMING")
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.streaming = !root.streaming
            }
        }
    }

    // ---- Sources | tracks | selected track ----
    Item {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.topMargin: 3
        visible: !root.streaming

        LibraryComponent.Browser {
            id: browser

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.top: parent.top
            highlighted: libraryControl.sidebarFocused
            model: root.sidebar
            width: root.compact ? 180 : 220

            onStorageAccessNeeded: storageAccessPopup.open()
        }
        LibraryComponent.TrackList {
            id: trackList

            anchors.bottom: parent.bottom
            anchors.left: browser.right
            anchors.leftMargin: 3
            anchors.right: info.visible ? info.left : parent.right
            anchors.rightMargin: info.visible ? 3 : 0
            anchors.top: parent.top
            clip: true
            highlighted: libraryControl.sidebarFocused === false
            model: root.sidebar ? root.sidebar.tracklist : null
            sortNewestFirst: browser.activeIsDrive
            sourceName: browser.activeLabel
        }

        // The selected track, like the info panel of a player
        Rectangle {
            id: info

            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.top: parent.top
            border.color: "#2a2c30"
            border.width: 1
            color: "#0c0d0f"
            visible: !root.compact && root.width > 900
            width: 210

            Column {
                anchors.left: parent.left
                anchors.margins: 10
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: 6

                Rectangle {
                    color: "#16181b"
                    height: width
                    width: parent.width

                    Image {
                        anchors.fill: parent
                        asynchronous: true
                        fillMode: Image.PreserveAspectCrop
                        source: root.cover
                    }
                    Text {
                        anchors.centerIn: parent
                        color: "#3a3c40"
                        font.pixelSize: 40
                        text: "♪"
                        visible: root.cover.toString().length === 0
                    }
                }
                Text {
                    color: "#ffffff"
                    font.pixelSize: 15
                    maximumLineCount: 2
                    text: root.title
                    width: parent.width
                    wrapMode: Text.Wrap
                }
                Text {
                    color: "#9a9ea6"
                    elide: Text.ElideRight
                    font.pixelSize: 13
                    text: root.artist
                    width: parent.width
                }
                Repeater {
                    model: [
                        {
                            "label": "BPM",
                            "value": root.bpm
                        },
                        {
                            "label": qsTr("KEY"),
                            "value": root.key
                        },
                        {
                            "label": qsTr("ADDED"),
                            "value": root.added
                        }
                    ]

                    Item {
                        required property var modelData

                        height: 22
                        visible: modelData.value.length > 0
                        width: parent.width

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#9a9ea6"
                            font.pixelSize: 12
                            text: parent.modelData.label
                        }
                        Text {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#ffffff"
                            font.pixelSize: 15
                            text: parent.modelData.value
                        }
                    }
                }
            }
        }
    }

    StreamingComponent.StreamingHub {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        visible: root.streaming
    }

    // Reading Rekordbox and Engine DJ drives needs access to all files
    Popup {
        id: storageAccessPopup

        anchors.centerIn: parent
        modal: true
        padding: 16
        width: Math.min(460, root.width - 20)

        background: Rectangle {
            border.color: "#5c6068"
            border.width: 1
            color: "#16181b"
        }
        contentItem: Column {
            spacing: 12

            Text {
                color: "#ffffff"
                font.bold: true
                font.pixelSize: 15
                text: qsTr("Permission to read USB")
                width: parent.width
            }
            Text {
                color: "#d8dadd"
                font.pixelSize: 13
                text: qsTr("To read Rekordbox and Engine DJ playlists from a USB drive, Android has to grant Mixxx access to all files. Enable it on the screen that opens, then tap the section again.")
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Row {
                anchors.right: parent.right
                spacing: 8

                Skin.Button {
                    height: 44
                    text: qsTr("NOT NOW")
                    width: 110

                    onClicked: storageAccessPopup.close()
                }
                Skin.Button {
                    activeColor: "#ffffff"
                    backgroundColor: "#1e5fb4"
                    font.bold: true
                    height: 44
                    normalColor: "#ffffff"
                    text: qsTr("GRANT PERMISSION")
                    width: 150

                    onClicked: {
                        storageAccessPopup.close();
                        Mixxx.MobileLibrary.requestFullStorageAccess();
                    }
                }
            }
        }
    }
}
