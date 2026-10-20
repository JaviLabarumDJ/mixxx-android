import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import Mixxx 1.0 as Mixxx
import "." as Setting
import ".." as Skin
import "../Theme"

Category {
    id: root

    label: "Acerca de"
    selectedIndex: 0

    Flickable {
        anchors.fill: parent
        contentHeight: aboutColumn.implicitHeight + 30
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: aboutColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 12
            spacing: 12

            // ================= HERO CARD: JAVI & IDENTIDAD =================
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 110
                radius: 8
                color: "#161b24"
                border.color: "#2a3648"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 14

                    // Avatar with neon glow border
                    Item {
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 80
                        Layout.alignment: Qt.AlignVCenter

                        Rectangle {
                            id: avatarContainer
                            anchors.fill: parent
                            radius: 40
                            color: "#0d1117"
                            border.color: "#00e5ff"
                            border.width: 2
                            clip: true

                            Image {
                                anchors.fill: parent
                                source: Qt.resolvedUrl("../images/javi_headphones.jpg")
                                fillMode: Image.PreserveAspectCrop
                                antialiasing: true
                            }
                        }
                    }

                    // Bio Header
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 3

                        RowLayout {
                            spacing: 8
                            Text {
                                text: "JaviLabarum"
                                font.bold: true
                                font.pixelSize: 18
                                color: "#ffffff"
                            }
                            Rectangle {
                                color: "#00b0ff22"
                                border.color: "#00b0ff"
                                border.width: 1
                                radius: 4
                                height: 18
                                width: 56
                                Text {
                                    anchors.centerIn: parent
                                    text: "DJ / CREATOR"
                                    font.pixelSize: 7
                                    font.bold: true
                                    color: "#00e5ff"
                                }
                            }
                        }

                        Text {
                            text: "@javilabarumdj"
                            font.bold: true
                            font.pixelSize: 12
                            color: "#ff9100"
                        }

                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: 10
                            color: "#b0bec5"
                            lineHeight: 1.2
                            text: "DJ, productor y apasionado del audio digital. Llevando la experiencia de mezcla profesional y tactilidad real a dispositivos Android."
                        }
                    }
                }
            }

            // ================= SECTION 1: ¿POR QUÉ ESTA APLICACIÓN? =================
            Rectangle {
                Layout.fillWidth: true
                radius: 8
                color: "#14171f"
                border.color: "#232b38"
                border.width: 1
                implicitHeight: whyCol.implicitHeight + 20

                ColumnLayout {
                    id: whyCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    RowLayout {
                        spacing: 8
                        Text {
                            text: "🎧"
                            font.pixelSize: 15
                        }
                        Text {
                            text: "¿POR QUÉ ESTA APLICACIÓN?"
                            font.bold: true
                            font.pixelSize: 12
                            color: "#00e5ff"
                            font.letterSpacing: 0.5
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#232b38"
                    }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 11
                        lineHeight: 1.3
                        color: "#cfd8dc"
                        text: "Durante mucho tiempo, las opciones de software DJ en móviles han estado limitadas por suscripciones caras, latencias inaceptables o interfaces poco profesionales.\n\n" +
                              "Esta edición de Mixxx para Android nace de una necesidad personal como DJ: tener una cabina completa, libre, potente y ultra-estable en el bolsillo.\n\n" +
                              "• Motor de audio profesional en tiempo real C++ sin cortes.\n" +
                              "• Soporte para controladoras DJ USB externas (plug & play por OTG).\n" +
                              "• Doble deck táctil con faders de pitch independientes, beatloops, hotcues y EQ completa.\n" +
                              "• Creada de forma independiente con pasión por la música y para la comunidad DJ."
                    }
                }
            }

            // ================= SECTION 2: SÍGUEME EN REDES SOCIALES =================
            Rectangle {
                Layout.fillWidth: true
                radius: 8
                color: "#14171f"
                border.color: "#232b38"
                border.width: 1
                implicitHeight: rrssCol.implicitHeight + 20

                ColumnLayout {
                    id: rrssCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        spacing: 8
                        Text {
                            text: "🚀"
                            font.pixelSize: 15
                        }
                        Text {
                            text: "SÍGUEME EN MIS REDES SOCIALES"
                            font.bold: true
                            font.pixelSize: 12
                            color: "#ff9100"
                            font.letterSpacing: 0.5
                        }
                    }

                    Text {
                        text: "Toca cualquier red para abrir el perfil y ver sesiones, trucos, novedades y actualizaciones:"
                        font.pixelSize: 10
                        color: "#90a4ae"
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        // Instagram
                        SocialButton {
                            Layout.fillWidth: true
                            title: "Instagram"
                            handle: "@javilabarumdj"
                            accentColor: "#e1306c"
                            url: "https://www.instagram.com/javilabarumdj"
                        }

                        // TikTok
                        SocialButton {
                            Layout.fillWidth: true
                            title: "TikTok"
                            handle: "@javilabarumdj"
                            accentColor: "#00f2fe"
                            url: "https://www.tiktok.com/@javilabarumdj"
                        }

                        // YouTube
                        SocialButton {
                            Layout.fillWidth: true
                            title: "YouTube"
                            handle: "@javilabarumdj"
                            accentColor: "#ff0000"
                            url: "https://www.youtube.com/@javilabarumdj"
                        }
                    }
                }
            }

            // ================= SECTION 3: APP INFO & VERSION =================
            Rectangle {
                Layout.fillWidth: true
                radius: 8
                color: "#14171f"
                border.color: "#232b38"
                border.width: 1
                implicitHeight: infoCol.implicitHeight + 16

                ColumnLayout {
                    id: infoCol
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Mixxx Android DJ Edition"
                            font.bold: true
                            font.pixelSize: 11
                            color: "#ffffff"
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "v2.6-mobile (ARM64)"
                            font.pixelSize: 10
                            color: "#78909c"
                        }
                    }

                    Text {
                        text: "Basado en el proyecto Open Source Mixxx. Adaptación móvil por JaviLabarum."
                        font.pixelSize: 9
                        color: "#546e7a"
                    }
                }
            }
        }
    }

    component SocialButton: Rectangle {
        id: sBtn
        property string title
        property string handle
        property color accentColor
        property string url

        height: 50
        radius: 6
        color: sMouse.pressed ? "#232b38" : "#1a202c"
        border.color: sMouse.containsMouse ? accentColor : "#2d3748"
        border.width: 1.5

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 2
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: sBtn.title
                font.bold: true
                font.pixelSize: 11
                color: sBtn.accentColor
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: sBtn.handle
                font.pixelSize: 9
                color: "#e2e8f0"
            }
        }

        MouseArea {
            id: sMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                Qt.openUrlExternally(sBtn.url);
            }
        }
    }
}
