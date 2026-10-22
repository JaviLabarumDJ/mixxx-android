import ".." as Skin
import "." as LibraryComponent
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls 2.15
import "../Theme"

Rectangle {
    id: root

    required property var model

    color: Theme.darkGray

    // Shows that the controller's browse knob moves through this list
    property bool highlighted: false

    function focusList() {
        view.forceActiveFocus();
    }
    function loadSelectedTrack(group, play) {
        view.loadSelectedTrack(group, play);
    }
    function loadSelectedTrackIntoNextAvailableDeck(play) {
        view.loadSelectedTrackIntoNextAvailableDeck(play);
    }
    // Knob turns that arrive together are applied at once, so fast turns
    // don't queue up a relayout per step
    property int pendingMove: 0

    function applyPendingMove() {
        const offset = root.pendingMove;
        root.pendingMove = 0;
        view.selectionModel.moveSelectionVertical(offset);
    }
    function moveSelectionVertical(offset) {
        root.pendingMove += offset;
        Qt.callLater(root.applyPendingMove);
    }

    border.color: Theme.accentColor
    border.width: highlighted ? 2 : 0
    HorizontalHeaderView {
        id: horizontalHeader

        property int sortingColumn: -1
        property var sortingOrder: Qt.Descending

        anchors.left: parent.left
        anchors.margins: 5
        anchors.right: parent.right
        anchors.top: parent.top
        syncView: view

        delegate: Item {
            id: column

            required property string display
            required property int index

            implicitHeight: columnName.contentHeight + 5
            implicitWidth: columnName.contentWidth + 5

            MouseArea {
                id: columnMouseHandler

                acceptedButtons: Qt.LeftButton
                anchors.fill: parent

                onClicked: {
                    if (horizontalHeader.sortingColumn == index) {
                        horizontalHeader.sortingOrder = horizontalHeader.sortingOrder == Qt.DescendingOrder ? Qt.AscendingOrder : Qt.DescendingOrder;
                    } else {
                        horizontalHeader.sortingColumn = index;
                        horizontalHeader.sortingOrder = Qt.AscendingOrder;
                    }
                    view.model.sort(horizontalHeader.sortingColumn, horizontalHeader.sortingOrder);
                }
            }
            Text {
                id: columnName

                anchors.fill: parent
                anchors.leftMargin: 15
                color: Theme.textColor
                elide: Text.ElideRight
                font.capitalization: Font.Capitalize
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignLeft
                text: display
                verticalAlignment: Text.AlignVCenter
            }
            Item {
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    leftMargin: 5
                    top: parent.top
                }
                Label {
                    id: sortIndicator

                    anchors.centerIn: parent
                    color: "red"
                    elide: Text.ElideRight
                    font.bold: true
                    font.capitalization: Font.AllUppercase
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.buttonFontPixelSize
                    horizontalAlignment: Text.AlignRight
                    rotation: horizontalHeader.sortingOrder == Qt.AscendingOrder ? 90 : -90
                    text: "▶"
                    verticalAlignment: Text.AlignVCenter
                    visible: horizontalHeader.sortingColumn == index
                }
            }
            Rectangle {
                id: columnResizer

                color: Theme.darkGray2
                width: 1

                anchors {
                    bottom: parent.bottom
                    right: parent.right
                    top: parent.top
                }
                MouseArea {
                    id: columnResizeHandler

                    property int sizeOffset: 0

                    anchors.fill: parent
                    cursorShape: Qt.SizeHorCursor
                    preventStealing: true

                    onMouseXChanged: {
                        if (drag.active) {
                            column.width += mouseX;
                            sizeOffset += mouseX;
                        }
                    }

                    drag {
                        axis: Drag.XAxis
                        target: parent
                        threshold: 2

                        onActiveChanged: {
                            if (!drag.active && columnResizeHandler.sizeOffset !== 0) {
                                view.model.columns[index].preferredWidth = column.width;
                                columnResizeHandler.sizeOffset = 0;
                                view.updateColumnSize();
                                view.forceLayout();
                            }
                        }
                    }
                }
            }
        }
    }
    TableView {
        id: view

        property int dynamicColumnCount: 0
        property int usedWidth: 0

        function loadSelectedTrack(group, play) {
            const urls = this.selectionModel.selectedTrackUrls();
            if (urls.length == 0)
                return;

            Mixxx.PlayerManager.getPlayer(group).loadTrackFromLocationUrl(urls[0], play);
        }
        function loadSelectedTrackIntoNextAvailableDeck(play) {
            const urls = this.selectionModel.selectedTrackUrls();
            if (urls.length == 0)
                return;

            Mixxx.PlayerManager.loadLocationUrlIntoNextAvailableDeck(urls[0], play);
        }
        function updateColumnSize() {
            const oldUsedWidth = usedWidth;
            const oldDynamicColumnCount = dynamicColumnCount;
            usedWidth = 0;
            dynamicColumnCount = 0;
            if (model == null) {
                return;
            }
            for (let c = 0; c < model.columns.length; c++) {
                if (model.columns[c].hidden || model.columns[c].autoHideWidth > view.width) {
                    continue;
                } else if (model.columns[c].preferredWidth > 0) {
                    usedWidth += model.columns[c].preferredWidth;
                } else {
                    dynamicColumnCount += model.columns[c].fillSpan || 1;
                }
            }
            return oldDynamicColumnCount != dynamicColumnCount || oldUsedWidth != usedWidth;
        }

        anchors.bottom: quickLoadBar.top
        anchors.left: parent.left
        anchors.margins: 5
        anchors.right: parent.right
        anchors.top: horizontalHeader.bottom
        clip: true
        columnWidthProvider: function (column) {
            const columnDef = view.model.columns[column];
            if (columnDef.hidden) {
                return 0;
            }
            if (columnDef.autoHideWidth > 0 && columnDef.autoHideWidth > view.width) {
                return 0;
            }
            if (columnDef.preferredWidth >= 0) {
                return columnDef.preferredWidth;
            }
            const span = columnDef.fillSpan || 1;
            return span * (view.width - view.usedWidth) / view.dynamicColumnCount;
        }
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        interactive: true
        keyNavigationEnabled: false
        model: root.model
        pointerNavigationEnabled: false
        reuseItems: true

        ScrollBar.vertical: ScrollBar {
            id: verticalScrollBar
            policy: ScrollBar.AlwaysOn
            width: 16
            interactive: true
            contentItem: Rectangle {
                implicitWidth: 12
                implicitHeight: 48
                radius: 6
                color: verticalScrollBar.pressed ? "#00e5ff" : (verticalScrollBar.hovered ? "#78909c" : "#546e7a")
            }
            background: Rectangle {
                implicitWidth: 16
                color: "#181a20"
                opacity: 0.7
                radius: 4
            }
        }
        delegate: Item {
            id: item

            required property url cover_art
            required property color decoration
            required property var display
            required property string file_url
            required property int row
            required property bool selected

            implicitHeight: 38

            Loader {
                id: loader

                property var capabilities: root.model ? root.model.getCapabilities() : Mixxx.LibraryTrackListModel.Capability.None
                property url cover_art: item.cover_art
                property color decoration: item.decoration
                property var display: item.display
                property url file_url: item.file_url
                property int row: item.row
                property bool selected: item.selected
                property var tableView: view

                anchors.fill: parent
                focus: true
                sourceComponent: delegate

                onLoaded:
                // Workaround needed for WaveformOverview column to load the data
                //     if (track)
                //         Mixxx.Library.analyze(track)
                {}
            }
            // Workaround needed for WaveformOverview column to load the data
            // TableView.onReused: {
            //     if (track)
            //         Mixxx.Library.analyze(track)
            // }
        }
        Timer {
            id: relayoutTimer

            interval: 150
            onTriggered: view.forceLayout()
        }

        selectionModel: ItemSelectionModel {
            function moveSelectionVertical(value) {
                if (value == 0)
                    return;

                const selected = this.selectedIndexes;
                const oldRow = (selected.length == 0) ? 0 : selected[0].row;
                this.selectRow(oldRow + value);
            }
            function selectRow(row) {
                const rowCount = this.model.rowCount();
                if (rowCount == 0) {
                    this.clear();
                    return;
                }
                // Stay at the first or last track instead of wrapping around
                const newRow = Math.max(0, Math.min(rowCount - 1, row));
                this.select(this.model.index(newRow, 0), ItemSelectionModel.Rows | ItemSelectionModel.Select | ItemSelectionModel.Clear | ItemSelectionModel.Current);
                // Keep the selection visible, e.g. when moving it with the controller
                view.positionViewAtRow(newRow, TableView.Contain);
            }
            function selectedTrackUrls() {
                return this.selectedIndexes.map(index => {
                    return this.model.getUrl(index.row);
                });
            }

            model: view.model
        }

        Component.onCompleted: this.updateColumnSize()
        Keys.onDownPressed: root.moveSelectionVertical(1)
        Keys.onEnterPressed: this.loadSelectedTrackIntoNextAvailableDeck(false)
        Keys.onReturnPressed: this.loadSelectedTrackIntoNextAvailableDeck(false)
        Keys.onUpPressed: root.moveSelectionVertical(-1)
        onModelChanged: this.updateColumnSize()
        // The library grows when switching to the LIB view, rows have to be
        // created for the new space once the layout has settled
        onHeightChanged: relayoutTimer.restart()
        onWidthChanged: {
            if (view.updateColumnSize()) {
                // forceLayout is costly - only invoke if there was a change in the column layouts
                view.forceLayout();
            }
        }
    }
    Rectangle {
        id: quickLoadBar

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: Theme.sunkenBackgroundColor
        height: 48
        border.color: Theme.panelSplitterBackground
        border.width: 1

        readonly property var selectedUrls: view.selectionModel.selectedTrackUrls()
        readonly property bool hasTrack: selectedUrls && selectedUrls.length > 0

        RowLayout {
            anchors.fill: parent
            anchors.margins: 5
            spacing: 8

            Rectangle {
                Layout.fillHeight: true
                Layout.fillWidth: true
                color: Theme.darkGray3
                radius: 4

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6

                    Text {
                        Layout.fillWidth: true
                        color: quickLoadBar.hasTrack ? Theme.white : Theme.lightGray3
                        elide: Text.ElideRight
                        font.bold: quickLoadBar.hasTrack
                        font.pixelSize: 13
                        text: {
                            if (!quickLoadBar.hasTrack)
                                return qsTr("Select a track to load");
                            const urlStr = quickLoadBar.selectedUrls[0].toString();
                            const filename = decodeURIComponent(urlStr.substring(urlStr.lastIndexOf('/') + 1));
                            return filename;
                        }
                    }
                }
            }

            Skin.Button {
                id: addToPlaylistBtn

                Layout.preferredHeight: 38
                Layout.preferredWidth: 110
                activeColor: Theme.accentColor
                backgroundColor: quickLoadBar.hasTrack ? "#2e3440" : "#262930"
                normalColor: quickLoadBar.hasTrack ? "#ffffff" : "#78909c"
                enabled: quickLoadBar.hasTrack
                font.bold: true
                text: "+ PLAYLIST"

                onClicked: playlistPopup.openFor(quickLoadBar.selectedUrls[0])
            }

            Skin.Button {
                id: loadDeck1Btn

                Layout.preferredHeight: 38
                Layout.preferredWidth: 115
                activeColor: "#64b5f6"
                backgroundColor: quickLoadBar.hasTrack ? "#1565c0" : "#262930"
                normalColor: quickLoadBar.hasTrack ? "#ffffff" : "#78909c"
                highlight: quickLoadBar.hasTrack
                enabled: quickLoadBar.hasTrack
                font.bold: true
                text: "◀ DECK 1"

                onClicked: {
                    view.loadSelectedTrack("[Channel1]", false);
                }
            }

            Skin.Button {
                id: loadDeck2Btn

                Layout.preferredHeight: 38
                Layout.preferredWidth: 115
                activeColor: "#ffb74d"
                backgroundColor: quickLoadBar.hasTrack ? "#e65100" : "#262930"
                normalColor: quickLoadBar.hasTrack ? "#ffffff" : "#78909c"
                highlight: quickLoadBar.hasTrack
                enabled: quickLoadBar.hasTrack
                font.bold: true
                text: "DECK 2 ▶"

                onClicked: {
                    view.loadSelectedTrack("[Channel2]", false);
                }
            }
        }
    }

    Popup {
        id: playlistPopup

        property url trackUrl
        property var playlists: []
        property string message: ""

        function openFor(url) {
            trackUrl = url;
            message = "";
            newPlaylistName.text = "";
            playlists = Mixxx.MobileLibrary.playlists();
            open();
        }
        function addTo(playlistId, name) {
            if (Mixxx.MobileLibrary.addTrackToPlaylist(playlistId, trackUrl)) {
                close();
            } else {
                message = qsTr("Could not add to %1").arg(name);
            }
        }

        anchors.centerIn: parent
        width: Math.min(380, root.width - 20)
        height: Math.min(440, root.height - 20)
        modal: true
        focus: true
        padding: 10

        background: Rectangle {
            color: Theme.darkGray2
            border.color: Theme.darkGray3
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 8

            Text {
                Layout.fillWidth: true
                color: Theme.white
                font.bold: true
                font.pixelSize: 15
                text: qsTr("Add to playlist")
            }
            ListView {
                Layout.fillHeight: true
                Layout.fillWidth: true
                clip: true
                model: playlistPopup.playlists
                spacing: 4

                delegate: Skin.Button {
                    required property var modelData

                    width: ListView.view.width
                    height: 44
                    enabled: !modelData.locked
                    backgroundColor: "#262930"
                    normalColor: Theme.white
                    activeColor: Theme.accentColor
                    font.pixelSize: 14
                    text: modelData.locked ? qsTr("%1 (bloqueada)").arg(modelData.name) : modelData.name
                    onClicked: playlistPopup.addTo(modelData.id, modelData.name)
                }

                Text {
                    anchors.centerIn: parent
                    visible: playlistPopup.playlists.length === 0
                    color: Theme.lightGray2
                    font.pixelSize: 13
                    text: qsTr("You have no playlists yet")
                }
            }
            Text {
                Layout.fillWidth: true
                visible: playlistPopup.message.length > 0
                color: Theme.red
                font.pixelSize: 12
                text: playlistPopup.message
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Skin.TextField {
                    id: newPlaylistName

                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    font.pixelSize: 14
                    placeholderText: qsTr("New playlist")
                    onAccepted: createPlaylistBtn.clicked()
                }
                Skin.Button {
                    id: createPlaylistBtn

                    Layout.preferredHeight: 44
                    Layout.preferredWidth: 90
                    enabled: newPlaylistName.text.trim().length > 0
                    activeColor: Theme.accentColor
                    backgroundColor: "#1565c0"
                    normalColor: "#ffffff"
                    font.bold: true
                    text: qsTr("CREATE")

                    onClicked: {
                        const name = newPlaylistName.text.trim();
                        const playlistId = Mixxx.MobileLibrary.createNamedPlaylist(name);
                        if (playlistId < 0) {
                            playlistPopup.message = qsTr("A playlist named %1 already exists").arg(name);
                            return;
                        }
                        playlistPopup.addTo(playlistId, name);
                    }
                }
            }
        }
    }
}
