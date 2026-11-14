import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"

// CoCo: servers of the webpack.ir account and the purchases in progress. Config hands the server's
// credentials (device key and pinned host key) to the normal install flow.
PageType {
    id: root

    property string passwordServerId: ""

    Component.onCompleted: {
        if (WebpackController.signedIn)
            WebpackController.refreshServers()
    }

    Connections {
        target: WebpackController
        enabled: root.visible
        function onErrorOccurred(message) { PageController.showErrorMessage(message) }
        function onNotificationShown(message) { PageController.showNotificationMessage(message) }
        function onBusyChanged() { PageController.showBusyIndicator(WebpackController.busy) }
        function onPasswordNeeded(serverPublicId, serverName) {
            root.passwordServerId = serverPublicId
            passwordDrawer.serverName = serverName
            passwordDrawer.openTriggered()
        }
        function onConfigReady() {
            ServersUiController.setProcessedServerId("")
            PageController.goToPage(PageEnum.PageSetupWizardEasy)
        }
    }

    BackButtonType {
        id: backButton
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 20 + PageController.safeAreaTopMargin
    }

    FlickableType {
        anchors.top: backButton.bottom
        anchors.bottom: parent.bottom
        contentHeight: content.implicitHeight + 32

        ColumnLayout {
            id: content
            width: parent.width
            spacing: 8

            BaseHeaderType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 8
                headerText: qsTr("Servers bought from webpack")
                descriptionText: qsTr("Pick a server and press Config. CoCo connects with this device's key and sets up the VPN.")
            }

            ParagraphTextType {
                visible: !WebpackController.signedIn
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                text: qsTr("Sign in to webpack first.")
            }

            // Purchases in progress
            Repeater {
                model: WebpackController.purchases

                delegate: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    LabelWithButtonType {
                        Layout.fillWidth: true
                        text: modelData.planName + (modelData.regionName !== "" ? " · " + modelData.regionName : "")
                        descriptionText: modelData.stageText
                        rightImageSource: modelData.dismissable ? "qrc:/images/controls/close.svg" : ""
                        clickedFunction: function() {
                            if (modelData.dismissable)
                                WebpackController.dismissPurchase(modelData.id)
                        }
                    }

                    DividerType {}
                }
            }

            // Account servers
            Repeater {
                model: WebpackController.servers

                delegate: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    LabelWithButtonType {
                        Layout.fillWidth: true
                        text: modelData.name
                        descriptionText: modelData.description
                    }

                    BasicButtonType {
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16
                        Layout.bottomMargin: 8
                        enabled: modelData.ready && !WebpackController.busy
                        text: modelData.ready ? qsTr("Config") : modelData.status
                        clickedFunc: function() { WebpackController.configureServer(modelData.id, "") }
                    }

                    DividerType {}
                }
            }

            ParagraphTextType {
                visible: WebpackController.signedIn && WebpackController.servers.length === 0
                         && WebpackController.purchases.length === 0
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                text: qsTr("No servers on this account yet.")
            }

            BasicButtonType {
                visible: WebpackController.signedIn
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                textColor: AmneziaStyle.color.paleGray
                borderWidth: 1
                text: qsTr("Refresh")
                clickedFunc: function() { WebpackController.refreshServers() }
            }

            BasicButtonType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                textColor: AmneziaStyle.color.paleGray
                borderWidth: 1
                text: qsTr("Buy a server from webpack")
                clickedFunc: function() { PageController.goToPage(PageEnum.PageWebpackShop) }
            }
        }
    }

    // Asked once for a server CoCo did not buy itself (or whose password it no longer has): the
    // login password lets CoCo allow sudo without a password, which the VPN installer needs.
    DrawerType2 {
        id: passwordDrawer

        property string serverName: ""

        parent: root
        anchors.fill: parent
        expandedHeight: root.height * 0.6

        expandedStateContent: ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 12

            Header2Type {
                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                headerText: qsTr("Server password")
                descriptionText: qsTr("Enter the login password of %1 once. CoCo uses it only to allow the VPN setup and does not keep it.").arg(passwordDrawer.serverName)
            }

            TextFieldWithHeaderType {
                id: serverPasswordField
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                headerText: qsTr("Password")
                textField.echoMode: TextInput.Password
            }

            BasicButtonType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 24
                text: qsTr("Continue")
                clickedFunc: function() {
                    var password = serverPasswordField.textField.text
                    serverPasswordField.textField.text = ""
                    passwordDrawer.closeTriggered()
                    if (password !== "")
                        WebpackController.configureServer(root.passwordServerId, password)
                }
            }
        }
    }
}
