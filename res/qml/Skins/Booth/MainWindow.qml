import "../.." as Skin
import "." as Booth
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Booth skin: a main window in the style of the screen of a booth player,
// for landscape phones and tablets. The waveforms are stacked over the
// whole width, the deck displays are below them and BEAT FX (effect unit 1)
// is on the right. BROWSE shows the library in place of the waveforms.
//
// A skin is a folder in res/qml/Skins with a MainWindow.qml whose root item
// takes the application window, see packaging/android/README.md. The
// library, the deck loaders and the settings are the shared ones.
Item {
    id: root

    required property ApplicationWindow applicationWindow
    property var menuBar: null

    // 2: the library is shown (BROWSE). The deck loader switches to it with
    // its "full library" button, as in the Classic skin.
    property int viewMode: 1
    readonly property bool browsing: viewMode === 2
    // Landscape phones
    readonly property bool compact: height < 520
    // BEAT FX is always shown on tablets, on demand on phones
    property bool beatFxShown: false
    readonly property int numDecks: 4
    readonly property int numSamplers: 16
    readonly property int numPreviewDecks: 1
    readonly property int waveformOverviewTypeRgb: 2

    function openQuickLoad(group) {
        quickLoadDrawer.targetDeck = (group && group.length > 0) ? group : "[Channel1]";
        quickLoadDrawer.open();
    }
    function toggleLibraryView() {
        root.viewMode = root.browsing ? 1 : 2;
    }

    // ---- The same engine setup as the Classic skin ----
    Mixxx.ControlProxy {
        group: "[App]"
        key: "num_decks"

        onInitializedChanged: value = root.numDecks
    }
    Mixxx.ControlProxy {
        group: "[App]"
        key: "num_samplers"

        onInitializedChanged: value = root.numSamplers
    }
    Mixxx.ControlProxy {
        group: "[App]"
        key: "num_preview_decks"

        onInitializedChanged: value = root.numPreviewDecks
    }
    Mixxx.ControlProxy {
        group: "[Waveform]"
        key: "WaveformOverviewType"

        onInitializedChanged: value = root.waveformOverviewTypeRgb
    }
    Skin.LibraryScanSummaryDialog {
    }

    component TopButton: Rectangle {
        id: topButton

        required property string label
        property bool active: false
        property color activeColor: "#2f9bff"
        signal clicked

        Layout.fillHeight: true
        Layout.preferredWidth: Math.max(root.compact ? 54 : 74, topText.implicitWidth + 20)
        border.color: active ? activeColor : "#3a3d42"
        border.width: 1
        color: active ? activeColor : (topMouse.pressed ? "#2a2d33" : "#1a1c20")
        radius: 3

        Text {
            id: topText

            anchors.centerIn: parent
            color: topButton.active ? "#000000" : "#e6e8eb"
            font.bold: true
            font.pixelSize: root.compact ? 11 : 13
            text: topButton.label
        }
        MouseArea {
            id: topMouse

            anchors.fill: parent
            onClicked: topButton.clicked()
        }
    }
    // Next to a waveform: deck number, quantize and slip
    component DeckStrip: Rectangle {
        id: strip

        required property string group
        required property int deckNumber
        readonly property color accent: deckNumber === 1 ? "#2f9bff" : "#ff8a1f"

        color: "#0b0c0e"
        border.color: "#2c2f34"
        border.width: 1

        Mixxx.ControlProxy {
            id: quantizeControl

            group: strip.group
            key: "quantize"
        }
        Mixxx.ControlProxy {
            id: slipControl

            group: strip.group
            key: "slip_enabled"
        }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 3
            spacing: 3

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: root.compact ? 16 : 20
                color: strip.accent
                radius: 2

                Text {
                    anchors.centerIn: parent
                    color: "#000000"
                    font.bold: true
                    font.pixelSize: root.compact ? 10 : 12
                    text: strip.deckNumber
                }
            }
            Rectangle {
                Layout.fillHeight: true
                Layout.fillWidth: true
                border.color: "#e0a000"
                border.width: 1
                color: quantizeControl.value > 0 ? "#e0a000" : "transparent"
                radius: 2

                Text {
                    anchors.centerIn: parent
                    color: quantizeControl.value > 0 ? "#000000" : "#e0a000"
                    font.bold: true
                    font.pixelSize: root.compact ? 10 : 12
                    text: "Q"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: quantizeControl.value = quantizeControl.value > 0 ? 0 : 1
                }
            }
            Rectangle {
                Layout.fillHeight: true
                Layout.fillWidth: true
                border.color: "#c0c4ca"
                border.width: 1
                color: slipControl.value > 0 ? "#c0c4ca" : "transparent"
                radius: 2

                Text {
                    anchors.centerIn: parent
                    color: slipControl.value > 0 ? "#000000" : "#c0c4ca"
                    font.bold: true
                    font.pixelSize: root.compact ? 8 : 10
                    text: "SLIP"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: slipControl.value = slipControl.value > 0 ? 0 : 1
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    ColumnLayout {
        id: content

        anchors.fill: parent
        enabled: !quickLoadDrawer.opened && !settingsPopup.opened
        spacing: 3

        // ---- Top bar ----
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.preferredHeight: root.compact ? 30 : 38
            Layout.rightMargin: 4
            Layout.topMargin: 3
            spacing: 4

            TopButton {
                active: root.browsing
                label: qsTr("BROWSE")
                onClicked: root.toggleLibraryView()
            }
            TopButton {
                activeColor: "#2f9bff"
                label: qsTr("LOAD %1").arg(1)
                onClicked: root.openQuickLoad("[Channel1]")
            }
            TopButton {
                activeColor: "#ff8a1f"
                label: qsTr("LOAD %1").arg(2)
                onClicked: root.openQuickLoad("[Channel2]")
            }
            Item {
                Layout.fillWidth: true
            }
            TopButton {
                active: root.beatFxShown
                label: "BEAT FX"
                visible: root.compact && !root.browsing
                onClicked: root.beatFxShown = !root.beatFxShown
            }
            TopButton {
                label: "⚙"
                Layout.preferredWidth: root.compact ? 36 : 44
                onClicked: settingsPopup.open()
            }
        }

        // ---- Waveforms, or the library while browsing ----
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true

            RowLayout {
                anchors.fill: parent
                spacing: 3
                visible: !root.browsing

                ColumnLayout {
                    Layout.fillHeight: true
                    Layout.preferredWidth: root.compact ? 38 : 52
                    spacing: 3

                    DeckStrip {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        deckNumber: 1
                        group: "[Channel1]"
                    }
                    DeckStrip {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        deckNumber: 2
                        group: "[Channel2]"
                    }
                }
                ColumnLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    spacing: 3

                    Skin.WaveformDisplay {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        group: "[Channel1]"
                        isTouchMode: true
                    }
                    Skin.WaveformDisplay {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        group: "[Channel2]"
                        isTouchMode: true
                    }
                }
                Booth.BoothBeatFx {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 190
                    visible: !root.compact
                }
            }

            // The library stays loaded once opened, like in the Classic skin
            Loader {
                id: library

                property bool keepLoaded: false

                anchors.fill: parent
                active: keepLoaded || root.browsing
                visible: root.browsing
                onLoaded: keepLoaded = true

                sourceComponent: Component {
                    Skin.Library {
                        anchors.fill: parent
                        acceptsController: !quickLoadDrawer.opened && !settingsPopup.opened
                    }
                }
            }

            // BEAT FX of phones, over the waveforms
            Booth.BoothBeatFx {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.top: parent.top
                visible: root.compact && root.beatFxShown && !root.browsing
                width: 200
                z: 10
            }
        }

        // ---- Deck displays ----
        RowLayout {
            Layout.bottomMargin: 3
            Layout.fillWidth: true
            Layout.leftMargin: 3
            Layout.preferredHeight: root.browsing ? (root.compact ? 52 : 68) : Math.round(root.height * (root.compact ? 0.40 : 0.36))
            Layout.rightMargin: 3
            spacing: 3

            Booth.BoothDeckPanel {
                Layout.fillHeight: true
                Layout.fillWidth: true
                compact: root.compact
                deckNumber: 1
                group: "[Channel1]"
                slim: root.browsing
            }
            Booth.BoothDeckPanel {
                Layout.fillHeight: true
                Layout.fillWidth: true
                compact: root.compact
                deckNumber: 2
                group: "[Channel2]"
                slim: root.browsing
            }
        }
    }

    Skin.Settings {
        id: settingsPopup

        height: root.applicationWindow.isMobile ? parent.height : Math.min(840, parent.height)
        modal: true
        width: root.applicationWindow.isMobile ? parent.width : Math.min(1400, parent.width)
        x: root.applicationWindow.isMobile ? 0 : Math.round((parent.width - width) / 2)
        y: root.applicationWindow.isMobile ? 0 : Math.round((parent.height - height) / 2)

        Overlay.modal: Rectangle {
            color: "#99000000"
        }
    }

    Skin.QuickLoadDrawer {
        id: quickLoadDrawer

        height: root.applicationWindow.isMobile ? parent.height : Math.min(840, parent.height)
        width: root.applicationWindow.isMobile ? parent.width : Math.min(1400, parent.width)
        x: root.applicationWindow.isMobile ? 0 : Math.round((parent.width - width) / 2)
        y: root.applicationWindow.isMobile ? 0 : Math.round((parent.height - height) / 2)
    }

    // ================= CONTROLLER ACCESS TO THE SCREENS =================
    // The same [Skin] controls as the Classic skin (see its MainWindow.qml):
    // mobile_library_toggle switches BROWSE, mobile_loader_deck1/2 open the
    // deck loaders, and LOAD opens the deck loader when the library isn't
    // shown.
    property bool mobileControlsCreated: false

    Mixxx.SkinControlCreator {
        group: "[Skin]"
        key: "mobile_library_toggle"

        Component.onCompleted: root.mobileControlsCreated = true
    }
    Mixxx.ControlProxy {
        group: "[Skin]"
        key: root.mobileControlsCreated ? "mobile_library_toggle" : ""
        onValueChanged: value => {
            if (value > 0) {
                quickLoadDrawer.close();
                root.toggleLibraryView();
            }
        }
    }
    Repeater {
        model: 2

        Item {
            id: deckLoaderAccess

            required property int index
            readonly property string group: `[Channel${index + 1}]`
            property bool controlCreated: false

            Mixxx.SkinControlCreator {
                group: "[Skin]"
                key: `mobile_loader_deck${deckLoaderAccess.index + 1}`

                Component.onCompleted: deckLoaderAccess.controlCreated = true
            }
            Mixxx.ControlProxy {
                group: "[Skin]"
                key: deckLoaderAccess.controlCreated ? `mobile_loader_deck${deckLoaderAccess.index + 1}` : ""
                onValueChanged: value => {
                    if (value > 0) {
                        root.openQuickLoad(deckLoaderAccess.group);
                    }
                }
            }
            Mixxx.ControlProxy {
                group: deckLoaderAccess.group
                key: "LoadSelectedTrack"
                onValueChanged: value => {
                    if (value <= 0) {
                        return;
                    }
                    if (quickLoadDrawer.opened) {
                        Qt.callLater(quickLoadDrawer.close);
                    } else if (!library.visible) {
                        Qt.callLater(root.openQuickLoad, deckLoaderAccess.group);
                    }
                }
            }
        }
    }
}
