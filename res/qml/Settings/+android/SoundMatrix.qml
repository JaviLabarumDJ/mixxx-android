import QtQuick
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import ".." as Skin

// The full sound routing, like the sound preferences of the desktop: for
// each output and input of Mixxx, a device and its channels (a stereo pair,
// or one channel in mono where Mixxx allows it). SoundHardware loads it,
// and applies it on save when it was edited.
ColumnLayout {
    id: root

    // The sound API whose devices are listed
    property string api: ""
    // Something was changed since the last load
    property bool edited: false
    // 0: outputs, 1: inputs
    property int tab: 0

    property var outputDevices: []
    property var inputDevices: []
    // Per row: {device: index in the devices or -1, channel: option index}
    property var outputSelection: []
    property var inputSelection: []

    // The types of SoundManagerConfig (AudioPathType) and their index
    readonly property var outputRows: [
        {"name": qsTr("Main"), "type": 0, "index": 0},
        {"name": qsTr("Headphones"), "type": 1, "index": 0},
        {"name": qsTr("Booth"), "type": 2, "index": 0},
        {"name": qsTr("Left bus"), "type": 3, "index": 0},
        {"name": qsTr("Center bus"), "type": 3, "index": 1},
        {"name": qsTr("Right bus"), "type": 3, "index": 2},
        {"name": qsTr("Deck 1"), "type": 4, "index": 0},
        {"name": qsTr("Deck 2"), "type": 4, "index": 1},
        {"name": qsTr("Deck 3"), "type": 4, "index": 2},
        {"name": qsTr("Deck 4"), "type": 4, "index": 3}
    ]
    readonly property var inputRows: [
        {"name": qsTr("Vinyl control 1"), "type": 5, "index": 0},
        {"name": qsTr("Vinyl control 2"), "type": 5, "index": 1},
        {"name": qsTr("Vinyl control 3"), "type": 5, "index": 2},
        {"name": qsTr("Vinyl control 4"), "type": 5, "index": 3},
        {"name": qsTr("Microphone 1"), "type": 6, "index": 0},
        {"name": qsTr("Microphone 2"), "type": 6, "index": 1},
        {"name": qsTr("Microphone 3"), "type": 6, "index": 2},
        {"name": qsTr("Microphone 4"), "type": 6, "index": 3},
        {"name": qsTr("Auxiliary 1"), "type": 7, "index": 0},
        {"name": qsTr("Auxiliary 2"), "type": 7, "index": 1},
        {"name": qsTr("Auxiliary 3"), "type": 7, "index": 2},
        {"name": qsTr("Auxiliary 4"), "type": 7, "index": 3},
        {"name": qsTr("Recording / broadcasting"), "type": 8, "index": 0}
    ]

    // Main, headphones, booth and recording have a single path; buses,
    // decks, vinyl control, microphones and auxiliaries are numbered
    function indexed(type) {
        return type >= 3 && type <= 7;
    }

    // The channels a path can use on a device: stereo pairs, then single
    // channels (mono) unless it is vinyl control
    function channelOptions(device, type) {
        const options = [];
        if (!device) {
            return options;
        }
        const channels = device.channelCount;
        for (let base = 0; base + 1 < channels; base += 2) {
            options.push({"label": qsTr("Channels %1 - %2").arg(base + 1).arg(base + 2), "base": base, "count": 2});
        }
        if (type !== 5) {
            for (let base = 0; base < channels; ++base) {
                options.push({"label": qsTr("Channel %1 (mono)").arg(base + 1), "base": base, "count": 1});
            }
        }
        return options;
    }

    function optionLabels(device, type) {
        return channelOptions(device, type).map(option => option.label);
    }

    // The selections from the current configuration
    function selectionsFor(devices, rows) {
        const manager = Mixxx.SoundManager;
        const selection = rows.map(() => ({"device": -1, "channel": 0}));
        for (let d = 0; d < devices.length; ++d) {
            const connections = devices[d].connections(manager) || [];
            for (const connection of connections) {
                for (let r = 0; r < rows.length; ++r) {
                    const row = rows[r];
                    if (connection.type !== row.type || (indexed(row.type) && connection.index !== row.index)) {
                        continue;
                    }
                    const options = channelOptions(devices[d], row.type);
                    let channel = options.findIndex(option => option.base === connection.channelGroup && option.count === connection.channelCount);
                    selection[r] = {"device": d, "channel": Math.max(0, channel)};
                }
            }
        }
        return selection;
    }

    function load() {
        const manager = Mixxx.SoundManager;
        root.outputDevices = manager.availableOutputDevices(root.api) || [];
        root.inputDevices = manager.availableInputDevices(root.api) || [];
        root.outputSelection = selectionsFor(root.outputDevices, root.outputRows);
        root.inputSelection = selectionsFor(root.inputDevices, root.inputRows);
        root.edited = false;
    }

    // Replaces the outputs and inputs of the manager with these ones
    function apply(manager) {
        manager.clearOutputs();
        manager.clearInputs();
        for (let r = 0; r < root.outputRows.length; ++r) {
            const selected = root.outputSelection[r];
            const device = root.outputDevices[selected.device];
            const option = channelOptions(device, root.outputRows[r].type)[selected.channel];
            if (device && option) {
                manager.addOutput(device, root.outputRows[r].type, option.base, root.outputRows[r].index, option.count);
            }
        }
        for (let r = 0; r < root.inputRows.length; ++r) {
            const selected = root.inputSelection[r];
            const device = root.inputDevices[selected.device];
            const option = channelOptions(device, root.inputRows[r].type)[selected.channel];
            if (device && option) {
                manager.addInput(device, root.inputRows[r].type, option.base, root.inputRows[r].index, option.count);
            }
        }
    }

    // [AI-GENERATED DISCLAIMER START: Autonomously generated by an AI agent]
    // Some input (vinyl control, microphone, auxiliary...) has a device
    function hasInputs() {
        return root.inputSelection.some(selected => selected && selected.device >= 0);
    }
    // [AI-GENERATED DISCLAIMER END: Autonomously generated by an AI agent]

    function select(inputs, row, device, channel) {
        const selection = (inputs ? root.inputSelection : root.outputSelection).slice();
        selection[row] = {"device": device, "channel": channel};
        if (inputs) {
            root.inputSelection = selection;
        } else {
            root.outputSelection = selection;
        }
        root.edited = true;
    }

    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        Repeater {
            model: [qsTr("Output"), qsTr("Input")]

            Rectangle {
                required property int index
                required property string modelData

                Layout.preferredHeight: 32
                Layout.preferredWidth: 120
                border.color: root.tab === index ? "#42a5f5" : "#324050"
                color: root.tab === index ? "#1e3a5f" : "#1a222c"
                radius: 6

                Text {
                    anchors.centerIn: parent
                    color: root.tab === parent.index ? "#ffffff" : "#b0bec5"
                    font.bold: true
                    font.pixelSize: 13
                    text: parent.modelData
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.tab = parent.index
                }
            }
        }
        Item {
            Layout.fillWidth: true
        }
        Text {
            color: "#78909c"
            font.pixelSize: 11
            text: root.tab === 0 ? qsTr("%1 output devices").arg(root.outputDevices.length) : qsTr("%1 input devices").arg(root.inputDevices.length)
        }
    }

    Repeater {
        model: root.tab === 0 ? root.outputRows : root.inputRows

        RowLayout {
            id: pathRow

            required property int index
            required property var modelData
            readonly property bool inputs: root.tab === 1
            readonly property var devices: inputs ? root.inputDevices : root.outputDevices
            readonly property var selected: (inputs ? root.inputSelection : root.outputSelection)[index] || {"device": -1, "channel": 0}
            readonly property var device: selected.device >= 0 ? devices[selected.device] : null

            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.preferredWidth: 190
                color: pathRow.device ? "#ffffff" : "#90a4ae"
                elide: Text.ElideRight
                font.bold: pathRow.device !== null
                font.pixelSize: 13
                text: pathRow.modelData.name
            }
            Skin.ComboBox {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                Layout.preferredWidth: 3
                currentIndex: pathRow.selected.device + 1
                model: ["---"].concat(pathRow.devices.map(device => device.displayName))

                onActivated: index => root.select(pathRow.inputs, pathRow.index, index - 1, 0)
            }
            Skin.ComboBox {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                Layout.preferredWidth: 2
                currentIndex: pathRow.device ? pathRow.selected.channel : -1
                enabled: pathRow.device !== null
                model: root.optionLabels(pathRow.device, pathRow.modelData.type)

                onActivated: index => root.select(pathRow.inputs, pathRow.index, pathRow.selected.device, index)
            }
        }
    }
}
