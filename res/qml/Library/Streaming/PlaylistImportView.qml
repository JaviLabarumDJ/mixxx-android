import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "../../Theme"
import "../.." as Skin

Item {
    id: root

    property string importedTitle: ""
    property string importedService: "spotify"
    property var tracksModel: []
    property string statusMessage: ""
    property bool isBusy: false
    property var downloadProgressMap: ({})

    Connections {
        target: Mixxx.StreamingManager

        function onPlaylistImportFinished(success, service, playlistTitle, tracks, errorString) {
            root.isBusy = false;
            if (success) {
                root.importedService = service;
                root.importedTitle = playlistTitle;
                root.tracksModel = tracks;
                root.statusMessage = qsTr("✔ Imported '%1' (%2 tracks)").arg(playlistTitle).arg(tracks.length);
            } else {
                root.statusMessage = qsTr("❌ Error: %1").arg(errorString);
            }
        }

        function onDownloadProgress(trackId, progress, bytesReceived, bytesTotal) {
            var updated = Object.assign({}, root.downloadProgressMap);
            updated[trackId] = progress;
            root.downloadProgressMap = updated;
        }

        function onDownloadFinished(trackId, localPath, targetDeck, autoPlay) {
            var updated = Object.assign({}, root.downloadProgressMap);
            delete updated[trackId];
            root.downloadProgressMap = updated;

            var currentTracks = root.tracksModel.slice();
            for (var i = 0; i < currentTracks.length; i++) {
                if (currentTracks[i].id === trackId) {
                    currentTracks[i].isCached = true;
                    break;
                }
            }
            root.tracksModel = currentTracks;
            root.statusMessage = qsTr("✔ Track ready and loaded into %1").arg(targetDeck === "[Channel1]" ? qsTr("Deck 1") : (targetDeck === "[Channel2]" ? qsTr("Deck 2") : qsTr("cache")));
        }

        function onDownloadFailed(trackId, errorMessage) {
            var updated = Object.assign({}, root.downloadProgressMap);
            delete updated[trackId];
            root.downloadProgressMap = updated;
            root.statusMessage = qsTr("❌ %1").arg(errorMessage);
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ================= TOP HEADER & BRAND =================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: "#121d15"
            border.color: "#1d3826"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Text {
                    text: qsTr("🟢 SPOTIFY & APPLE MUSIC")
                    font.bold: true
                    font.pixelSize: 13
                    font.letterSpacing: 1
                    color: "#1db954"
                }

                Rectangle {
                    Layout.preferredHeight: 22
                    Layout.preferredWidth: liveText.implicitWidth + 14
                    radius: 11
                    color: "#1db95422"
                    border.color: "#1db954"
                    border.width: 1

                    Text {
                        id: liveText
                        anchors.centerIn: parent
                        font.pixelSize: 10
                        font.bold: true
                        color: "#1db954"
                        text: qsTr("● Public Playlist Importer")
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    visible: root.statusMessage !== ""
                    text: root.statusMessage
                    font.pixelSize: 11
                    color: "#b0bec5"
                    elide: Text.ElideRight
                    Layout.maximumWidth: 320
                }
            }
        }

        // ================= LINK INPUT & IMPORT BAR =================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "#0f1712"
            border.color: "#192c20"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8

                TextField {
                    id: linkInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    placeholderText: qsTr("Paste Spotify or Apple Music link (e.g. https://open.spotify.com/playlist/... or https://music.apple.com/...)")
                    color: "#ffffff"
                    font.pixelSize: 12
                    background: Rectangle {
                        color: "#15241b"
                        border.color: linkInput.activeFocus ? "#1db954" : "#213d2b"
                        border.width: 1
                        radius: 4
                    }
                    onAccepted: {
                        if (text.trim() !== "") {
                            root.isBusy = true;
                            root.statusMessage = qsTr("Importing playlist...");
                            Mixxx.StreamingManager.importPlaylistLink(text);
                        }
                    }
                }

                Skin.Button {
                    Layout.preferredHeight: 38
                    Layout.preferredWidth: 110
                    activeColor: "#1ed760"
                    backgroundColor: "#1db954"
                    normalColor: "#ffffff"
                    font.bold: true
                    font.pixelSize: 12
                    text: root.isBusy ? qsTr("Loading...") : qsTr("📥 Import")
                    enabled: !root.isBusy && linkInput.text.trim() !== ""
                    onClicked: {
                        root.isBusy = true;
                        root.statusMessage = qsTr("Importing playlist...");
                        Mixxx.StreamingManager.importPlaylistLink(linkInput.text);
                    }
                }
            }
        }

        // ================= IMPORTED PLAYLIST TITLE BAR =================
        Rectangle {
            visible: root.importedTitle !== ""
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: "#132118"
            border.color: "#1f3828"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: root.importedService === "spotify" ? "🟢" : "🍎"
                    font.pixelSize: 14
                }

                Text {
                    text: root.importedTitle
                    font.bold: true
                    font.pixelSize: 12
                    color: "#ffffff"
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    text: qsTr("%1 tracks").arg(root.tracksModel.length)
                    font.pixelSize: 11
                    color: "#a5d6a7"
                }
            }
        }

        // ================= TRACKS LIST =================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0b120d"

            ListView {
                id: tracksList
                anchors.fill: parent
                clip: true
                spacing: 2
                model: root.tracksModel

                ScrollBar.vertical: ScrollBar {
                    id: spScroll
                    width: 14
                    policy: ScrollBar.AsNeeded
                    contentItem: Rectangle {
                        implicitWidth: 10
                        implicitHeight: 40
                        radius: 5
                        color: spScroll.pressed ? "#1db954" : "#1e472a"
                    }
                }

                delegate: Rectangle {
                    id: trackDelegate
                    width: tracksList.width
                    height: 58
                    color: index % 2 === 0 ? "#111c14" : "#0d1710"
                    border.color: "#1a2e20"
                    border.width: 1

                    readonly property var track: modelData
                    readonly property real downloadProg: root.downloadProgressMap[track.id] !== undefined ? root.downloadProgressMap[track.id] : -1
                    readonly property bool isDownloading: downloadProg >= 0

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        Rectangle {
                            Layout.preferredWidth: 42
                            Layout.preferredHeight: 42
                            radius: 4
                            color: "#17291e"
                            clip: true

                            Image {
                                anchors.fill: parent
                                source: track.coverArtUrl ? track.coverArtUrl : ""
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                visible: status === Image.Ready
                            }

                            Text {
                                anchors.centerIn: parent
                                text: root.importedService === "spotify" ? "🟢" : "🍎"
                                font.pixelSize: 16
                                visible: !track.coverArtUrl
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                text: track.title
                                font.bold: true
                                font.pixelSize: 13
                                color: "#ffffff"
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Text {
                                    text: track.artist
                                    font.pixelSize: 11
                                    color: "#81c784"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        Text {
                            text: {
                                var s = track.duration || 0;
                                var m = Math.floor(s / 60);
                                var sec = Math.floor(s % 60);
                                return m + ":" + (sec < 10 ? "0" : "") + sec;
                            }
                            font.pixelSize: 11
                            color: "#90a4ae"
                        }

                        Rectangle {
                            Layout.preferredHeight: 20
                            Layout.preferredWidth: cacheStatusText.implicitWidth + 10
                            radius: 10
                            color: track.isCached ? "#00e67622" : "#1db95422"
                            border.color: track.isCached ? "#00e676" : "#1db954"
                            border.width: 1

                            Text {
                                id: cacheStatusText
                                anchors.centerIn: parent
                                font.pixelSize: 9
                                font.bold: true
                                color: track.isCached ? "#00e676" : "#1db954"
                                text: track.isCached ? qsTr("💾 Ready") : qsTr("🟢 Full Track")
                            }
                        }

                        Skin.Button {
                            Layout.preferredHeight: 34
                            Layout.preferredWidth: 80
                            activeColor: "#64b5f6"
                            backgroundColor: "#1565c0"
                            normalColor: "#ffffff"
                            font.bold: true
                            font.pixelSize: 11
                            arrowLeft: true
                            text: "D1"
                            enabled: !trackDelegate.isDownloading
                            onClicked: {
                                Mixxx.StreamingManager.downloadAndLoad(
                                    track.streamUrl, root.importedService, track.id, track.title, track.artist, "[Channel1]", false
                                );
                            }
                        }

                        Skin.Button {
                            Layout.preferredHeight: 34
                            Layout.preferredWidth: 80
                            activeColor: "#ffb74d"
                            backgroundColor: "#e65100"
                            normalColor: "#ffffff"
                            font.bold: true
                            font.pixelSize: 11
                            arrowRight: true
                            text: "D2"
                            enabled: !trackDelegate.isDownloading
                            onClicked: {
                                Mixxx.StreamingManager.downloadAndLoad(
                                    track.streamUrl, root.importedService, track.id, track.title, track.artist, "[Channel2]", false
                                );
                            }
                        }
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        height: 3
                        visible: trackDelegate.isDownloading
                        width: parent.width * Math.max(0.05, trackDelegate.downloadProg)
                        color: "#1db954"
                    }
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * 0.7
                    height: 120
                    color: "transparent"
                    visible: tracksList.count === 0 && !root.isBusy

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 8

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "📥"
                            font.pixelSize: 32
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("No playlist imported yet")
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: 14
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Paste any Spotify or Apple Music playlist/album URL above")
                            color: "#78909c"
                            font.pixelSize: 12
                        }
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                color: "#aa000000"
                visible: root.isBusy

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 12

                    BusyIndicator {
                        Layout.alignment: Qt.AlignHCenter
                        running: root.isBusy
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Parsing playlist from provider...")
                        color: "#1db954"
                        font.bold: true
                        font.pixelSize: 13
                    }
                }
            }
        }
    }
}

