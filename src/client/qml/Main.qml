import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QChat

ApplicationWindow {
    id: window
    width: 880
    height: 720
    minimumWidth: 600
    minimumHeight: 500
    visible: true
    title: "QChat"
    color: "#101722"
    palette.text: "#e6edf5"
    palette.windowText: "#e6edf5"
    palette.base: "#1c2736"
    palette.button: "#263449"
    palette.buttonText: "#e6edf5"
    palette.highlight: "#61cbb5"
    palette.highlightedText: "#101722"

    ChatController {
        id: chat
        onMessageSent: composer.clear()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16
        RowLayout {
            Label { text: "QChat"; font.pixelSize: 30; font.bold: true }
            Item { Layout.fillWidth: true }
            Label {
                text: chat.busy ? "Connecting…" : chat.connected ? "●  Connected" : "○  Offline"
                color: chat.connected ? "#61cbb5" : "#a5b3c7"
            }
        }
        Label { text: "One room. Everyone welcome."; color: "#a5b3c7" }
        RowLayout {
            spacing: 8
            TextField {
                id: address
                text: "127.0.0.1"
                placeholderText: "Server IP"
                Accessible.name: "Server IP address"
                Layout.fillWidth: true
                enabled: !chat.connected && !chat.busy
            }
            TextField {
                id: port
                text: "9090"
                placeholderText: "Port"
                Accessible.name: "Server port"
                Layout.preferredWidth: 80
                validator: IntValidator { bottom: 1; top: 65535 }
                enabled: !chat.connected && !chat.busy
            }
            TextField {
                id: nickname
                placeholderText: "Display name"
                Accessible.name: "Display name"
                Layout.preferredWidth: 150
                maximumLength: 64
                enabled: !chat.connected && !chat.busy
                onAccepted: connectButton.clicked()
            }
            Button {
                id: connectButton
                text: chat.connected ? "Disconnect" : "Join room"
                enabled: !chat.busy && !chat.sending
                onClicked: {
                    if (chat.connected)
                        chat.disconnectFromServer()
                    else
                        chat.connectToServer(address.text.trim(), Number(port.text), nickname.text)
                }
            }
        }
        Label {
            text: chat.error
            color: "#ffb4ab"
            visible: text.length > 0
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Layout.fillWidth: true
            Accessible.role: Accessible.AlertMessage
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#172130"
            radius: 12
            border.color: "#29374b"
            ListView {
                id: feed
                anchors.fill: parent
                anchors.margins: 16
                clip: true
                spacing: 16
                model: chat.messages
                ScrollBar.vertical: ScrollBar {}
                property bool followLatest: true
                onMovementEnded: followLatest = atYEnd
                onCountChanged: {
                    if (followLatest)
                        Qt.callLater(positionViewAtEnd)
                }
                delegate: ColumnLayout {
                    id: bubble
                    required property string displayName
                    required property string body
                    required property string timeLabel
                    width: ListView.view.width - 12
                    spacing: 4
                    RowLayout {
                        Label {
                            text: bubble.displayName
                            textFormat: Text.PlainText
                            font.bold: true
                            color: "#61cbb5"
                        }
                        Label { text: bubble.timeLabel; color: "#91a2b9"; font.pixelSize: 11 }
                    }
                    Label {
                        text: bubble.body
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: feed.count === 0
                    text: chat.connected ? "Say hello to the room." : "Choose a name and join the conversation."
                    color: "#91a2b9"
                }
            }
        }
        RowLayout {
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 90
                TextArea {
                    id: composer
                    placeholderText: "Write a message…  (Ctrl+Enter to send)"
                    Accessible.name: "Message"
                    enabled: chat.connected && !chat.sending
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    Keys.onPressed: event => {
                        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                            && (event.modifiers & Qt.ControlModifier)) {
                            chat.sendMessage(text)
                            event.accepted = true
                        }
                    }
                }
            }
            Button {
                text: chat.sending ? "Sending…" : "Send"
                enabled: chat.connected && !chat.sending && composer.text.trim().length > 0
                onClicked: chat.sendMessage(composer.text)
            }
        }
        Label {
            text: "Public room · No accounts · Last 1,000 messages · History clears on server restart"
            color: "#91a2b9"
            font.pixelSize: 11
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }
}
