import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// BEAT FX of the booth skin: effect unit 1, which the BEAT FX section of
// controllers like the DDJ-FLX4 drives as well. Shows the focused effect
// slot, like the effect select of a booth player.
Rectangle {
    id: root

    readonly property string unitGroup: "[EffectRack1_EffectUnit1]"
    // The focused slot; the mappings use the first one when none is
    readonly property int focusedSlot: Math.min(3, Math.max(1, focusedControl.value))
    readonly property string slotGroup: `[EffectRack1_EffectUnit1_Effect${root.focusedSlot}]`
    readonly property var slot: Mixxx.EffectsManager.getEffectSlot(1, root.focusedSlot)
    readonly property color accent: "#2f9bff"

    color: "#0b0c0e"
    border.color: "#2c2f34"
    border.width: 1

    component SmallLabel: Text {
        color: "#8a9099"
        font.pixelSize: 10
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: true
    }
    // A button that switches a control on and off
    component ToggleBox: Rectangle {
        id: toggleBox

        required property string group
        required property string key
        required property string label
        property color onColor: root.accent

        Layout.fillWidth: true
        Layout.preferredHeight: 30
        border.color: toggleControl.value > 0 ? onColor : "#3a3d42"
        border.width: 1
        color: toggleControl.value > 0 ? onColor : "#141518"
        radius: 3

        Mixxx.ControlProxy {
            id: toggleControl

            group: toggleBox.group
            key: toggleBox.key
        }
        Text {
            anchors.centerIn: parent
            color: toggleControl.value > 0 ? "#000000" : "#c0c4ca"
            font.bold: true
            font.pixelSize: 12
            text: toggleBox.label
        }
        MouseArea {
            anchors.fill: parent
            onClicked: toggleControl.value = toggleControl.value > 0 ? 0 : 1
        }
    }

    Mixxx.ControlProxy {
        id: focusedControl

        group: root.unitGroup
        key: "focused_effect"
    }
    Mixxx.ControlProxy {
        id: mixControl

        group: root.unitGroup
        key: "mix"
    }
    Mixxx.ControlProxy {
        id: metaControl

        group: root.slotGroup
        key: "meta"
    }
    Mixxx.ControlProxy {
        id: enabledControl

        group: root.slotGroup
        key: "enabled"
    }
    Mixxx.ControlProxy {
        id: nextEffectControl

        group: root.slotGroup
        key: "next_effect"
    }
    Mixxx.ControlProxy {
        id: prevEffectControl

        group: root.slotGroup
        key: "prev_effect"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        Text {
            Layout.fillWidth: true
            color: "#ffffff"
            font.bold: true
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            text: "BEAT FX"
        }

        // The effect of the focused slot: ◀ ▶ change it
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            spacing: 4

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 24
                color: prevMouse.pressed ? "#2a2d33" : "#141518"
                radius: 3

                Text {
                    anchors.centerIn: parent
                    color: "#c0c4ca"
                    text: "◀"
                }
                MouseArea {
                    id: prevMouse

                    anchors.fill: parent
                    onClicked: {
                        prevEffectControl.value = 1;
                        prevEffectControl.value = 0;
                    }
                }
            }
            Rectangle {
                Layout.fillHeight: true
                Layout.fillWidth: true
                border.color: "#5a5e66"
                border.width: 1
                color: "#000000"

                Text {
                    anchors.fill: parent
                    anchors.margins: 3
                    color: "#ffffff"
                    elide: Text.ElideRight
                    font.bold: true
                    font.pixelSize: 15
                    horizontalAlignment: Text.AlignHCenter
                    text: root.slot && root.slot.loaded ? root.slot.effectName : "—"
                    verticalAlignment: Text.AlignVCenter
                }
            }
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 24
                color: nextMouse.pressed ? "#2a2d33" : "#141518"
                radius: 3

                Text {
                    anchors.centerIn: parent
                    color: "#c0c4ca"
                    text: "▶"
                }
                MouseArea {
                    id: nextMouse

                    anchors.fill: parent
                    onClicked: {
                        nextEffectControl.value = 1;
                        nextEffectControl.value = 0;
                    }
                }
            }
        }

        // The three slots of the unit, like BEAT LEFT / RIGHT
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Repeater {
                model: 3

                Rectangle {
                    id: slotBox

                    required property int index
                    readonly property bool focused: root.focusedSlot === index + 1

                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    border.color: focused ? root.accent : "#3a3d42"
                    border.width: 1
                    color: slotEnabledControl.value > 0 ? "#123a5c" : "#141518"
                    radius: 3

                    Mixxx.ControlProxy {
                        id: slotEnabledControl

                        group: `[EffectRack1_EffectUnit1_Effect${slotBox.index + 1}]`
                        key: "enabled"
                    }
                    Text {
                        anchors.centerIn: parent
                        color: slotBox.focused ? "#ffffff" : "#8a9099"
                        font.bold: slotBox.focused
                        font.pixelSize: 11
                        text: slotBox.index + 1
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: focusedControl.value = slotBox.index + 1
                    }
                }
            }
        }

        SmallLabel {
            text: qsTr("LEVEL/DEPTH %1%").arg(Math.round(mixControl.value * 100))
        }
        Slider {
            Layout.fillWidth: true
            from: 0
            to: 1
            value: mixControl.value
            onMoved: mixControl.value = value
        }
        SmallLabel {
            text: qsTr("EFFECT AMOUNT %1%").arg(Math.round(metaControl.value * 100))
        }
        Slider {
            Layout.fillWidth: true
            from: 0
            to: 1
            value: metaControl.value
            onMoved: metaControl.value = value
        }

        SmallLabel {
            text: qsTr("CH SELECT")
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            ToggleBox {
                group: root.unitGroup
                key: "group_[Channel1]_enable"
                label: "1"
            }
            ToggleBox {
                group: root.unitGroup
                key: "group_[Channel2]_enable"
                label: "2"
            }
            ToggleBox {
                group: root.unitGroup
                key: "group_[Master]_enable"
                label: qsTr("MASTER")
            }
        }

        Item {
            Layout.fillHeight: true
        }

        // ON/OFF of the focused effect
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            border.color: "#e04040"
            border.width: 2
            color: enabledControl.value > 0 ? "#e04040" : "#141518"
            radius: 4

            Text {
                anchors.centerIn: parent
                color: enabledControl.value > 0 ? "#000000" : "#e04040"
                font.bold: true
                font.pixelSize: 15
                text: "ON / OFF"
            }
            MouseArea {
                anchors.fill: parent
                onClicked: enabledControl.value = enabledControl.value > 0 ? 0 : 1
            }
        }
    }
}
