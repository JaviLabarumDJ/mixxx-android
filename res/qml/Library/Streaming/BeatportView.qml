import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "../../Theme"
import "../.." as Skin

Item {
    id: root

    property bool showTokenSettings: false
    property var tracksModel: []
    property string statusMessage: ""
    property bool isBusy: false
    property var downloadProgressMap: ({})

    Connections {
        target: Mixxx.StreamingManager

        function onBeatportSearchFinished(success, tracks, errorString) {
            root.isBusy = false;
            if (success) {
                root.tracksModel = tracks;
                root.statusMessage = qsTr("%1 tracks found on Beatport").arg(tracks.length);
            } else {
                root.statusMessage = qsTr("❌ Error: %1").arg(errorString);
            }
        }

        function onBeatportTopTracksFinished(success, tracks, errorString) {
            root.isBusy = false;
            if (success) {
                root.tracksModel = tracks;
                root.statusMessage = qsTr("%1 electronic chart tracks loaded").arg(tracks.length);
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
        Mixxx.StreamingManager.getBeatportTopTracks();
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ================= TOP HEADER & BRAND =================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: "#121d17"
            border.color: "#1d382b"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Text {
                    text: qsTr("⚡ BEATPORT")
                    font.bold: true
                    font.pixelSize: 13
                    font.letterSpacing: 1
                    color: "#00ff88"
                }

                Rectangle {
                    Layout.preferredHeight: 22
                    Layout.preferredWidth: liveText.implicitWidth + 14
                    radius: 11
                    color: "#00ff8822"
                    border.color: "#00ff88"
                    border.width: 1

                    Text {
                        id: liveText
                        anchors.centerIn: parent
                        font.pixelSize: 10
                        font.bold: true
                        color: "#00ff88"
                        text: qsTr("● Electronic DJ Top 100")
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    visible: root.statusMessage !== ""
                    text: root.statusMessage
                    font.pixelSize: 11
                    color: "#b0bec5"
                    elide: Text.ElideRight
                    Layout.maximumWidth: 260
                }

                Skin.Button {
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 90
                    activeColor: "#00ff88"
                    backgroundColor: root.showTokenSettings ? "#00e676" : "#1a382b"
                    normalColor: root.showTokenSettings ? "#000000" : "#ffffff"
                    font.bold: true
                    font.pixelSize: 11
                    text: qsTr("🔑 Token")
                    onClicked: root.showTokenSettings = !root.showTokenSettings
                }

                Skin.Button {
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 100
                    activeColor: "#00ff88"
                    backgroundColor: "#004d40"
                    normalColor: "#ffffff"
                    font.bold: true
                    font.pixelSize: 11
                    text: root.isBusy ? qsTr("🔄 ...") : qsTr("⚡ Top 100")
                    enabled: !root.isBusy
                    onClicked: {
                        root.isBusy = true;
                        Mixxx.StreamingManager.getBeatportTopTracks();
                    }
                }
            }
        }

        // ================= TOKEN SETTINGS DRAWER =================
        Rectangle {
            visible: root.showTokenSettings
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: "#0d1a13"
            border.color: "#183323"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8

                Text {
                    text: qsTr("Bearer Token:")
                    font.pixelSize: 11
                    font.bold: true
                    color: "#00ff88"
                }

                TextField {
                    id: tokenInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    placeholderText: qsTr("Paste Beatport streaming Bearer token (optional for full downloads)...")
                    color: "#ffffff"
                    font.pixelSize: 12
                    text: Mixxx.StreamingManager ? Mixxx.StreamingManager.beatportToken : ""
                    background: Rectangle {
                        color: "#15261d"
                        border.color: "#224a35"
                        border.width: 1
                        radius: 4
                    }
                    onEditingFinished: {
                        if (Mixxx.StreamingManager) {
                            Mixxx.StreamingManager.beatportToken = text.trim();
                        }
                    }
                }

                Skin.Button {
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 70
                    activeColor: "#00ff88"
                    backgroundColor: "#00695c"
                    normalColor: "#ffffff"
                    font.bold: true
                    font.pixelSize: 11
                    text: qsTr("Save")
                    onClicked: {
                        if (Mixxx.StreamingManager) {
                            Mixxx.StreamingManager.beatportToken = tokenInput.text.trim();
                            root.showTokenSettings = false;
                        }
                    }
                }
            }
        }

        // ================= SEARCH BAR =================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: "#0f1713"
            border.color: "#1a2b22"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 5
                spacing: 6

                TextField {
                    id: searchInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    placeholderText: qsTr("Search Beatport releases, tracks, remixers (e.g. Fisher, CamelPhat, Tale of Us)...")
                    color: "#ffffff"
                    font.pixelSize: 13
                    background: Rectangle {
                        color: "#16241d"
                        border.color: searchInput.activeFocus ? "#00ff88" : "#223d2e"
                        border.width: 1
                        radius: 4
                    }
                    onAccepted: {
                        if (text.trim() !== "") {
                            root.isBusy = true;
                            root.statusMessage = qsTr("Searching Beatport...");
                            Mixxx.StreamingManager.searchBeatport(text);
                        }
                    }
                }

                Skin.Button {
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: 80
                    activeColor: "#00ff88"
                    backgroundColor: "#00796b"
                    normalColor: "#ffffff"
                    font.bold: true
                    font.pixelSize: 11
                    text: qsTr("Search")
                    enabled: !root.isBusy
                    onClicked: {
                        if (searchInput.text.trim() !== "") {
                            root.isBusy = true;
                            root.statusMessage = qsTr("Searching Beatport...");
                            Mixxx.StreamingManager.searchBeatport(searchInput.text);
                        }
                    }
                }
            }
        }

        // ================= TRACKS LIST =================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0b120e"

            ListView {
                id: tracksList
                anchors.fill: parent
                clip: true
                spacing: 2
                model: root.tracksModel

                ScrollBar.vertical: ScrollBar {
                    id: bpScroll
                    width: 14
                    policy: ScrollBar.AsNeeded
                    contentItem: Rectangle {
                        implicitWidth: 10
                        implicitHeight: 40
                        radius: 5
                        color: bpScroll.pressed ? "#00ff88" : "#1e4732"
                    }
                }

                delegate: Rectangle {
                    id: trackDelegate
                    width: tracksList.width
                    height: 58
                    color: index % 2 === 0 ? "#111c16" : "#0d1712"
                    border.color: "#1a2e23"
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
                            color: "#193325"
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
                                text: "⚡"
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
                                    text: track.artist + (track.album ? (" • " + track.album) : "")
                                    font.pixelSize: 11
                                    color: "#00ff88"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        // Key (Camelot) Badge
                        Rectangle {
                            visible: track.key !== undefined && track.key !== "" && track.key !== "0"
                            Layout.preferredHeight: 18
                            Layout.preferredWidth: keyText.implicitWidth + 8
                            color: "#004d40"
                            radius: 3

                            Text {
                                id: keyText
                                anchors.centerIn: parent
                                text: track.key || ""
                                font.pixelSize: 9
                                font.bold: true
                                color: "#a7ffeb"
                            }
                        }

                        // BPM Badge
                        Rectangle {
                            visible: track.bpm !== undefined && track.bpm > 0
                            Layout.preferredHeight: 18
                            Layout.preferredWidth: bpmText.implicitWidth + 8
                            color: "#133b27"
                            radius: 3

                            Text {
                                id: bpmText
                                anchors.centerIn: parent
                                text: Math.round(track.bpm) + " BPM"
                                font.pixelSize: 9
                                font.bold: true
                                color: "#00ff88"
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
                            color: track.isCached ? "#00e67622" : "#00ff8822"
                            border.color: track.isCached ? "#00e676" : "#00ff88"
                            border.width: 1

                            Text {
                                id: cacheStatusText
                                anchors.centerIn: parent
                                font.pixelSize: 9
                                font.bold: true
                                color: track.isCached ? "#00e676" : "#00ff88"
                                text: track.isCached ? qsTr("💾 Ready") : qsTr("⚡ Stream")
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
                                    track.streamUrl, "beatport", track.id, track.title, track.artist, "[Channel1]", false
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
                                    track.streamUrl, "beatport", track.id, track.title, track.artist, "[Channel2]", false
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
                        color: "#00ff88"
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
                            text: "⚡"
                            font.pixelSize: 32
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("No Beatport tracks loaded yet")
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: 14
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Search for electronic releases or click Top 100")
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
                        text: qsTr("Connecting to Beatport...")
                        color: "#00ff88"
                        font.bold: true
                        font.pixelSize: 13
                    }
                }
            }
        }
    }
}

