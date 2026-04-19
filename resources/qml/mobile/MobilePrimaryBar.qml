import QtQuick 2.15

Rectangle {
    id: root
    color: "#1F2329"
    height: 64

    property string openText: "Open"
    property string saveText: "Save"
    property string playText: "Play"
    property string leftPanelText: "Hide Left"
    property string rightPanelText: "Hide Right"
    property string functionsText: "Functions"
    property string noteText: "Note"
    property string bpmText: "BPM"
    property string metaText: "Meta"
    property string activePanel: "note"

    signal openRequested()
    signal saveRequested()
    signal playRequested()
    signal toggleLeftPanelRequested()
    signal toggleRightPanelRequested()
    signal functionsRequested()
    signal notePanelRequested()
    signal bpmPanelRequested()
    signal metaPanelRequested()

    Rectangle {
        anchors.fill: parent
        anchors.margins: 8
        radius: 10
        color: "#2A3038"
        border.color: "#3D4651"
        border.width: 1

        Flickable {
            id: flick
            anchors.fill: parent
            anchors.margins: 6
            contentWidth: controlsRow.width
            contentHeight: controlsRow.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.HorizontalFlick

            Row {
                id: controlsRow
                spacing: 6
                height: 40

                Repeater {
                    model: [
                        { "text": root.openText, "kind": "normal", "tap": function(){ root.openRequested(); } },
                        { "text": root.saveText, "kind": "normal", "tap": function(){ root.saveRequested(); } },
                        { "text": root.playText, "kind": "normal", "tap": function(){ root.playRequested(); } },
                        { "text": root.leftPanelText, "kind": "normal", "tap": function(){ root.toggleLeftPanelRequested(); } },
                        { "text": root.rightPanelText, "kind": "normal", "tap": function(){ root.toggleRightPanelRequested(); } },
                        { "text": root.functionsText, "kind": "normal", "tap": function(){ root.functionsRequested(); } },
                        { "text": root.noteText, "kind": root.activePanel === "note" ? "active" : "normal", "tap": function(){ root.notePanelRequested(); } },
                        { "text": root.bpmText, "kind": root.activePanel === "bpm" ? "active" : "normal", "tap": function(){ root.bpmPanelRequested(); } },
                        { "text": root.metaText, "kind": root.activePanel === "meta" ? "active" : "normal", "tap": function(){ root.metaPanelRequested(); } }
                    ]

                    delegate: Rectangle {
                        width: Math.max(78, label.implicitWidth + 24)
                        height: 40
                        radius: 8
                        color: modelData.kind === "active" ? "#3C7DFF" : "#394250"
                        border.width: 1
                        border.color: modelData.kind === "active" ? "#6EA0FF" : "#4C5869"

                        Text {
                            id: label
                            anchors.centerIn: parent
                            text: modelData.text
                            color: "#F2F5F8"
                            font.pixelSize: 14
                            font.bold: modelData.kind === "active"
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: modelData.tap()
                        }
                    }
                }
            }
        }
    }
}
