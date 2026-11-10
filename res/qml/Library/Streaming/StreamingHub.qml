import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "../../Theme"
import "../.." as Skin

Item {
    id: root

    property string targetDeck: "[Channel1]"
    // 0: SoundCloud, 1: Deezer, 2: Beatport, 3: Spotify/Apple, 4: Subsonic, 5: Audius, 6: Custom URL
    property int activePlatform: 1
    // Keep all platform implementations and background matching intact, while showing only Deezer in the UI
    property bool showAllPlatforms: false

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ================= MULTI-PLATFORM SELECTOR BAR =================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.showAllPlatforms ? 48 : 0
            visible: root.showAllPlatforms
            color: "#12151c"
            border.color: "#222734"
            border.width: root.showAllPlatforms ? 1 : 0
            clip: true

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 8
                spacing: 8

                // Horizontal scrollable platforms tabs
                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: tabsRow.width
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    RowLayout {
                        id: tabsRow
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        Skin.Button {
                            Layout.preferredHeight: 36
                            Layout.preferredWidth: 145
                            activeColor: "#ff5500"
                            backgroundColor: root.activePlatform === 0 ? "#4a1c0d" : "#1a212d"
                            normalColor: root.activePlatform === 0 ? "#ff7733" : "#ffab91"
                            font.bold: true
                            font.pixelSize: 11
                            text: qsTr("☁️ SOUNDCLOUD")
                            onClicked: root.activePlatform = 0
                        }

                        Skin.Button {
                            Layout.preferredHeight: 36
                            Layout.preferredWidth: 125
                            activeColor: "#b388ff"
                            backgroundColor: root.activePlatform === 1 ? "#311b92" : "#1a212d"
                            normalColor: root.activePlatform === 1 ? "#b388ff" : "#d1c4e9"
                            font.bold: true
                            font.pixelSize: 11
                            text: qsTr("🎧 DEEZER")
                            onClicked: root.activePlatform = 1
                        }

                        Skin.Button {
                            Layout.preferredHeight: 36
                            Layout.preferredWidth: 135
                            activeColor: "#00ff88"
                            backgroundColor: root.activePlatform === 2 ? "#004d40" : "#1a212d"
                            normalColor: root.activePlatform === 2 ? "#00ff88" : "#a7ffeb"
                            font.bold: true
                            font.pixelSize: 11
                            text: qsTr("⚡ BEATPORT")
                            onClicked: root.activePlatform = 2
                        }

                        Skin.Button {
                            Layout.preferredHeight: 36
                            Layout.preferredWidth: 165
                            activeColor: "#1db954"
                            backgroundColor: root.activePlatform === 3 ? "#0f381c" : "#1a212d"
                            normalColor: root.activePlatform === 3 ? "#1db954" : "#a5d6a7"
                            font.bold: true
                            font.pixelSize: 11
                            text: qsTr("🟢 SPOTIFY / APPLE")
                            onClicked: root.activePlatform = 3
                        }

                        Skin.Button {
                            Layout.preferredHeight: 36
                            Layout.preferredWidth: 175
                            activeColor: "#00e5ff"
                            backgroundColor: root.activePlatform === 4 ? "#00363a" : "#1a212d"
                            normalColor: root.activePlatform === 4 ? "#00e5ff" : "#80d8ff"
                            font.bold: true
                            font.pixelSize: 11
                            text: qsTr("🏠 NAVIDROME / SUBSONIC")
                            onClicked: root.activePlatform = 4
                        }

                        Skin.Button {
                            Layout.preferredHeight: 36
                            Layout.preferredWidth: 120
                            activeColor: "#ea80fc"
                            backgroundColor: root.activePlatform === 5 ? "#4a148c" : "#1a212d"
                            normalColor: root.activePlatform === 5 ? "#ea80fc" : "#e1bee7"
                            font.bold: true
                            font.pixelSize: 11
                            text: qsTr("🔥 AUDIUS")
                            onClicked: root.activePlatform = 5
                        }

                        Skin.Button {
                            Layout.preferredHeight: 36
                            Layout.preferredWidth: 140
                            activeColor: "#ffb300"
                            backgroundColor: root.activePlatform === 6 ? "#4a3b00" : "#1a212d"
                            normalColor: root.activePlatform === 6 ? "#ffb300" : "#ffe082"
                            font.bold: true
                            font.pixelSize: 11
                            text: qsTr("🔗 DIRECT URL")
                            onClicked: root.activePlatform = 6
                        }
                    }
                }

                // Quick Deck Destination Indicator
                Rectangle {
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: 105
                    radius: 4
                    color: root.targetDeck === "[Channel1]" ? "#0d47a1" : "#bf360c"
                    border.color: root.targetDeck === "[Channel1]" ? "#00b0ff" : "#ff6d00"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 4

                        Text {
                            text: qsTr("Target:")
                            color: "#b0bec5"
                            font.pixelSize: 10
                        }

                        Text {
                            text: root.targetDeck === "[Channel1]" ? "DECK 1" : "DECK 2"
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            root.targetDeck = (root.targetDeck === "[Channel1]") ? "[Channel2]" : "[Channel1]";
                        }
                    }
                }
            }
        }

        // ================= PLATFORM VIEWS CONTAINER =================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            SoundCloudView {
                anchors.fill: parent
                visible: root.showAllPlatforms && root.activePlatform === 0
            }

            DeezerView {
                anchors.fill: parent
                visible: root.activePlatform === 1
            }

            BeatportView {
                anchors.fill: parent
                visible: root.showAllPlatforms && root.activePlatform === 2
            }

            PlaylistImportView {
                anchors.fill: parent
                visible: root.showAllPlatforms && root.activePlatform === 3
            }

            SubsonicView {
                anchors.fill: parent
                visible: root.showAllPlatforms && root.activePlatform === 4
            }

            OnlineStreamView {
                anchors.fill: parent
                visible: root.showAllPlatforms && root.activePlatform === 5
            }

            CustomUrlView {
                anchors.fill: parent
                targetDeck: root.targetDeck
                visible: root.showAllPlatforms && root.activePlatform === 6
            }
        }
    }
}
