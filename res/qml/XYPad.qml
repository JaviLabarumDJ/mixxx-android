import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Mixxx 1.0 as Mixxx
import "." as Skin
import "Theme"

Rectangle {
    id: root

    property string targetDeck: "[Channel1]"
    property bool holdMode: false
    property real currentX: 0.5 // 0.0 (LPF) ... 0.5 (Neutral) ... 1.0 (HPF)
    property real currentY: 0.0 // 0.0 (Dry) ... 1.0 (Wet)
    property bool isTouching: false

    signal closeRequested()

    color: "#e60a0d14"
    radius: 12
    border.color: "#00e5ff66"
    border.width: 1

    Mixxx.ControlProxy {
        id: filterProxy
        group: "[QuickEffectRack1_" + root.targetDeck + "]"
        key: "super1"
    }
    Mixxx.ControlProxy {
        id: filterDeck1Proxy
        group: "[QuickEffectRack1_[Channel1]]"
        key: "super1"
    }
    Mixxx.ControlProxy {
        id: filterDeck2Proxy
        group: "[QuickEffectRack1_[Channel2]]"
        key: "super1"
    }

    Mixxx.ControlProxy {
        id: fxMixProxy
        group: "[EffectRack1_EffectUnit1]"
        key: "mix"
    }

    // One proxy per deck, so the routing of the previous deck can be
    // restored after the target deck changed
    Mixxx.ControlProxy {
        id: routingDeck1Proxy
        group: "[EffectRack1_EffectUnit1]"
        key: "group_[Channel1]_enable"
    }
    Mixxx.ControlProxy {
        id: routingDeck2Proxy
        group: "[EffectRack1_EffectUnit1]"
        key: "group_[Channel2]_enable"
    }

    function filterProxyFor(deck) {
        return deck === "[Channel2]" ? filterDeck2Proxy : filterDeck1Proxy;
    }

    function routingProxyFor(deck) {
        return deck === "[Channel2]" ? routingDeck2Proxy : routingDeck1Proxy;
    }

    Mixxx.ControlProxy {
        id: slot1EnabledProxy
        group: "[EffectRack1_EffectUnit1_Effect1]"
        key: "enabled"
    }
    Mixxx.ControlProxy {
        id: slot2EnabledProxy
        group: "[EffectRack1_EffectUnit1_Effect2]"
        key: "enabled"
    }
    Mixxx.ControlProxy {
        id: slot3EnabledProxy
        group: "[EffectRack1_EffectUnit1_Effect3]"
        key: "enabled"
    }

    // State before the pad was touched, restored in SNAP mode
    property bool hasSavedState: false
    property real savedFilter: 0.5
    property real savedMix: 0.0
    property real savedRouting: 0
    property string savedDeck: ""
    property bool enabledSlotForPad: false

    function saveState() {
        if (root.hasSavedState) {
            return;
        }
        root.savedFilter = filterProxy.value;
        root.savedMix = fxMixProxy.value;
        root.savedDeck = root.targetDeck;
        root.savedRouting = routingProxyFor(root.savedDeck).value;
        // The pad must be audible: route the effect unit to the deck and
        // switch on its first effect if none is on
        routingProxyFor(root.savedDeck).value = 1;
        root.enabledSlotForPad = !slot1EnabledProxy.value && !slot2EnabledProxy.value && !slot3EnabledProxy.value;
        if (root.enabledSlotForPad) {
            slot1EnabledProxy.value = 1;
        }
        root.hasSavedState = true;
    }

    function restoreState() {
        if (!root.hasSavedState) {
            return;
        }
        filterProxyFor(root.savedDeck).value = root.savedFilter;
        fxMixProxy.value = root.savedMix;
        routingProxyFor(root.savedDeck).value = root.savedRouting;
        if (root.enabledSlotForPad) {
            slot1EnabledProxy.value = 0;
        }
        root.hasSavedState = false;
    }

    function applyValues(nx, ny) {
        root.currentX = Math.max(0.0, Math.min(1.0, nx));
        root.currentY = Math.max(0.0, Math.min(1.0, ny));

        filterProxy.value = root.currentX;
        // Only the dry/wet mix: super1 drives the linked effect parameters
        // and the controller's own knobs
        fxMixProxy.value = root.currentY;
    }

    function resetNeutral() {
        if (root.hasSavedState) {
            restoreState();
        } else {
            // Neutral filter, effect off
            filterProxy.value = 0.5;
            fxMixProxy.value = 0.0;
        }
        root.currentX = filterProxy.value;
        root.currentY = fxMixProxy.value;
    }

    onTargetDeckChanged: {
        // The saved state belongs to the previous deck
        restoreState();
    }

    onVisibleChanged: {
        if (visible) {
            root.currentX = filterProxy.value;
            root.currentY = fxMixProxy.value;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // ================= HEADER =================
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("⚡ XY TOUCH PAD")
                font.bold: true
                font.pixelSize: 13
                font.letterSpacing: 1
                color: "#00e5ff"
            }

            Item { Layout.fillWidth: true }

            // Deck 1 / Deck 2 Selector
            Rectangle {
                Layout.preferredHeight: 28
                Layout.preferredWidth: 64
                radius: 4
                color: root.targetDeck === "[Channel1]" ? "#00b0ff" : "#1a222d"
                border.color: root.targetDeck === "[Channel1]" ? "#ffffff" : "#303e4f"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "DECK 1"
                    font.bold: true
                    font.pixelSize: 10
                    color: root.targetDeck === "[Channel1]" ? "#001018" : "#90a4ae"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.targetDeck = "[Channel1]"
                }
            }

            Rectangle {
                Layout.preferredHeight: 28
                Layout.preferredWidth: 64
                radius: 4
                color: root.targetDeck === "[Channel2]" ? "#ff6d00" : "#1a222d"
                border.color: root.targetDeck === "[Channel2]" ? "#ffffff" : "#303e4f"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "DECK 2"
                    font.bold: true
                    font.pixelSize: 10
                    color: root.targetDeck === "[Channel2]" ? "#000000" : "#90a4ae"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.targetDeck = "[Channel2]"
                }
            }

            // Hold Toggle Button
            Rectangle {
                Layout.preferredHeight: 28
                Layout.preferredWidth: 62
                radius: 4
                color: root.holdMode ? "#ab47bc" : "#1a222d"
                border.color: root.holdMode ? "#ffffff" : "#303e4f"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: root.holdMode ? "🔒 HOLD" : "SNAP"
                    font.bold: true
                    font.pixelSize: 10
                    color: root.holdMode ? "#ffffff" : "#90a4ae"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.holdMode = !root.holdMode
                }
            }

            // Reset Button
            Rectangle {
                Layout.preferredHeight: 28
                Layout.preferredWidth: 54
                radius: 4
                color: resetMouse.pressed ? "#455a64" : "#263238"
                border.color: "#455a64"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: qsTr("RESET")
                    font.bold: true
                    font.pixelSize: 10
                    color: "#cfd8dc"
                }

                MouseArea {
                    id: resetMouse
                    anchors.fill: parent
                    onClicked: root.resetNeutral()
                }
            }

            // Close Button
            Rectangle {
                Layout.preferredHeight: 28
                Layout.preferredWidth: 28
                radius: 14
                color: closeMouse.pressed ? "#b71c1c" : "#374151"

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    font.bold: true
                    font.pixelSize: 12
                    color: "#ffffff"
                }

                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    onClicked: {
                        if (!root.holdMode) {
                            root.resetNeutral();
                        }
                        root.closeRequested();
                    }
                }
            }
        }

        // ================= 2D TOUCH PAD SURFACE =================
        Rectangle {
            id: touchSurface
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: "#080b10"
            border.color: root.isTouching ? "#00e5ff" : "#1f2937"
            border.width: 1
            clip: true

            // Grid lines
            Rectangle {
                // Vertical Center (Neutral Filter)
                x: touchSurface.width * 0.5 - 0.5
                y: 0
                width: 1
                height: touchSurface.height
                color: "#00e5ff33"
            }

            Rectangle {
                // Horizontal Center
                x: 0
                y: touchSurface.height * 0.5 - 0.5
                width: touchSurface.width
                height: 1
                color: "#ffffff15"
            }

            // Subtle quadrant cross lines
            Repeater {
                model: 3
                Rectangle {
                    x: touchSurface.width * ((index + 1) * 0.25)
                    y: 0
                    width: 1
                    height: touchSurface.height
                    color: "#ffffff08"
                    visible: index !== 1
                }
            }

            // Axis Labels
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("◀ LOW PASS")
                font.bold: true
                font.pixelSize: 11
                color: "#00b0ff66"
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("HIGH PASS ▶")
                font.bold: true
                font.pixelSize: 11
                color: "#ff6d0066"
            }

            Text {
                anchors.top: parent.top
                anchors.topMargin: 8
                anchors.horizontalCenter: parent.horizontalCenter
                text: "▲ FX WET (100%)"
                font.bold: true
                font.pixelSize: 10
                color: "#e040fb88"
            }

            Text {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                anchors.horizontalCenter: parent.horizontalCenter
                text: "▼ FX DRY (0%)"
                font.bold: true
                font.pixelSize: 10
                color: "#90a4ae66"
            }

            // Current Coordinates Crosshair lines
            Rectangle {
                x: Math.round(root.currentX * touchSurface.width) - 0.5
                y: 0
                width: 1
                height: touchSurface.height
                color: root.isTouching ? "#00e5ff" : "#00e5ff66"
            }

            Rectangle {
                x: 0
                y: Math.round((1.0 - root.currentY) * touchSurface.height) - 0.5
                width: touchSurface.width
                height: 1
                color: root.isTouching ? "#e040fb" : "#e040fb66"
            }

            // Glowing Touch Puck
            Rectangle {
                id: puckGlow
                x: (root.currentX * touchSurface.width) - (width / 2)
                y: ((1.0 - root.currentY) * touchSurface.height) - (height / 2)
                width: root.isTouching ? 44 : 32
                height: width
                radius: width / 2
                color: "#00e5ff33"
                border.color: "#00e5ff"
                border.width: 2

                Behavior on width {
                    NumberAnimation { duration: 80 }
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: 12
                    radius: 6
                    color: root.isTouching ? "#ffffff" : "#00e5ff"
                }
            }

            // Touch Handler
            MultiPointTouchArea {
                anchors.fill: parent
                mouseEnabled: true

                onPressed: touchPoints => {
                    root.isTouching = true;
                    snapBackAnim.stop();
                    root.saveState();
                    if (touchPoints.length > 0) {
                        var pt = touchPoints[0];
                        var nx = pt.x / touchSurface.width;
                        var ny = 1.0 - (pt.y / touchSurface.height);
                        root.applyValues(nx, ny);
                    }
                }

                onTouchUpdated: touchPoints => {
                    if (touchPoints.length > 0) {
                        var pt = touchPoints[0];
                        var nx = pt.x / touchSurface.width;
                        var ny = 1.0 - (pt.y / touchSurface.height);
                        root.applyValues(nx, ny);
                    }
                }

                onReleased: {
                    root.isTouching = false;
                    if (!root.holdMode) {
                        // Animate the cursor back, then restore exactly the
                        // values from before the touch
                        snapBackAnimX.to = root.hasSavedState ? root.savedFilter : 0.5;
                        snapBackAnimY.to = root.hasSavedState ? root.savedMix : 0.0;
                        snapBackAnim.restart();
                    }
                }
            }

            ParallelAnimation {
                id: snapBackAnim
                NumberAnimation {
                    id: snapBackAnimX
                    target: root
                    property: "currentX"
                    duration: 180
                    easing.type: Easing.OutQuad
                }
                NumberAnimation {
                    id: snapBackAnimY
                    target: root
                    property: "currentY"
                    duration: 180
                    easing.type: Easing.OutQuad
                }
                onFinished: {
                    root.restoreState();
                }
            }
        }

        // ================= BOTTOM STATUS READOUT =================
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: {
                    var f = (root.currentX - 0.5) * 200;
                    if (Math.abs(f) < 2) return qsTr("FILTER: NEUTRAL");
                    if (f < 0) return qsTr("FILTER: LPF %1%").arg(Math.round(Math.abs(f)));
                    return qsTr("FILTER: HPF %1%").arg(Math.round(f));
                }
                font.bold: true
                font.pixelSize: 11
                color: root.currentX < 0.48 ? "#00b0ff" : (root.currentX > 0.52 ? "#ff6d00" : "#90a4ae")
            }

            Item { Layout.fillWidth: true }

            Text {
                text: qsTr("FX DEPTH: %1%").arg(Math.round(root.currentY * 100))
                font.bold: true
                font.pixelSize: 11
                color: root.currentY > 0.05 ? "#e040fb" : "#90a4ae"
            }
        }
    }
}
