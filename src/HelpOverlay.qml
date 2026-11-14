import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: overlay

    property color accent: theme.accent
    property color dim: "#9a9a9e"
    property color faint: "#6c6c72"
    property color line: "#26262b"

    signal closed()

    z: 100

    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: overlay.visible
        onActivated: overlay.closed()
    }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.62
        TapHandler { onTapped: overlay.closed() }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(overlay.width - 80, 470)
        height: body.implicitHeight + 48
        color: "#18181b"
        radius: Math.min(8, theme.radius)
        border.color: overlay.line

        ColumnLayout {
            id: body
            anchors.fill: parent
            anchors.margins: 24
            spacing: 14

            Text {
                text: qsTr("Shortcuts")
                color: "#f1f1f2"
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }

            Repeater {
                model: {
                    translator.language
                    return [
                        { keys: "Space", text: qsTr("Start or pause the timer") },
                        { keys: "S", text: qsTr("Stop and log the session") },
                        { keys: "1–5", text: qsTr("Choose the mode") },
                        { keys: "N", text: qsTr("Capture a new idea") },
                        { keys: "P", text: qsTr("Next project") },
                        { keys: "?", text: qsTr("Show these shortcuts") },
                        { keys: "Q", text: qsTr("Quit") }
                    ]
                }
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 18

                    Text {
                        Layout.preferredWidth: 74
                        text: modelData.keys
                        color: overlay.accent
                        font.pixelSize: 12
                        font.family: "monospace"
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.text
                        color: "#d9d9dc"
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Double-click a project to open its note, or an idea to open it in Obsidian.")
                color: overlay.faint
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }
}
