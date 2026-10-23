import "../.." as Skin
import "../../Deck" as DeckComponent
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Layouts

// The deck display of the booth skin: track, remaining time, tempo, BPM,
// hot cues and overview of one deck, in the style of a booth player screen.
Rectangle {
    id: root

    required property string group
    required property int deckNumber
    // Small screens and the browse view: fewer and smaller elements
    property bool compact: false
    // Only the title row and the overview, while browsing the library
    property bool slim: false

    readonly property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)
    readonly property var currentTrack: deckPlayer?.currentTrack
    readonly property bool loaded: trackLoadedControl.value > 0
    readonly property color accent: deckNumber === 1 ? "#2f9bff" : "#ff8a1f"
    readonly property color dim: "#7a7f87"
    property bool showElapsed: false

    function formatTime(seconds) {
        if (!(seconds >= 0)) {
            seconds = 0;
        }
        const minutes = Math.floor(seconds / 60);
        const rest = seconds - minutes * 60;
        const whole = Math.floor(rest);
        const tenths = Math.floor((rest - whole) * 10);
        return minutes + ":" + (whole < 10 ? "0" : "") + whole + "." + tenths;
    }
    function hotcueColor(value) {
        return "#" + ("000000" + Math.max(0, Math.round(value)).toString(16)).slice(-6);
    }

    color: "#000000"
    border.color: playControl.value > 0 ? root.accent : "#33363b"
    border.width: 1

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    Mixxx.ControlProxy {
        id: playControl

        group: root.group
        key: "play"
    }
    Mixxx.ControlProxy {
        id: cueControl

        group: root.group
        key: "cue_default"
    }
    Mixxx.ControlProxy {
        id: durationControl

        group: root.group
        key: "duration"
    }
    Mixxx.ControlProxy {
        id: positionControl

        group: root.group
        key: "playposition"
    }
    Mixxx.ControlProxy {
        id: bpmControl

        group: root.group
        key: "bpm"
    }
    Mixxx.ControlProxy {
        id: rateControl

        group: root.group
        key: "rate"
    }
    Mixxx.ControlProxy {
        id: rateRangeControl

        group: root.group
        key: "rateRange"
    }
    Mixxx.ControlProxy {
        id: syncControl

        group: root.group
        key: "sync_enabled"
    }
    Mixxx.ControlProxy {
        id: syncLeaderControl

        group: root.group
        key: "sync_leader"
    }
    Mixxx.ControlProxy {
        id: keylockControl

        group: root.group
        key: "keylock"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: root.compact ? 2 : 4

        // ---- Deck number, cover, title, key ----
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compact ? 22 : 30
            spacing: 6

            Rectangle {
                Layout.preferredHeight: parent.height
                Layout.preferredWidth: parent.height
                color: playControl.value > 0 ? root.accent : "#1c1f24"
                radius: 2

                Text {
                    anchors.centerIn: parent
                    color: playControl.value > 0 ? "#000000" : root.accent
                    font.bold: true
                    font.pixelSize: root.compact ? 13 : 17
                    text: root.deckNumber
                }
            }
            Image {
                Layout.preferredHeight: parent.height
                Layout.preferredWidth: parent.height
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
                source: root.currentTrack?.coverArtUrl ?? ""
                visible: root.loaded && status === Image.Ready
            }
            Text {
                Layout.fillWidth: true
                color: root.loaded ? "#ffffff" : root.dim
                elide: Text.ElideRight
                font.pixelSize: root.compact ? 13 : 17
                text: root.loaded ? (root.currentTrack?.title || qsTr("Unknown title")) : qsTr("No track")
            }
            Text {
                color: "#ffffff"
                font.bold: true
                font.pixelSize: root.compact ? 12 : 15
                text: root.loaded ? (root.currentTrack?.keyText ?? "") : ""
                visible: text.length > 0
            }
        }

        // ---- Transport, time, tempo, BPM, sync ----
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: root.compact ? 4 : 8
            visible: !root.slim

            // CUE and PLAY for playing by touch
            ColumnLayout {
                Layout.fillHeight: true
                Layout.preferredWidth: root.compact ? 40 : 54
                spacing: 4

                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    border.color: "#e0a000"
                    border.width: 1
                    color: cueControl.value > 0 ? "#e0a000" : "#141414"
                    radius: 3

                    Text {
                        anchors.centerIn: parent
                        color: cueControl.value > 0 ? "#000000" : "#e0a000"
                        font.bold: true
                        font.pixelSize: root.compact ? 11 : 13
                        text: "CUE"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onPressed: cueControl.value = 1
                        onReleased: cueControl.value = 0
                        onCanceled: cueControl.value = 0
                    }
                }
                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    border.color: "#3ddc6a"
                    border.width: 1
                    color: playControl.value > 0 ? "#3ddc6a" : "#141414"
                    radius: 3

                    Text {
                        anchors.centerIn: parent
                        color: playControl.value > 0 ? "#000000" : "#3ddc6a"
                        font.bold: true
                        font.pixelSize: root.compact ? 13 : 16
                        text: playControl.value > 0 ? "❚❚" : "▶"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: playControl.value = playControl.value > 0 ? 0 : 1
                    }
                }
            }

            // Remaining or elapsed time, tap to switch
            Item {
                Layout.fillHeight: true
                Layout.fillWidth: true

                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    color: root.dim
                    font.pixelSize: root.compact ? 9 : 11
                    text: root.showElapsed ? qsTr("TIME") : qsTr("REMAIN")
                }
                Text {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    color: root.loaded ? "#ffffff" : root.dim
                    font.family: "monospace"
                    font.pixelSize: Math.max(18, Math.min(root.compact ? 30 : 46, parent.height * 0.72))
                    text: {
                        const duration = durationControl.value;
                        const elapsed = duration * positionControl.value;
                        return root.showElapsed ? root.formatTime(elapsed) : "-" + root.formatTime(duration - elapsed);
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.showElapsed = !root.showElapsed
                }
            }

            // Tempo and key lock
            ColumnLayout {
                Layout.fillHeight: true
                spacing: 2

                RowLayout {
                    spacing: 4

                    Rectangle {
                        Layout.preferredHeight: root.compact ? 15 : 18
                        Layout.preferredWidth: rangeText.implicitWidth + 8
                        color: "#c8102e"
                        radius: 2

                        Text {
                            id: rangeText

                            anchors.centerIn: parent
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: root.compact ? 9 : 11
                            text: "±" + Math.round(rateRangeControl.value * 100)
                        }
                    }
                    Rectangle {
                        Layout.preferredHeight: root.compact ? 15 : 18
                        Layout.preferredWidth: mtText.implicitWidth + 8
                        border.color: "#e04040"
                        border.width: 1
                        color: keylockControl.value > 0 ? "#e04040" : "transparent"
                        radius: 2

                        Text {
                            id: mtText

                            anchors.centerIn: parent
                            color: keylockControl.value > 0 ? "#000000" : "#e04040"
                            font.bold: true
                            font.pixelSize: root.compact ? 9 : 11
                            text: "MT"
                        }
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -4
                            onClicked: keylockControl.value = keylockControl.value > 0 ? 0 : 1
                        }
                    }
                }
                Text {
                    color: "#ffffff"
                    font.family: "monospace"
                    font.pixelSize: root.compact ? 13 : 17
                    text: {
                        const percent = rateControl.value * rateRangeControl.value * 100;
                        return (percent > 0 ? "+" : "") + percent.toFixed(2) + "%";
                    }
                }
            }

            // BPM: tap to correct the beatgrid
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: root.compact ? 72 : 104
                border.color: syncControl.value > 0 ? root.accent : "#d0d0d0"
                border.width: 1
                color: bpmMouse.pressed ? "#1c2430" : "#000000"
                radius: 3

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.top: parent.top
                    color: root.dim
                    font.pixelSize: root.compact ? 8 : 10
                    text: syncLeaderControl.value > 0 ? qsTr("BPM LEADER") : "BPM"
                }
                Text {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: syncControl.value > 0 ? root.accent : "#ffffff"
                    font.family: "monospace"
                    font.pixelSize: Math.max(14, Math.min(root.compact ? 22 : 32, parent.height * 0.55))
                    text: root.loaded && bpmControl.value > 0 ? bpmControl.value.toFixed(1) : "--.-"
                }
                MouseArea {
                    id: bpmMouse

                    anchors.fill: parent
                    enabled: root.loaded
                    onClicked: beatgridPopup.open()
                }
                DeckComponent.BeatgridPopup {
                    id: beatgridPopup

                    group: root.group
                }
            }

            // SYNC
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: root.compact ? 44 : 58
                border.color: root.accent
                border.width: 1
                color: syncControl.value > 0 ? root.accent : "#141414"
                radius: 3

                Text {
                    anchors.centerIn: parent
                    color: syncControl.value > 0 ? "#000000" : root.accent
                    font.bold: true
                    font.pixelSize: root.compact ? 11 : 13
                    text: "SYNC"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: syncControl.value = syncControl.value > 0 ? 0 : 1
                }
            }
        }

        // ---- Hot cues A-H: tap to jump, like the pads ----
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compact ? 16 : 20
            spacing: 2
            visible: !root.slim

            Repeater {
                model: 8

                Rectangle {
                    id: hotcueBox

                    required property int index
                    readonly property bool isSet: statusControl.value > 0

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    border.color: isSet ? root.hotcueColor(colorControl.value) : "#2c2f34"
                    border.width: 1
                    color: isSet ? Qt.darker(root.hotcueColor(colorControl.value), 1.6) : "#0c0d0f"
                    radius: 2

                    Mixxx.ControlProxy {
                        id: statusControl

                        group: root.group
                        key: `hotcue_${hotcueBox.index + 1}_status`
                    }
                    Mixxx.ControlProxy {
                        id: colorControl

                        group: root.group
                        key: `hotcue_${hotcueBox.index + 1}_color`
                    }
                    Mixxx.ControlProxy {
                        id: activateControl

                        group: root.group
                        key: `hotcue_${hotcueBox.index + 1}_activate`
                    }
                    Text {
                        anchors.centerIn: parent
                        color: hotcueBox.isSet ? "#ffffff" : "#4a4e55"
                        font.bold: true
                        font.pixelSize: root.compact ? 9 : 11
                        text: String.fromCharCode(65 + hotcueBox.index)
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.loaded
                        onPressed: activateControl.value = 1
                        onReleased: activateControl.value = 0
                        onCanceled: activateControl.value = 0
                    }
                }
            }
        }

        // ---- Overview of the whole track ----
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: root.slim
            Layout.preferredHeight: root.compact ? 22 : 34
            border.color: "#2c2f34"
            border.width: 1
            clip: true
            color: "#050607"

            Skin.WaveformOverview {
                anchors.fill: parent
                anchors.margins: 1
                group: root.group
            }
        }
    }
}
