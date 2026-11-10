import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "../../Theme"
import "../.." as Skin

Item {
    id: root

    property string currentTag: "Trending"
    property var tracksModel: []
    property string statusMessage: ""
    property bool isBusy: false
    property var downloadProgressMap: ({})

    Connections {
        target: Mixxx.StreamingManager

        function onSoundCloudSearchFinished(success, tracks, errorString) {
            root.isBusy = false;
            if (success) {
                root.tracksModel = tracks;
                root.statusMessage = qsTr("%1 tracks found on SoundCloud").arg(tracks.length);
            } else {
                root.statusMessage = qsTr("❌ Error: %1").arg(errorString);
            }
        }

        function onSoundCloudTrendingFinished(success, tracks, errorString) {
            root.isBusy = false;
            if (success) {
                root.tracksModel = tracks;
                root.statusMessage = qsTr("%1 trending tracks on SoundCloud").arg(tracks.length);
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
            root.statusMessage = qsTr("❌ Download error: %1").arg(errorMessage);
        }
    }

    Component.onCompleted: {
        root.isBusy = true;
        Mixxx.StreamingManager.getSoundCloudTrending();
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ================= TOP HEADER & BRAND =================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: "#1c1714"
            border.color: "#3d2215"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Text {
                    text: qsTr("☁️ SOUNDCLOUD")
                    font.bold: true
                    font.pixelSize: 13
                    font.letterSpacing: 1
                    color: "#ff5500"
                }

                Rectangle {
                    Layout.preferredHeight: 22
                    Layout.preferredWidth: liveText.implicitWidth + 14
                    radius: 11
                    color: "#ff550022"
                    border.color: "#ff5500"
                    border.width: 1

                    Text {
                        id: liveText
                        anchors.centerIn: parent
                        font.pixelSize: 10
                        font.bold: true
                        color: "#ff7733"
                        text: qsTr("● High Quality Streams")
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

                Skin.Button {
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 120
                    activeColor: "#ff5500"
                    backgroundColor: "#331c12"
                    normalColor: "#ffffff"
                    font.bold: true
                    font.pixelSize: 11
                    text: root.isBusy ? qsTr("🔄 Loading...") : qsTr("🔥 Trending")
                    enabled: !root.isBusy
                    onClicked: {
                        root.isBusy = true;
                        Mixxx.StreamingManager.getSoundCloudTrending();
                    }
                }
            }
        }

        // ================= SEARCH & QUICK TAG PILLS =================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: "#181412"
            border.color: "#281e18"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 5
                spacing: 6

                TextField {
                    id: searchInput
                    Layout.preferredWidth: 260
                    Layout.preferredHeight: 34
                    placeholderText: qsTr("Search tracks, DJ sets, remixes...")
                    color: "#ffffff"
                    font.pixelSize: 13
                    background: Rectangle {
                        color: "#241a14"
                        border.color: searchInput.activeFocus ? "#ff5500" : "#3d2a20"
                        border.width: 1
                        radius: 4
                    }
                    onAccepted: {
                        if (text.trim() !== "") {
                            root.isBusy = true;
                            root.statusMessage = qsTr("Searching SoundCloud...");
                            Mixxx.StreamingManager.searchSoundCloud(text);
                        }
                    }
                }

                Skin.Button {
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: 80
                    activeColor: "#ff7733"
                    backgroundColor: "#e64a19"
                    normalColor: "#ffffff"
                    font.bold: true
                    font.pixelSize: 11
                    text: qsTr("Search")
                    enabled: !root.isBusy
                    onClicked: {
                        if (searchInput.text.trim() !== "") {
                            root.isBusy = true;
                            root.statusMessage = qsTr("Searching SoundCloud...");
                            Mixxx.StreamingManager.searchSoundCloud(searchInput.text);
                        }
                    }
                }

                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: genreRow.width
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    RowLayout {
                        id: genreRow
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        Repeater {
                            model: [
                                "House", "Techno", "Drum & Bass", "EDM", "Deep House", "Remix", "Mashup", "Acapella"
                            ]

                            delegate: Rectangle {
                                Layout.preferredHeight: 28
                                Layout.preferredWidth: genreLabel.implicitWidth + 16
                                radius: 14
                                color: root.currentTag === modelData ? "#ff5500" : "#241b16"
                                border.color: root.currentTag === modelData ? "#ff7733" : "#3d2a20"
                                border.width: 1

                                Text {
                                    id: genreLabel
                                    anchors.centerIn: parent
                                    text: modelData
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: root.currentTag === modelData ? "#ffffff" : "#ffccbc"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        root.currentTag = modelData;
                                        root.isBusy = true;
                                        root.statusMessage = qsTr("Searching %1...").arg(modelData);
                                        searchInput.text = modelData;
                                        Mixxx.StreamingManager.searchSoundCloud(modelData);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ================= TRACKS LIST =================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#110e0c"

            ListView {
                id: tracksList
                anchors.fill: parent
                clip: true
                spacing: 2
                model: root.tracksModel

                ScrollBar.vertical: ScrollBar {
                    id: scScroll
                    width: 14
                    policy: ScrollBar.AsNeeded
                    contentItem: Rectangle {
                        implicitWidth: 10
                        implicitHeight: 40
                        radius: 5
                        color: scScroll.pressed ? "#ff5500" : "#4a3528"
                    }
                }

                delegate: Rectangle {
                    id: trackDelegate
                    width: tracksList.width
                    height: 58
                    color: index % 2 === 0 ? "#17120f" : "#130f0c"
                    border.color: "#281d16"
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
                            color: "#2c1c14"
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
                                text: "☁️"
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
                                    color: "#ff8a65"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        Rectangle {
                            visible: track.genre !== undefined && track.genre !== ""
                            Layout.preferredHeight: 18
                            Layout.preferredWidth: genreText.implicitWidth + 8
                            color: "#331c12"
                            radius: 3

                            Text {
                                id: genreText
                                anchors.centerIn: parent
                                text: track.genre || ""
                                font.pixelSize: 9
                                font.bold: true
                                color: "#ffab91"
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
                            color: track.isCached ? "#00e67622" : "#ff550022"
                            border.color: track.isCached ? "#00e676" : "#ff5500"
                            border.width: 1

                            Text {
                                id: cacheStatusText
                                anchors.centerIn: parent
                                font.pixelSize: 9
                                font.bold: true
                                color: track.isCached ? "#00e676" : "#ff5500"
                                text: track.isCached ? qsTr("💾 Ready") : qsTr("☁️ Stream")
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
                                    track.streamUrl, "soundcloud", track.id, track.title, track.artist, "[Channel1]", false
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
                                    track.streamUrl, "soundcloud", track.id, track.title, track.artist, "[Channel2]", false
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
                        color: "#ff5500"
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
                            text: "☁️"
                            font.pixelSize: 32
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("No SoundCloud tracks loaded yet")
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: 14
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Search for any DJ set, track or mashup above")
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
                        text: qsTr("Connecting to SoundCloud...")
                        color: "#ff7733"
                        font.bold: true
                        font.pixelSize: 13
                    }
                }
            }
        }
    }
}

