import QtQuick
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "." as Skin

// The Groove Station (src/engine/fxstation/groovestation): a step sequencer
// of 8 sounds and 16 steps, and pads to play them by hand, at the tempo and
// on the bars of the playing track. In place of the library, like the FX
// Station.
Rectangle {
    id: root

    signal closeRequested

    readonly property var names: ["KICK", "SNARE", "CLAP", "HAT", "OPEN HAT", "LOW TOM", "HIGH TOM", "CRASH"]
    readonly property var colors: ["#ff5252", "#ffab40", "#ffd740", "#69f0ae", "#64ffda", "#40c4ff", "#b388ff", "#ff4081"]
    readonly property bool narrow: width < 900
    // 0: sequencer, 1: pads
    property int view: 0

    color: "#0b0e13"

    // Touches on the gaps don't reach what is under the panel
    MouseArea {
        anchors.fill: parent
    }

    // A button of the panel: lit when active
    component GrooveButton: Rectangle {
        id: button

        property string text: ""
        property bool active: false
        property color activeColor: "#76ff03"
        property bool enabledLook: true
        signal clicked
        signal pressed
        signal released

        border.color: active ? activeColor : "#323a48"
        border.width: active ? 2 : 1
        color: buttonArea.pressed ? "#2a3240" : (active ? Qt.darker(activeColor, 2.6) : "#161b24")
        opacity: enabledLook ? 1 : 0.4
        radius: 4

        Text {
            anchors.fill: parent
            anchors.margins: 2
            color: button.active ? "#ffffff" : "#b0bec5"
            font.bold: true
            font.pixelSize: Math.max(8, Math.min(13, button.height * 0.36))
            fontSizeMode: Text.Fit
            horizontalAlignment: Text.AlignHCenter
            minimumPixelSize: 7
            text: button.text
            verticalAlignment: Text.AlignVCenter
        }
        MouseArea {
            id: buttonArea

            anchors.fill: parent
            enabled: button.enabledLook
            onCanceled: button.released()
            onClicked: button.clicked()
            onPressed: button.pressed()
            onReleased: button.released()
        }
    }

    Mixxx.ControlProxy {
        id: playControl

        group: "[GrooveStation]"
        key: "play"
    }
    Mixxx.ControlProxy {
        id: recordControl

        group: "[GrooveStation]"
        key: "record"
    }
    Mixxx.ControlProxy {
        id: patternControl

        group: "[GrooveStation]"
        key: "pattern"
    }
    Mixxx.ControlProxy {
        id: kitControl

        group: "[GrooveStation]"
        key: "kit"
    }
    Mixxx.ControlProxy {
        id: repeatControl

        group: "[GrooveStation]"
        key: "repeat"
    }
    Mixxx.ControlProxy {
        id: quantizeControl

        group: "[GrooveStation]"
        key: "quantize"
    }
    Mixxx.ControlProxy {
        id: clearControl

        group: "[GrooveStation]"
        key: "clear"
    }
    Mixxx.ControlProxy {
        id: reloadControl

        group: "[GrooveStation]"
        key: "reload"
    }
    Mixxx.ControlProxy {
        id: stepControl

        group: "[GrooveStation]"
        key: "step"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        // ---- Transport, patterns and kits ----
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            spacing: 4

            Text {
                color: "#b2ff59"
                font.bold: true
                font.letterSpacing: 1
                font.pixelSize: 13
                text: "GROOVE"
                visible: !root.narrow
            }
            GrooveButton {
                Layout.fillHeight: true
                Layout.preferredWidth: 96
                active: root.view === 0
                text: "SEQUENCER"
                onClicked: root.view = 0
            }
            GrooveButton {
                Layout.fillHeight: true
                Layout.preferredWidth: 64
                active: root.view === 1
                text: "PADS"
                onClicked: root.view = 1
            }
            Item {
                Layout.preferredWidth: 6
            }
            GrooveButton {
                Layout.fillHeight: true
                Layout.preferredWidth: 64
                active: playControl.value > 0
                text: playControl.value > 0 ? "■ STOP" : "▶ PLAY"
                onClicked: playControl.value = playControl.value > 0 ? 0 : 1
            }
            GrooveButton {
                Layout.fillHeight: true
                Layout.preferredWidth: 52
                active: recordControl.value > 0
                activeColor: "#ff1744"
                text: "● REC"
                onClicked: recordControl.value = recordControl.value > 0 ? 0 : 1
            }
            Item {
                Layout.preferredWidth: 6
            }
            Text {
                color: "#78909c"
                font.bold: true
                font.pixelSize: 9
                text: "PATTERN"
            }
            Repeater {
                model: 8

                GrooveButton {
                    required property int index

                    Layout.fillHeight: true
                    Layout.preferredWidth: 28
                    active: patternControl.value === index
                    activeColor: "#40c4ff"
                    text: index + 1
                    onClicked: patternControl.value = index
                }
            }
            Item {
                Layout.preferredWidth: 6
            }
            Text {
                color: "#78909c"
                font.bold: true
                font.pixelSize: 9
                text: "KIT"
            }
            Repeater {
                model: 4

                GrooveButton {
                    id: kitButton

                    required property int index

                    Layout.fillHeight: true
                    Layout.preferredWidth: 28
                    active: kitControl.value === index
                    activeColor: "#ffd740"
                    enabledLook: index === 0 || kitSounds.value > 0
                    text: index + 1
                    onClicked: kitControl.value = index

                    Mixxx.ControlProxy {
                        id: kitSounds

                        group: "[GrooveStation]"
                        key: "kit_sounds_" + (kitButton.index + 1)
                    }
                }
            }
            Item {
                Layout.fillWidth: true
            }
            GrooveButton {
                Layout.fillHeight: true
                Layout.preferredWidth: 56
                text: "CLOSE"
                onClicked: root.closeRequested()
            }
        }

        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 6

            // ---- Sequencer: a row per sound, a column per step ----
            ColumnLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 2
                visible: root.view === 0

                Repeater {
                    model: 8

                    RowLayout {
                        id: trackRow

                        required property int index
                        readonly property color trackColor: root.colors[index]

                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        spacing: 2

                        Mixxx.ControlProxy {
                            id: muteControl

                            group: "[GrooveStation]"
                            key: "mute_" + (trackRow.index + 1)
                        }
                        Mixxx.ControlProxy {
                            id: soloControl

                            group: "[GrooveStation]"
                            key: "solo_" + (trackRow.index + 1)
                        }
                        Mixxx.ControlProxy {
                            id: padControl

                            group: "[GrooveStation]"
                            key: "pad_" + (trackRow.index + 1)
                        }

                        // The name plays the sound
                        GrooveButton {
                            Layout.fillHeight: true
                            Layout.preferredWidth: root.narrow ? 54 : 74
                            active: padControl.value > 0
                            activeColor: trackRow.trackColor
                            text: root.names[trackRow.index]
                            onPressed: padControl.value = 0.9
                            onReleased: padControl.value = 0
                        }
                        GrooveButton {
                            Layout.fillHeight: true
                            Layout.preferredWidth: 22
                            active: muteControl.value > 0
                            activeColor: "#ff5252"
                            text: "M"
                            onClicked: muteControl.value = muteControl.value > 0 ? 0 : 1
                        }
                        GrooveButton {
                            Layout.fillHeight: true
                            Layout.preferredWidth: 22
                            active: soloControl.value > 0
                            activeColor: "#ffd740"
                            text: "S"
                            onClicked: soloControl.value = soloControl.value > 0 ? 0 : 1
                        }
                        Repeater {
                            model: 16

                            Rectangle {
                                id: cell

                                required property int index
                                readonly property real velocity: cellControl.value
                                readonly property bool current: stepControl.value === index

                                Layout.fillHeight: true
                                Layout.fillWidth: true
                                // The beats of the bar, in alternate shades
                                border.color: current ? "#ffffff" : (velocity > 0 ? trackRow.trackColor : "#2a313d")
                                border.width: current ? 2 : 1
                                color: velocity > 0 ? Qt.rgba(trackRow.trackColor.r, trackRow.trackColor.g, trackRow.trackColor.b, 0.3 + 0.6 * velocity) : (Math.floor(index / 4) % 2 === 0 ? "#161b24" : "#1d2430")
                                radius: 3

                                Mixxx.ControlProxy {
                                    id: cellControl

                                    group: "[GrooveStation]"
                                    key: "cell_" + (trackRow.index + 1) + "_" + (cell.index + 1)
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    // Tap: on or off; a held press: a softer hit
                                    onClicked: cellControl.value = cellControl.value > 0 ? 0 : 0.8
                                    onPressAndHold: cellControl.value = 0.4
                                }
                            }
                        }
                    }
                }
            }

            // ---- Pads: the 8 sounds to play by hand, big, filling the
            // panel ----
            GridLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 300
                columnSpacing: 8
                columns: 4
                rowSpacing: 8
                visible: root.view === 1

                Repeater {
                    // The kick bottom left, like on pad controllers
                    model: [4, 5, 6, 7, 0, 1, 2, 3]

                    Rectangle {
                        id: pad

                        required property int modelData
                        readonly property color padColor: root.colors[modelData]
                        readonly property bool lit: padHit.value > 0

                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.minimumHeight: 50
                        Layout.minimumWidth: 60
                        Layout.preferredHeight: 100
                        Layout.preferredWidth: 100
                        border.color: padColor
                        border.width: lit ? 4 : 2
                        color: lit ? Qt.rgba(padColor.r, padColor.g, padColor.b, 0.7) : Qt.rgba(padColor.r, padColor.g, padColor.b, 0.16)
                        radius: 10
                        scale: lit ? 0.97 : 1

                        Behavior on scale {
                            NumberAnimation {
                                duration: 60
                            }
                        }

                        Mixxx.ControlProxy {
                            id: padHit

                            group: "[GrooveStation]"
                            key: "pad_" + (pad.modelData + 1)
                        }
                        // A bar on the left: louder towards the top
                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 8
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.top: parent.top
                            anchors.topMargin: 8
                            color: Qt.rgba(pad.padColor.r, pad.padColor.g, pad.padColor.b, 0.35)
                            radius: 2
                            width: 4
                        }
                        Text {
                            anchors.centerIn: parent
                            color: "#ffffff"
                            font.bold: true
                            font.letterSpacing: 1
                            font.pixelSize: Math.max(11, Math.min(22, pad.height * 0.2))
                            fontSizeMode: Text.HorizontalFit
                            horizontalAlignment: Text.AlignHCenter
                            minimumPixelSize: 9
                            text: root.names[pad.modelData]
                            width: pad.width - 24
                        }
                        // One finger per pad, several pads at once; higher
                        // on the pad is louder
                        MultiPointTouchArea {
                            anchors.fill: parent
                            maximumTouchPoints: 1
                            mouseEnabled: true

                            onCanceled: padHit.value = 0
                            onPressed: points => {
                                const point = points[0];
                                padHit.value = 0.45 + 0.55 * Math.max(0, Math.min(1, 1 - point.y / height));
                            }
                            onReleased: padHit.value = 0
                        }
                    }
                }
            }

            // ---- Sound and timing ----
            ColumnLayout {
                Layout.fillHeight: true
                Layout.fillWidth: false
                Layout.maximumWidth: root.narrow ? 150 : 210
                Layout.preferredWidth: root.narrow ? 150 : 210
                spacing: 4

                RowLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    spacing: 2

                    Skin.FxStationKnob {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        accentColor: "#b2ff59"
                        defaultValue: 0
                        group: "[GrooveStation]"
                        key: "swing"
                        label: "SWING"
                        valueText: Math.round(value * 200) + "%"
                    }
                    Skin.FxStationKnob {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        accentColor: "#b2ff59"
                        defaultValue: 0.8
                        group: "[GrooveStation]"
                        key: "volume"
                        label: "LEVEL"
                    }
                }
                Text {
                    color: "#78909c"
                    font.bold: true
                    font.pixelSize: 9
                    text: "NOTE REPEAT"
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    spacing: 2

                    Repeater {
                        model: ["OFF", "1/4", "1/8", "1/16", "1/32"]

                        GrooveButton {
                            required property int index
                            required property string modelData

                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            active: repeatControl.value === index
                            text: modelData
                            onClicked: repeatControl.value = index
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    spacing: 2

                    GrooveButton {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        active: quantizeControl.value > 0
                        text: "QUANTIZE"
                        onClicked: quantizeControl.value = quantizeControl.value > 0 ? 0 : 1
                    }
                    GrooveButton {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        text: "CLEAR"
                        onPressed: clearControl.value = 1
                        onReleased: clearControl.value = 0
                    }
                    GrooveButton {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        text: "RELOAD"
                        onPressed: reloadControl.value = 1
                        onReleased: reloadControl.value = 0
                    }
                }
            }
        }
    }
}
