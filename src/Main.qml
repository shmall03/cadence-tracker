import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1180
    height: 800
    minimumWidth: 980
    minimumHeight: 660
    visible: true
    title: "cadence"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.primary: theme.accent
    Material.roundedScale: theme.radius

    property color accent: theme.accent
    property color dim: "#9a9a9e"
    property color faint: "#6c6c72"
    property color panel: "#151517"
    property color line: "#26262b"
    property bool helpVisible: false
    property bool typing: false

    function tint(alpha) {
        return Qt.rgba(accent.r, accent.g, accent.b, alpha)
    }

    function fmt(minutes) {
        translator.language
        if (minutes < 60)
            return qsTr("%1 min").arg(minutes.toLocaleString(Qt.locale(), 'f', 0))
        return qsTr("%1 h").arg((minutes / 60).toLocaleString(Qt.locale(), 'f', 1))
    }

    function clock(seconds) {
        var total = Math.max(0, Math.floor(seconds))
        var h = Math.floor(total / 3600)
        var m = Math.floor(total % 3600 / 60)
        var s = total % 60
        function pad(n) { return n < 10 ? "0" + n : "" + n }
        return pad(h) + ":" + pad(m) + ":" + pad(s)
    }

    function dayLabel(iso) {
        translator.language
        var today = new Date()
        var start = new Date(today.getFullYear(), today.getMonth(), today.getDate())
        var day = new Date(iso + "T00:00:00")
        var diff = Math.round((start - day) / 86400000)
        if (diff === 0)
            return qsTr("Today")
        if (diff === 1)
            return qsTr("Yesterday")
        return Qt.locale().toString(day, "d MMM")
    }

    function requestQuit() {
        if (backend.pending) {
            logDialog.open()
            return
        }
        if (backend.elapsed > 0 || backend.running)
            quitDialog.open()
        else
            Qt.quit()
    }

    Shortcut {
        sequence: "?"
        context: Qt.ApplicationShortcut
        enabled: !win.typing
        onActivated: win.helpVisible = !win.helpVisible
    }
    Shortcut {
        sequence: "Q"
        context: Qt.ApplicationShortcut
        enabled: !win.typing && !backend.pending
        onActivated: win.requestQuit()
    }
    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        enabled: !win.typing && !win.helpVisible && !backend.pending
        onActivated: backend.startPause()
    }
    Shortcut {
        sequence: "S"
        context: Qt.ApplicationShortcut
        enabled: !win.typing && !win.helpVisible && !backend.pending
        onActivated: backend.stopAndLog()
    }
    Shortcut {
        sequence: "N"
        context: Qt.ApplicationShortcut
        enabled: !win.typing && !win.helpVisible
        onActivated: ideaField.forceActiveFocus()
    }
    Shortcut {
        sequence: "P"
        context: Qt.ApplicationShortcut
        enabled: !win.typing && !win.helpVisible
        onActivated: backend.nextProject()
    }
    Shortcut {
        sequence: "1"; context: Qt.ApplicationShortcut
        enabled: !win.typing && !win.helpVisible
        onActivated: backend.setMode(0)
    }
    Shortcut {
        sequence: "2"; context: Qt.ApplicationShortcut
        enabled: !win.typing && !win.helpVisible
        onActivated: backend.setMode(1)
    }
    Shortcut {
        sequence: "3"; context: Qt.ApplicationShortcut
        enabled: !win.typing && !win.helpVisible
        onActivated: backend.setMode(2)
    }
    Shortcut {
        sequence: "4"; context: Qt.ApplicationShortcut
        enabled: !win.typing && !win.helpVisible
        onActivated: backend.setMode(3)
    }
    Shortcut {
        sequence: "5"; context: Qt.ApplicationShortcut
        enabled: !win.typing && !win.helpVisible
        onActivated: backend.setMode(4)
    }

    onClosing: function(close) {
        if (backend.pending) {
            close.accepted = false
            logDialog.open()
            return
        }
        if (backend.elapsed > 0 || backend.running) {
            close.accepted = false
            quitDialog.open()
        }
    }

    component StatCard: Rectangle {
        id: card
        property string label: ""
        property int minutes: 0

        Layout.fillWidth: true
        Layout.preferredHeight: 62
        color: win.panel
        radius: Math.min(6, theme.radius)
        border.color: win.line

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 3

            Text {
                text: card.label
                color: win.faint
                font.pixelSize: 10
            }
            Text {
                text: win.fmt(card.minutes)
                color: "#f0f0f1"
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---------------------------------------------------------------- projects
        Rectangle {
            Layout.preferredWidth: 250
            Layout.fillHeight: true
            color: win.panel

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Projects")
                        color: "#f0f0f1"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: backend.projects.length.toLocaleString(Qt.locale(), 'f', 0)
                        color: win.faint
                        font.pixelSize: 11
                    }
                }

                ListView {
                    id: projectList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 2
                    model: backend.projects

                    delegate: Rectangle {
                        id: projectRow
                        required property int index
                        required property var modelData

                        width: ListView.view ? ListView.view.width : 0
                        height: 54
                        radius: Math.min(4, theme.radius)
                        color: index === backend.currentProject
                               ? win.tint(0.14)
                               : (rowHover.hovered ? "#1c1c1f" : "transparent")

                        Rectangle {
                            visible: index === backend.currentProject
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.margins: 9
                            width: 2
                            radius: 1
                            color: win.accent
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 10
                            anchors.topMargin: 9
                            anchors.bottomMargin: 9
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                color: index === backend.currentProject ? "#ffffff" : "#d6d6d8"
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                            Text {
                                text: win.fmt(modelData.minutes)
                                color: win.faint
                                font.pixelSize: 11
                            }
                        }

                        HoverHandler { id: rowHover }
                        TapHandler {
                            onSingleTapped: backend.setProject(index)
                            onDoubleTapped: backend.openProjectNote(index)
                        }
                    }
                }

                TextField {
                    id: projectField
                    Layout.fillWidth: true
                    visible: false
                    placeholderText: qsTr("Project name")
                    font.pixelSize: 12
                    onAccepted: {
                        backend.addProject(text)
                        text = ""
                        visible = false
                    }
                    onActiveFocusChanged: win.typing = activeFocus
                    Keys.onEscapePressed: {
                        text = ""
                        visible = false
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: qsTr("Add project")
                    flat: true
                    focusPolicy: Qt.NoFocus
                    onClicked: {
                        projectField.visible = true
                        projectField.forceActiveFocus()
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#1c1c1f" : "transparent"
                        radius: Math.min(4, theme.radius)
                    }
                    contentItem: Text {
                        text: parent.text
                        color: win.dim
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Double-click a project to open its note")
                    color: win.faint
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                    visible: backend.vaultExists
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: win.line
        }

        // ------------------------------------------------------------------ center
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? 44 : 0
                    visible: !backend.vaultExists
                    color: win.tint(0.08)
                    radius: Math.min(5, theme.radius)
                    border.color: win.tint(0.4)

                    Text {
                        anchors.fill: parent
                        anchors.margins: 12
                        text: qsTr("Vault not found at %1. Set CADENCE_VAULT to point cadence at it.").arg(backend.vaultPath)
                        color: "#e6d9b8"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? 44 : 0
                    visible: backend.error !== ""
                    color: "#2a1616"
                    radius: Math.min(5, theme.radius)
                    border.color: "#5a2a2a"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: backend.error
                            color: "#f0c9c9"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            elide: Text.ElideRight
                        }
                        Button {
                            text: qsTr("Dismiss")
                            flat: true
                            focusPolicy: Qt.NoFocus
                            onClicked: backend.clearError()
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? 44 : 0
                    visible: backend.recovered
                    color: win.tint(0.08)
                    radius: Math.min(5, theme.radius)
                    border.color: win.tint(0.4)

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("An unfinished session was recovered.")
                            color: "#e8e3cf"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                        Button {
                            text: qsTr("Dismiss")
                            flat: true
                            focusPolicy: Qt.NoFocus
                            onClicked: backend.dismissRecovered()
                        }
                        Button {
                            text: qsTr("Discard")
                            flat: true
                            focusPolicy: Qt.NoFocus
                            onClicked: backend.discardSession()
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Text {
                            Layout.fillWidth: true
                            text: backend.currentProjectName !== "" ? backend.currentProjectName : qsTr("No projects yet")
                            color: "#f2f2f3"
                            font.pixelSize: 23
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            text: {
                                translator.language
                                return Qt.locale().toString(new Date(), "dddd d MMMM")
                            }
                            color: win.faint
                            font.pixelSize: 11
                        }
                    }

                    Button {
                        visible: backend.currentProjectName !== "" && backend.vaultExists
                        text: qsTr("Open note")
                        flat: true
                        focusPolicy: Qt.NoFocus
                        onClicked: backend.openProjectNote(backend.currentProject)
                        contentItem: Text {
                            text: parent.text
                            color: win.dim
                            font.pixelSize: 11
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Repeater {
                        model: backend.modes

                        delegate: Button {
                            id: modeButton
                            required property int index
                            required property var modelData

                            Layout.preferredHeight: 34
                            padding: 0
                            focusPolicy: Qt.NoFocus
                            onClicked: backend.setMode(index)

                            background: Rectangle {
                                implicitWidth: modeRow.implicitWidth + 26
                                radius: Math.min(4, theme.radius)
                                color: index === backend.currentMode ? win.tint(0.18) : "transparent"
                                border.color: index === backend.currentMode ? win.accent : win.line
                                border.width: 1
                            }
                            contentItem: RowLayout {
                                id: modeRow
                                spacing: 6
                                Text {
                                    text: {
                                        translator.language
                                        return qsTr(modeButton.modelData)
                                    }
                                    color: index === backend.currentMode ? win.accent : win.dim
                                    font.pixelSize: 12
                                }
                                Text {
                                    text: (index + 1)
                                    color: win.faint
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: win.clock(backend.elapsed)
                    color: backend.running ? win.accent : "#f4f4f5"
                    font.pixelSize: 86
                    font.weight: Font.Light
                    font.letterSpacing: 2
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 10

                    Button {
                        id: primaryButton
                        text: backend.running ? qsTr("Pause") : qsTr("Start")
                        Layout.preferredWidth: 140
                        Layout.preferredHeight: 44
                        focusPolicy: Qt.NoFocus
                        onClicked: backend.startPause()
                        background: Rectangle {
                            color: win.accent
                            radius: Math.min(6, theme.radius)
                        }
                        contentItem: Text {
                            text: primaryButton.text
                            color: theme.accentForeground
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Button {
                        text: qsTr("Stop and log")
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 44
                        enabled: backend.elapsed > 0 && !backend.pending
                        focusPolicy: Qt.NoFocus
                        onClicked: backend.stopAndLog()
                        background: Rectangle {
                            color: "transparent"
                            radius: Math.min(6, theme.radius)
                            border.color: win.line
                        }
                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? "#e8e8ea" : win.faint
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Space start or pause · S stop and log")
                    color: win.faint
                    font.pixelSize: 10
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    StatCard { label: qsTr("Today"); minutes: backend.todayMinutes }
                    StatCard { label: qsTr("This week"); minutes: backend.weekMinutes }
                    StatCard { label: qsTr("All time"); minutes: backend.allMinutes }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 104
                    color: win.panel
                    radius: Math.min(6, theme.radius)
                    border.color: win.line

                    Text {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.margins: 12
                        text: qsTr("Last 14 days")
                        color: win.faint
                        font.pixelSize: 10
                    }

                    Item {
                        id: chart
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 12

                        Row {
                            anchors.fill: parent
                            spacing: 6

                            Repeater {
                                model: backend.daily

                                delegate: Item {
                                    required property var modelData

                                    width: (chart.width - 13 * 6) / 14
                                    height: chart.height

                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        anchors.bottomMargin: 16
                                        width: parent.width
                                        height: Math.max(2, (modelData.minutes / backend.dailyMax) * (parent.height - 22))
                                        radius: Math.min(2, theme.radius)
                                        color: modelData.today ? win.accent : win.tint(0.38)
                                    }

                                    Text {
                                        anchors.bottom: parent.bottom
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.day.toLocaleString(Qt.locale(), 'f', 0)
                                        color: modelData.today ? win.dim : win.faint
                                        font.pixelSize: 9
                                    }
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Recent sessions")
                        color: "#e9e9ea"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: qsTr("%n sessions", "", backend.sessions.length)
                        color: win.faint
                        font.pixelSize: 10
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: backend.sessions.length === 0
                    text: qsTr("No sessions yet. Press Space to start.")
                    color: win.faint
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignTop
                }

                ListView {
                    id: sessionList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: backend.sessions.length > 0
                    clip: true
                    spacing: 0
                    model: backend.sessions

                    delegate: Item {
                        required property var modelData

                        width: ListView.view ? ListView.view.width : 0
                        height: 44

                        RowLayout {
                            anchors.fill: parent
                            spacing: 12

                            ColumnLayout {
                                Layout.preferredWidth: 92
                                spacing: 0

                                Text {
                                    text: win.dayLabel(modelData.date)
                                    color: "#d8d8da"
                                    font.pixelSize: 11
                                }
                                Text {
                                    text: modelData.start
                                    color: win.faint
                                    font.pixelSize: 10
                                }
                            }

                            Text {
                                Layout.preferredWidth: 150
                                text: modelData.project
                                color: "#d8d8da"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.preferredWidth: 92
                                text: {
                                    translator.language
                                    return qsTr(modelData.mode)
                                }
                                color: win.dim
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData.note
                                color: win.dim
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.preferredWidth: 64
                                horizontalAlignment: Text.AlignRight
                                text: win.fmt(modelData.minutes)
                                color: win.dim
                                font.pixelSize: 11
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: win.line
                            opacity: 0.6
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: win.line
        }

        // ------------------------------------------------------------------- ideas
        Rectangle {
            Layout.preferredWidth: 320
            Layout.fillHeight: true
            color: win.panel

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Ideas")
                        color: "#f0f0f1"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: backend.ideas.length.toLocaleString(Qt.locale(), 'f', 0)
                        color: win.faint
                        font.pixelSize: 11
                    }
                }

                TextField {
                    id: ideaField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Capture an idea…")
                    font.pixelSize: 12
                    onAccepted: {
                        if (text.trim() !== "") {
                            backend.addIdea(text, "")
                            text = ""
                        }
                    }
                    onActiveFocusChanged: win.typing = activeFocus
                }

                Text {
                    Layout.fillWidth: true
                    visible: backend.ideas.length === 0
                    text: qsTr("No ideas yet. Type one above and press Enter.")
                    color: win.faint
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }

                ListView {
                    id: ideaList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: backend.ideas.length > 0
                    clip: true
                    spacing: 8
                    model: backend.ideas

                    delegate: Rectangle {
                        id: ideaCard
                        required property int index
                        required property var modelData

                        width: ListView.view ? ListView.view.width : 0
                        height: ideaLayout.implicitHeight + 20
                        radius: Math.min(6, theme.radius)
                        color: "#1b1b1e"
                        border.color: cardHover.hovered ? win.tint(0.5) : win.line

                        ColumnLayout {
                            id: ideaLayout
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.title
                                    color: "#ededee"
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    id: chip
                                    property color stageColor: {
                                        if (modelData.stage === "active")
                                            return win.accent
                                        if (modelData.stage === "provisional")
                                            return win.tint(0.55)
                                        return win.dim
                                    }

                                    Layout.preferredHeight: 20
                                    width: chipLabel.implicitWidth + 14
                                    radius: Math.min(4, theme.radius)
                                    color: modelData.stage === "active" ? win.accent : "transparent"
                                    border.color: stageColor
                                    border.width: 1
                                    opacity: modelData.stage === "parked" ? 0.55 : 1

                                    Text {
                                        id: chipLabel
                                        anchors.centerIn: parent
                                        text: {
                                            translator.language
                                            return qsTr(modelData.stage)
                                        }
                                        color: modelData.stage === "active" ? theme.accentForeground : chip.stageColor
                                        font.pixelSize: 9
                                        font.capitalization: Font.AllUppercase
                                    }

                                    TapHandler { onTapped: backend.cycleIdeaStage(index) }
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: modelData.excerpt
                                color: win.dim
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
                                maximumLineCount: 3
                                elide: Text.ElideRight
                            }
                        }

                        HoverHandler { id: cardHover }
                        TapHandler { onDoubleTapped: backend.openIdea(index) }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: backend.ideas.length > 0
                    text: qsTr("Double-click an idea to open it in Obsidian")
                    color: win.faint
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    HelpOverlay {
        anchors.fill: parent
        visible: win.helpVisible
        onClosed: win.helpVisible = false
    }

    Dialog {
        id: logDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 480
        modal: true
        closePolicy: Popup.NoAutoClose
        visible: backend.pending
        padding: 24

        background: Rectangle {
            color: "#17171a"
            radius: Math.min(8, theme.radius)
            border.color: win.line
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: qsTr("What did you do?")
                color: "#f0f0f1"
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: {
                    translator.language
                    var info = backend.pendingInfo
                    return win.fmt(info.minutes) + " · " + info.project + " · " + qsTr(info.mode)
                }
                color: win.dim
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            TextArea {
                id: noteArea
                Layout.fillWidth: true
                Layout.preferredHeight: 90
                placeholderText: qsTr("A sentence for later")
                wrapMode: TextArea.Wrap
                font.pixelSize: 12
                onActiveFocusChanged: win.typing = activeFocus
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return && (event.modifiers & Qt.ControlModifier)) {
                        backend.savePending(noteArea.text)
                        event.accepted = true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Discard")
                    flat: true
                    focusPolicy: Qt.NoFocus
                    onClicked: backend.discardPending()
                }
                Button {
                    text: qsTr("Save")
                    highlighted: true
                    focusPolicy: Qt.NoFocus
                    onClicked: backend.savePending(noteArea.text)
                }
            }
        }

        onOpened: noteArea.forceActiveFocus()
        onClosed: noteArea.text = ""
    }

    Dialog {
        id: quitDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 420
        modal: true
        padding: 24

        background: Rectangle {
            color: "#17171a"
            radius: Math.min(8, theme.radius)
            border.color: win.line
        }

        contentItem: ColumnLayout {
            spacing: 16

            Text {
                text: qsTr("Discard the running session?")
                color: "#f0f0f1"
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("The time will not be added to the log.")
                color: win.dim
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Cancel")
                    flat: true
                    focusPolicy: Qt.NoFocus
                    onClicked: quitDialog.close()
                }
                Button {
                    text: qsTr("Discard and quit")
                    highlighted: true
                    focusPolicy: Qt.NoFocus
                    onClicked: {
                        backend.discardSession()
                        Qt.quit()
                    }
                }
            }
        }
    }
}
