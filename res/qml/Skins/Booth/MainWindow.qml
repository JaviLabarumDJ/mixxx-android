import "../.." as Root
import QtQuick
import QtQuick.Controls

// A skin is a folder in res/qml/Skins with a MainWindow.qml whose root item
// takes the application window, like res/qml/MainWindow.qml ("Classic").
// main.qml loads the skin selected in Settings > Interface.
//
// Placeholder of the booth skin while it is being built: the Classic
// layout with a badge.
Item {
    id: root

    required property ApplicationWindow applicationWindow
    property alias menuBar: classic.menuBar

    function openQuickLoad(group) {
        classic.openQuickLoad(group);
    }

    Root.MainWindow {
        id: classic

        anchors.fill: parent
        applicationWindow: root.applicationWindow
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        anchors.horizontalCenter: parent.horizontalCenter
        color: "#cc000000"
        height: badge.implicitHeight + 8
        radius: 4
        width: badge.implicitWidth + 16
        z: 1000

        Text {
            id: badge

            anchors.centerIn: parent
            color: "#ffab40"
            font.bold: true
            font.pixelSize: 11
            text: qsTr("Booth skin – preview, work in progress")
        }
    }
}
