import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "../../Theme"
import "../.." as Skin

Item {
    id: root

    property string currentGenre: "Top"
    property var tracksModel: []
    property string statusMessage: ""
    property bool isBusy: false
    property bool showSettings: false
    property var downloadProgressMap: ({})

    Connections {
        target: Mixxx.StreamingManager

        function onDeezerSearchFinished(success, tracks, errorString) {
            root.isBusy = false;
            if (success) {
                root.tracksModel = tracks;
                root.statusMessage = qsTr("%1 tracks found on Deezer").arg(tracks.length);
            } else {
                root.statusMessage = qsTr("❌ Error: %1").arg(errorString);
            }
        }

        function onDeezerChartFinished(success, tracks, errorString) {
            root.isBusy = false;
            if (success) {
                root.tracksModel = tracks;
                root.statusMessage = qsTr("%1 top chart tracks loaded").arg(tracks.length);
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
        Mixxx.StreamingManager.getDeezerChart();
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ================= TOP HEADER & BRAND =================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: "#181424"
            border.color: "#2a1e44"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Text {
                    text: qsTr("🎧 DEEZER")
                    font.bold: true
                    font.pixelSize: 13
                    font.letterSpacing: 1
                    color: "#b388ff"
                }

                Rectangle {
                    Layout.preferredHeight: 22
                    Layout.preferredWidth: liveText.implicitWidth + 14
                    radius: 11
                    color: "#b388ff22"
                    border.color: "#b388ff"
                    border.width: 1

                    Text {
                        id: liveText
                        anchors.centerIn: parent
                        font.pixelSize: 10
                        font.bold: true
                        color: "#b388ff"
                        text: qsTr("● Global Catalog & Charts")
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
                    activeColor: "#b388ff"
                    backgroundColor: "#311b92"
                    normalColor: "#ffffff"
                    font.bold: true
                    font.pixelSize: 11
                    text: root.isBusy ? qsTr("🔄 Loading...") : qsTr("📈 Top Charts")
                    enabled: !root.isBusy
                    onClicked: {
                        root.isBusy = true;
                        Mixxx.StreamingManager.getDeezerChart();
                    }
                }

                Skin.Button {
                    id: arlSettingsBtn
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 90
                    activeColor: "#b388ff"
                    backgroundColor: root.showSettings ? "#4a148c" : "#241838"
                    normalColor: root.showSettings ? "#e1bee7" : "#b388ff"
                    font.bold: true
                    font.pixelSize: 11
                    text: root.showSettings ? qsTr("✕ Cerrar") : qsTr("⚙️ ARL")
                    onClicked: root.showSettings = !root.showSettings
                }
            }
        }

        // ================= ARL & DEEMIX SETTINGS DRAWER =================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.showSettings ? settingsCol.implicitHeight + 16 : 0
            visible: root.showSettings
            clip: true
            color: "#120e1c"
            border.color: "#311b92"
            border.width: root.showSettings ? 1 : 0

            ColumnLayout {
                id: settingsCol
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: qsTr("Cookie ARL de Deezer:")
                        font.pixelSize: 11
                        font.bold: true
                        color: "#b388ff"
                    }

                    Rectangle {
                        Layout.preferredHeight: 18
                        Layout.preferredWidth: arlBadgeText.implicitWidth + 10
                        radius: 9
                        color: (Mixxx.StreamingManager && Mixxx.StreamingManager.deezerArl.length > 20) ? "#00e67622" : "#ff525222"
                        border.color: (Mixxx.StreamingManager && Mixxx.StreamingManager.deezerArl.length > 20) ? "#00e676" : "#ff5252"
                        border.width: 1

                        Text {
                            id: arlBadgeText
                            anchors.centerIn: parent
                            font.pixelSize: 9
                            font.bold: true
                            color: (Mixxx.StreamingManager && Mixxx.StreamingManager.deezerArl.length > 20) ? "#00e676" : "#ff8a80"
                            text: (Mixxx.StreamingManager && Mixxx.StreamingManager.deezerArl.length > 20) ? qsTr("● Configurada") : qsTr("● Sin ARL (usando matching)")
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TextField {
                        id: arlInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        placeholderText: qsTr("Pega aquí tu cookie ARL (192 caracteres)...")
                        color: "#ffffff"
                        font.pixelSize: 11
                        text: Mixxx.StreamingManager ? Mixxx.StreamingManager.deezerArl : ""
                        background: Rectangle {
                            color: "#1c142c"
                            border.color: "#3d2466"
                            border.width: 1
                            radius: 4
                        }
                    }

                    Skin.Button {
                        Layout.preferredHeight: 34
                        Layout.preferredWidth: 80
                        activeColor: "#b388ff"
                        backgroundColor: "#4a148c"
                        normalColor: "#ffffff"
                        font.bold: true
                        font.pixelSize: 11
                        text: qsTr("Guardar")
                        onClicked: {
                            if (Mixxx.StreamingManager) {
                                Mixxx.StreamingManager.deezerArl = arlInput.text.trim();
                                root.statusMessage = qsTr("✔ Cookie ARL guardada correctamente");
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: qsTr("Servidor Deemix (opcional):")
                        font.pixelSize: 11
                        color: "#9575cd"
                    }

                    TextField {
                        id: deemixInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        placeholderText: qsTr("http://192.168.0.X:6595 (opcional si usas Deemix relay local)")
                        color: "#ffffff"
                        font.pixelSize: 11
                        text: Mixxx.StreamingManager ? Mixxx.StreamingManager.deemixServerUrl : ""
                        background: Rectangle {
                            color: "#1c142c"
                            border.color: "#3d2466"
                            border.width: 1
                            radius: 4
                        }
                    }

                    Skin.Button {
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 80
                        activeColor: "#b388ff"
                        backgroundColor: "#311b92"
                        normalColor: "#ffffff"
                        font.bold: true
                        font.pixelSize: 11
                        text: qsTr("Guardar")
                        onClicked: {
                            if (Mixxx.StreamingManager) {
                                Mixxx.StreamingManager.deemixServerUrl = deemixInput.text.trim();
                                root.statusMessage = qsTr("✔ Servidor Deemix guardado");
                            }
                        }
                    }
                }
            }
        }

        // ================= SEARCH & QUICK GENRES =================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: "#14111f"
            border.color: "#231a38"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 5
                spacing: 6

                TextField {
                    id: searchInput
                    Layout.preferredWidth: 260
                    Layout.preferredHeight: 34
                    placeholderText: qsTr("Search millions of tracks & artists...")
                    color: "#ffffff"
                    font.pixelSize: 13
                    background: Rectangle {
                        color: "#211b33"
                        border.color: searchInput.activeFocus ? "#b388ff" : "#372c54"
                        border.width: 1
                        radius: 4
                    }
                    onAccepted: {
                        if (text.trim() !== "") {
                            root.isBusy = true;
                            root.statusMessage = qsTr("Searching Deezer...");
                            Mixxx.StreamingManager.searchDeezer(text);
                        }
                    }
                }

                Skin.Button {
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: 80
                    activeColor: "#d1c4e9"
                    backgroundColor: "#512da8"
                    normalColor: "#ffffff"
                    font.bold: true
                    font.pixelSize: 11
                    text: qsTr("Search")
                    enabled: !root.isBusy
                    onClicked: {
                        if (searchInput.text.trim() !== "") {
                            root.isBusy = true;
                            root.statusMessage = qsTr("Searching Deezer...");
                            Mixxx.StreamingManager.searchDeezer(searchInput.text);
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
                            model: ["Dance", "Electronic", "House", "Techno", "Pop", "Rock", "Hip-Hop"]

                            delegate: Rectangle {
                                Layout.preferredHeight: 28
                                Layout.preferredWidth: genreLabel.implicitWidth + 16
                                radius: 14
                                color: root.currentGenre === modelData ? "#7c4dff" : "#211a33"
                                border.color: root.currentGenre === modelData ? "#b388ff" : "#382959"
                                border.width: 1

                                Text {
                                    id: genreLabel
                                    anchors.centerIn: parent
                                    text: modelData
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: root.currentGenre === modelData ? "#ffffff" : "#d1c4e9"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        root.currentGenre = modelData;
                                        root.isBusy = true;
                                        root.statusMessage = qsTr("Searching %1 on Deezer...").arg(modelData);
                                        searchInput.text = modelData;
                                        Mixxx.StreamingManager.searchDeezer(modelData);
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
            color: "#0f0d17"

            ListView {
                id: tracksList
                anchors.fill: parent
                clip: true
                spacing: 2
                model: root.tracksModel

                ScrollBar.vertical: ScrollBar {
                    id: dzScroll
                    width: 14
                    policy: ScrollBar.AsNeeded
                    contentItem: Rectangle {
                        implicitWidth: 10
                        implicitHeight: 40
                        radius: 5
                        color: dzScroll.pressed ? "#b388ff" : "#3d3257"
                    }
                }

                delegate: Rectangle {
                    id: trackDelegate
                    width: tracksList.width
                    height: 58
                    color: index % 2 === 0 ? "#151221" : "#110e1a"
                    border.color: "#251d38"
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
                            color: "#251d38"
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
                                text: "🎧"
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
                                    color: "#b388ff"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        Rectangle {
                            visible: track.bpm !== undefined && track.bpm > 0
                            Layout.preferredHeight: 18
                            Layout.preferredWidth: bpmText.implicitWidth + 8
                            color: "#261a40"
                            radius: 3

                            Text {
                                id: bpmText
                                anchors.centerIn: parent
                                text: Math.round(track.bpm) + " BPM"
                                font.pixelSize: 9
                                font.bold: true
                                color: "#b388ff"
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
                            color: track.isCached ? "#00e67622" : "#b388ff22"
                            border.color: track.isCached ? "#00e676" : "#b388ff"
                            border.width: 1

                            Text {
                                id: cacheStatusText
                                anchors.centerIn: parent
                                font.pixelSize: 9
                                font.bold: true
                                color: track.isCached ? "#00e676" : "#b388ff"
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
                                    track.streamUrl, "deezer", track.id, track.title, track.artist, "[Channel1]", false
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
                                    track.streamUrl, "deezer", track.id, track.title, track.artist, "[Channel2]", false
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
                        color: "#b388ff"
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
                            text: "🎧"
                            font.pixelSize: 32
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("No Deezer tracks loaded yet")
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: 14
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Search for tracks or browse top charts above")
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
                        text: qsTr("Connecting to Deezer...")
                        color: "#b388ff"
                        font.bold: true
                        font.pixelSize: 13
                    }
                }
            }
        }
    }
}

